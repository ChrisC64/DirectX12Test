import DX12Device;
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <ranges>
#include <d3d12.h>
#include "DirectX-Headers/include/directx/d3dx12.h"

inline std::string HrToString(HRESULT hr)
{
	char s_str[64] = {};
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}

namespace LS
{
	struct FrameContext
	{
		ID3D12CommandAllocator* CommandAllocator;
		UINT64                  FenceValue;
	};

	using namespace Microsoft::WRL;
	class LSDeviceDX12
	{
	private:
		static constexpr uint32_t								FRAME_COUNT = 3;
		FrameContext											m_frameContext[FRAME_COUNT] = {};
		uint32_t												m_frameIndex;
		// pipeline objects
		ComPtr<ID3D12Device4>									m_pd3dDevice;
		ComPtr<ID3D12DescriptorHeap>							m_pd3dRtvDescHeap;
		ComPtr<ID3D12DescriptorHeap>							m_pd3dSrvDescHeap;
		ComPtr<ID3D12CommandQueue>								m_pd3dCommandQueue;
		ComPtr<IDXGISwapChain4>									m_pSwapChain = NULL;
		ComPtr<ID3D12GraphicsCommandList>						m_pd3dCommandList;
		HANDLE													m_hSwapChainWaitableObject = NULL;
		std::array<ComPtr<ID3D12Resource>, FRAME_COUNT>			m_mainRenderTargetResource = {};
		D3D12_CPU_DESCRIPTOR_HANDLE								m_mainRenderTargetDescriptor[FRAME_COUNT] = {};

		// Synchronization Objects
		ComPtr<ID3D12Fence>										m_fence;
		HANDLE													m_fenceEvent = NULL;
		uint64_t												m_fenceLastSignaledValue = 0;
		UINT													m_rtvDescriptorSize = 0;
	public:

		// Creates the device and pipeline 
		bool CreateDevice(HWND hwnd)
		{
			// [DEBUG] Enable debug interface
#ifdef _DEBUG
			ID3D12Debug* pdx12Debug = NULL;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
				pdx12Debug->EnableDebugLayer();
#endif
			// Create our DXGI Factory
			ComPtr<IDXGIFactory7> factory;
			CreateDXGIFactory2(0u, IID_PPV_ARGS(&factory));

			// Find the best graphics card (best performing one, with single GPU systems, this should be the default)
			ComPtr<IDXGIAdapter1> hardwareAdapter;
			GetHardwareAdapter(factory.Get(), &hardwareAdapter, false);

			// Create device
			D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
			ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), featureLevel, IID_PPV_ARGS(&m_pd3dDevice)));

			// [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef _DEBUG
			if (pdx12Debug != NULL)
			{
				ID3D12InfoQueue* pInfoQueue = NULL;
				m_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
				pInfoQueue->Release();
				pdx12Debug->Release();
			}
