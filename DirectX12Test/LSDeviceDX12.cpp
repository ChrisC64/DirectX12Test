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
		std::array<FrameContext, FRAME_COUNT>					m_frameContext = {};
		uint32_t												m_frameIndex;
		// pipeline objects
		ComPtr<ID3D12Device4>									m_pd3dDevice;
		ComPtr<ID3D12DescriptorHeap>							m_pRtvDescHeap;
		ComPtr<ID3D12DescriptorHeap>							m_pSrvDescHeap;
		ComPtr<ID3D12CommandQueue>								m_pCommandQueue;
		ComPtr<IDXGISwapChain4>									m_pSwapChain = nullptr;
		ComPtr<ID3D12GraphicsCommandList>						m_pCommandList;
		HANDLE													m_hSwapChainWaitableObject = nullptr;
		std::array<ComPtr<ID3D12Resource>, FRAME_COUNT>			m_mainRenderTargetResource = {};
		D3D12_CPU_DESCRIPTOR_HANDLE								m_mainRenderTargetDescriptor[FRAME_COUNT] = {};

		// Synchronization Objects
		ComPtr<ID3D12Fence>										m_fence;
		HANDLE													m_fenceEvent = nullptr;
		uint64_t												m_fenceLastSignaledValue = 0;
		UINT													m_rtvDescriptorSize = 0;
	public:

		// Creates the device and pipeline 
		bool CreateDevice(HWND hwnd, uint32_t x, uint32_t y)
		{
			// [DEBUG] Enable debug interface
#ifdef _DEBUG
			ComPtr<ID3D12Debug> pdx12Debug = nullptr;
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
			if (pdx12Debug)
			{
				ComPtr<ID3D12InfoQueue> pInfoQueue = nullptr;
				m_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			}
#endif
			// Create command queue
			{
				D3D12_COMMAND_QUEUE_DESC desc = {};
				desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				desc.NodeMask = 1;

				if (m_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pCommandQueue)) != S_OK)
					return false;
			}
			bool useRect = x == 0 || y == 0;
			long width{}, height{};
			RECT rect;
			if (useRect && GetWindowRect(hwnd, &rect))
			{
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;
			}
			// Setup swap chain
			DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
			swapchainDesc1.BufferCount = FRAME_COUNT;
			swapchainDesc1.Width = useRect ? static_cast<UINT>(width) : x;
			swapchainDesc1.Height = useRect ? static_cast<UINT>(height) : y;
			swapchainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapchainDesc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			swapchainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchainDesc1.SampleDesc.Count = 1;
			swapchainDesc1.SampleDesc.Quality = 0;
			swapchainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapchainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			swapchainDesc1.Scaling = DXGI_SCALING_STRETCH;
			swapchainDesc1.Stereo = FALSE;
			// Since we are using an HWND (Win32) system, we can create the swapchain for HWND 
			{
				ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
				if (factory->CreateSwapChainForHwnd(m_pCommandQueue.Get(), hwnd, &swapchainDesc1, nullptr, nullptr, &swapChain1) != S_OK)
					return false;
				if (swapChain1.As(&m_pSwapChain) != S_OK)
					return false;

				// Helper function that displays our display's resolution and refresh rates and other information 
				LogAdapters(factory.Get());
				// Don't allot ALT+ENTER fullscreen
				factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

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

				if (m_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pRtvDescHeap)) != S_OK)
					return false;
				// Handles have a size that varies by GPU, so we have to ask for the Handle size on the GPU before processing
				m_rtvDescriptorSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}
			// a descriptor heap for the Constant Buffer View/Shader Resource View/Unordered Access View types (this one is just the SRV)
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				desc.NumDescriptors = 1;

				if (m_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pSrvDescHeap)) != S_OK)
					return false;
			}

			CreateRenderTarget();

			return LoadAssets();
		}

		bool LoadAssets()
		{
			// Creating the command list using the command allocator
			// CreateCommandList1 can be used to avoid the unnecessary Create and Closing of the Command List that generally is done the first time we create it. This means
			// we don't need to create a list with an allocator just to close it. 
			if (m_pd3dDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_pCommandList)) != S_OK)
				return false;

			{
				// A fence is used for synchronization
				if (m_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)) != S_OK)
					return false;

				// Update the fence value, from startup, this should be 0, and thus the next frame we'll be creating will be the first frame (back buffer, as 0 is currently in front)
				m_frameContext[FrameIndex()].FenceValue++;

				m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (m_fenceEvent == nullptr)
				{
					ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
				}
			}

			return true;
		}

		constexpr UINT FrameIndex()
		{
			return m_frameIndex == 0 ? 0 : m_frameIndex % FRAME_COUNT;
		}

		// Waits for work on the GPU to finish before moving on to the next frame
		void WaitForGpu()
		{
			FrameContext* frameCon = &m_frameContext[FrameIndex()];

			// Signals the GPU the next upcoming fence value
			ThrowIfFailed(m_pCommandQueue->Signal(m_fence.Get(), frameCon->FenceValue));

			// Wait for the fence to be processes
			ThrowIfFailed(m_fence->SetEventOnCompletion(frameCon->FenceValue, m_fenceEvent));
			WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
			// Increment value to the next frame
			frameCon->FenceValue++;
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
			// The handle can now be used to help use build our RTVs - one RTV per frame/back buffer
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < FRAME_COUNT; i++)
			{
				ThrowIfFailed(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_mainRenderTargetResource[i])));
				m_pd3dDevice->CreateRenderTargetView(m_mainRenderTargetResource[i].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_rtvDescriptorSize);
				m_mainRenderTargetDescriptor[i] = rtvHandle;
				ThrowIfFailed(m_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_frameContext[i].CommandAllocator)));
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
			PopulateCommandList();
			// Execut the command list
			ID3D12CommandList* ppCommandLists[] = { m_pCommandList.Get() };
			m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
			// Present the frame from the swapchain
			ThrowIfFailed(m_pSwapChain->Present(1, 0));
			// Wait for GPU work to finish before proceeding
			MoveToNextFrame();
		}

		void PopulateCommandList()
		{
			FrameContext* frameCon = &m_frameContext[FrameIndex()];
			ThrowIfFailed(frameCon->CommandAllocator->Reset());

			// Taken from ImGui example
			/*D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = m_mainRenderTargetResource[backbufferIndex].Get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;*/

			ThrowIfFailed(m_pCommandList->Reset(frameCon->CommandAllocator, nullptr));

			auto backbufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			//m_pCommandList->ResourceBarrier(1, &barrier);
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_mainRenderTargetResource[backbufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			m_pCommandList->ResourceBarrier(1, &barrier);

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
			m_pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

			const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
			m_pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

			// Indicate that the back buffer will now be used to present.
			auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_mainRenderTargetResource[backbufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			m_pCommandList->ResourceBarrier(1, &barrier2);

			ThrowIfFailed(m_pCommandList->Close());
		}

		void MoveToNextFrame()
		{
			// Get frame context and send to the command queu our fence value 
			auto frameCon = &m_frameContext[m_frameIndex];
			ThrowIfFailed(m_pCommandQueue->Signal(m_fence.Get(), frameCon->FenceValue));

			// Update frame index
			m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

			if (m_fence->GetCompletedValue() < frameCon->FenceValue)
			{
				ThrowIfFailed(m_fence->SetEventOnCompletion(frameCon->FenceValue, m_fenceEvent));
				WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
			}

			m_frameContext[m_frameIndex].FenceValue = frameCon->FenceValue + 1;
		}
	};

	LSDevice::LSDevice() : m_pImpl(std::make_unique<LSDeviceDX12>())
	{
	}

	LSDevice::~LSDevice()
	{

	}

	bool LSDevice::CreateDevice(void* handle, uint32_t x, uint32_t y)
	{
		return m_pImpl->CreateDevice(reinterpret_cast<HWND>(handle), x, y);
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