#endif
			// Create command queue
			{
				D3D12_COMMAND_QUEUE_DESC desc = {};
				desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				desc.NodeMask = 1;
				if (m_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pd3dCommandQueue)) != S_OK)
					return false;
			}

			// Setup swap chain
			DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
			swapchainDesc1.BufferCount = FRAME_COUNT;
			swapchainDesc1.Width = 0;
			swapchainDesc1.Height = 0;
			swapchainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchainDesc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			swapchainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchainDesc1.SampleDesc.Count = 1;
			swapchainDesc1.SampleDesc.Quality = 0;
			swapchainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapchainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			swapchainDesc1.Scaling = DXGI_SCALING_STRETCH;
			swapchainDesc1.Stereo = FALSE;

			{
				//IDXGIFactory4* dxgiFactory = NULL;
				ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
				//if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
					//return false;
				if (factory->CreateSwapChainForHwnd(m_pd3dCommandQueue.Get(), hwnd, &swapchainDesc1, nullptr, nullptr, &swapChain1) != S_OK)
					return false;
				if (swapChain1.As(&m_pSwapChain) != S_OK)
					return false;

				LogAdapters(factory.Get());
				// Don't allot ALT+ENTER fullscreen
				factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
				//swapChain1->Release();
				//dxgiFactory->Release();
				m_pSwapChain->SetMaximumFrameLatency(FRAME_COUNT);
				m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
				m_hSwapChainWaitableObject = m_pSwapChain->GetFrameLatencyWaitableObject();
			}

			// Descriptor - a block of data that describes an object to the GPU (SRV, UAVs, CBVs, RTVs, DSVs)
			// Descriptor Heap - A collection of contiguous allocations of descriptors
			// This is the RTV descriptor heap (render target view)
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				desc.NumDescriptors = FRAME_COUNT;
				desc.NodeMask = 1;

				if (m_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pd3dRtvDescHeap)) != S_OK)
					return false;
				// Handles have a size that varies by GPU, so we have to ask for the Handle size on the GPU before processing
				m_rtvDescriptorSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				// The handle can now be used to help use build our RTVs - one RTV per frame/back buffer
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
				for (UINT i = 0; i < FRAME_COUNT; i++)
				{
					m_mainRenderTargetDescriptor[i] = rtvHandle;
					rtvHandle.ptr += m_rtvDescriptorSize;
				}
			}
			// a descriptor heap for the Constant Buffer View/Shader Resource View/Unordered Access View types (this one is just the SRV)
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				desc.NumDescriptors = 1;

				if (m_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pd3dSrvDescHeap)) != S_OK)
					return false;
			}

			// Create Command Allocator for each frame
			for (UINT i = 0; i < FRAME_COUNT; i++)
				if (m_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContext[i].CommandAllocator)) != S_OK)
					return false;

			uint32_t i = 0;
			for (auto m : m_frameContext)
			{
				m.FenceValue = i++;
			}

			CreateRenderTarget();

			return LoadAssets();
		}

		bool LoadAssets()
		{
			// Creating the command list using the command allocator
			// CreateCommandList1 can be used to avoid the unnecessary Create and Closing of the Command List that generally is done the first time we create it. This means
			// we don't need to create a list with an allocator just to close it. 
			if (m_pd3dDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_pd3dCommandList)) != S_OK)
				return false;

			// A fence is used for synchronization
			if (m_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)) != S_OK)
				return false;

			m_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (m_fenceEvent == NULL)
				return false;
		}

		void GetHardwareAdapter(
			IDXGIFactory1* pFactory,
			IDXGIAdapter1** ppAdapter,
			bool requestHighPerformanceAdapter)
		{
			*ppAdapter = nullptr;

			ComPtr<IDXGIAdapter1> adapter;

			ComPtr<IDXGIFactory6> factory6;
			if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
			{
				for (
					UINT adapterIndex = 0;
					SUCCEEDED(factory6->EnumAdapterByGpuPreference(
						adapterIndex,
						requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
						IID_PPV_ARGS(&adapter)));
					++adapterIndex)
				{
					DXGI_ADAPTER_DESC1 desc;
					adapter->GetDesc1(&desc);

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						// Don't select the Basic Render Driver adapter.
						// If you want a software adapter, pass in "/warp" on the command line.
						continue;
					}

					// Check to see whether the adapter supports Direct3D 12, but don't create the
					// actual device yet.
					if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
					{
						break;
					}
				}
			}

			if (adapter.Get() == nullptr)
			{
				for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
				{
					DXGI_ADAPTER_DESC1 desc;
					adapter->GetDesc1(&desc);

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						// Don't select the Basic Render Driver adapter.
						// If you want a software adapter, pass in "/warp" on the command line.
						continue;
					}

					// Check to see whether the adapter supports Direct3D 12, but don't create the
					// actual device yet.
					if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
					{
						break;
					}
				}
			}

			*ppAdapter = adapter.Detach();
		}

		void CreateRenderTarget()
		{
			for (UINT i = 0; i < FRAME_COUNT; i++)
			{
				m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_mainRenderTargetResource[i]));
				m_pd3dDevice->CreateRenderTargetView(m_mainRenderTargetResource[i].Get(), NULL, m_mainRenderTargetDescriptor[i]);
			}
		}

		void CheckFeatures(std::string& s)
		{
			std::cout << "Checking features ... \n";
		}

		void LogAdapters(IDXGIFactory4* factory)
		{
			uint32_t i = 0;
			IDXGIAdapter* adapter = nullptr;
			std::vector<IDXGIAdapter*> adapterList;
			while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
			{
				DXGI_ADAPTER_DESC desc;
				adapter->GetDesc(&desc);
				std::wstring text = L"***Adapter: ";
				text += desc.Description;
				text += L"\n";
				OutputDebugString(text.c_str());

				adapterList.emplace_back(adapter);
				++i;
			}

			for (auto a : adapterList)
			{
				LogAdapterOutput(a);
				a->Release();
			}
		}

		void LogAdapterOutput(IDXGIAdapter* adapter)
		{
			uint32_t i = 0;
			IDXGIOutput* output = nullptr;
			while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
			{
				DXGI_OUTPUT_DESC desc;
				output->GetDesc(&desc);

				std::wstring text = L"***Output: ";
				text += desc.DeviceName;
				text += L"\n";
				OutputDebugString(text.c_str());

				LogOutputDisplayModes(output, DXGI_FORMAT_B8G8R8A8_UNORM);

				output->Release();
				++i;
			}
		}

		void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
		{
			UINT count = 0;
			UINT flags = 0;

			output->GetDisplayModeList(format, flags, &count, nullptr);
			std::vector<DXGI_MODE_DESC> modeList(count);
			output->GetDisplayModeList(format, flags, &count, &modeList[0]);

			for (auto& x : modeList)
			{
				UINT n = x.RefreshRate.Numerator;
				UINT d = x.RefreshRate.Denominator;
				std::wstring text =
					L"Width = " + std::to_wstring(x.Width) + L" " +
					L"Height = " + std::to_wstring(x.Height) + L" " +
					L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) + L"\n";

				::OutputDebugString(text.c_str());
			}
		}

		void Render()
		{
			std::cout << "D3D Render function called!\n";
			// Populate the command list
			// This means record all commands we need to render the scene (clearing for now)
			// Execut the command list
			// Present the frame from the swapchain
			// Wait for GPU work to finish before proceeding
		}
	};

	LSDevice::LSDevice() : m_pImpl(std::make_unique<LSDeviceDX12>())
	{
	}

	LSDevice::~LSDevice()
	{

	}

	bool LSDevice::CreateDevice(void* handle)
	{
		return m_pImpl->CreateDevice(reinterpret_cast<HWND>(handle));
	}

	void LSDevice::CheckFeatures(std::string& s)
	{
		m_pImpl->CheckFeatures(s);
	}
	void LSDevice::CleanupDevice()
	{
	}

	void LSDevice::Render()
	{
		m_pImpl->Render();
	}
}