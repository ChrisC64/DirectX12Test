import DX12Device;
import Constants;
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
//#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <array>
#include <vector>
//#include <string>
#include <algorithm>
#include <ranges>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <optional>
#include "DirectX-Headers/include/directx/d3dx12.h"

#pragma comment(lib, "dxguid.lib")

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

inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	uint64_t byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	D3D12_RESOURCE_STATES finalState,
	std::optional<std::wstring_view> defaultName = std::nullopt,
	std::optional<std::wstring_view> uploadName = std::nullopt)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

	// Creat Default Buffer
	auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapDefault,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())
	));

	// Upload type is needed to get the data onto the GPU
	auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapUpload,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())
	));

	// Create subresource data to copy into the default barrier
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = initData;
	subresourceData.RowPitch = byteSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	auto defaultBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1,
		&defaultBarrier);

	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subresourceData);

	auto defaultBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
	cmdList->ResourceBarrier(1,
		&defaultBarrier2);

	if (defaultName)
	{
		defaultBuffer->SetName(defaultName.value().data());
	}

	if (uploadName)
	{
		uploadBuffer->SetName(uploadName.value().data());
	}

	return defaultBuffer;
}

// The difference with this one is we don't start at the common state and transition to copy destination state
// instead we start in the resource state copy destination and transition to final state for our buffer. 
// This seems to work just fine, removing one resource barrier transition, but I still need to learn more about resource barriers
// and states before knowinng what, if any, impacts this has. Test it out!
inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer2(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	uint64_t byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
	D3D12_RESOURCE_STATES finalState)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

	// Creat Default Buffer
	auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapDefault,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())
	));

	// Upload type is needed to get the data onto the GPU
	auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapUpload,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())
	));

	// Create subresource data to copy into the default barrier
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = initData;
	subresourceData.RowPitch = byteSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subresourceData);

	auto defaultBarrier2 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
	cmdList->ResourceBarrier(1,
		&defaultBarrier2);

	return defaultBuffer;
}

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> GenerateTextureData(uint32_t textureWidth, uint32_t textureHeight, uint32_t pixelSize)
{
	const UINT rowPitch = textureWidth * pixelSize;
	const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
	const UINT cellHeight = textureWidth >> 3;    // The height of a cell in the checkerboard texture.
	const UINT textureSize = rowPitch * textureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < textureSize; n += pixelSize)
	{
		UINT x = n % rowPitch;
		UINT y = n / rowPitch;
		UINT i = x / cellPitch;
		UINT j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;        // R
			pData[n + 1] = 0x00;    // G
			pData[n + 2] = 0x00;    // B
			pData[n + 3] = 0xff;    // A
		}
		else
		{
			pData[n] = 0xff;        // R
			pData[n + 1] = 0xff;    // G
			pData[n + 2] = 0xff;    // B
			pData[n + 3] = 0xff;    // A
		}
	}

	return data;
}

void CreateTileSampleTexture(
	ID3D12Device* device,
	Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
	uint32_t textureWidth,
	uint32_t textureHeight,
	uint32_t pixelSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList,
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& srvHeap)
{
	// Describe and create a Texture2D.
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = textureWidth;
	textureDesc.Height = textureHeight;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&heapDefault,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

	CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);
	auto buffer = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	// Create the GPU upload buffer.
	ThrowIfFailed(device->CreateCommittedResource(
		&heapUpload,
		D3D12_HEAP_FLAG_NONE,
		&buffer,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureUploadHeap)));

	// Copy data to the intermediate upload heap and then schedule a copy 
	// from the upload heap to the Texture2D.
	std::vector<UINT8> textureData = GenerateTextureData(textureWidth, textureHeight, pixelSize);

	D3D12_SUBRESOURCE_DATA textureSubresourceData = {};
	textureSubresourceData.pData = &textureData[0];
	textureSubresourceData.RowPitch = textureWidth * pixelSize;
	textureSubresourceData.SlicePitch = textureSubresourceData.RowPitch * textureHeight;

	UpdateSubresources(commandList.Get(), texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureSubresourceData);
	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &transition);

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(texture.Get(), &srvDesc, srvHeap->GetCPUDescriptorHandleForHeapStart());
	texture->SetName(L"texture");
}

namespace LS
{
	using namespace Microsoft::WRL;
	const constinit uint32_t							FRAME_COUNT = 3;
	template <class T, uint32_t Size>
	using ComPtrArray = std::array<ComPtr<T>, Size>;

	struct FrameContext
	{
		ComPtrArray<ID3D12CommandAllocator, Engine::NUM_CONTEXT> CommandAllocator;// Manages a heap for the command lists. This cannot be reset while the CommandList is still in flight on the GPU0
		//ComPtr<ID3D12CommandAllocator> CommandAllocator;// Manages a heap for the command lists. This cannot be reset while the CommandList is still in flight on the GPU0
		ComPtr<ID3D12CommandAllocator> BundleAllocator;// Use with the bundle list, this allocator performs the same operations as a command list, but is associated with the bundle
		ComPtrArray<ID3D12GraphicsCommandList, Engine::NUM_CONTEXT> CommandList;// Sends commands to the GPU - represents this frames commands
		//ComPtr<ID3D12GraphicsCommandList> CommandList;// Sends commands to the GPU - represents this frames commands
		ComPtr<ID3D12GraphicsCommandList> BundleList;// Bundle up calls you would want repeated constantly, like setting up a draw for a vertex buffer. 
		UINT64                         FenceValue;// Singal value between the GPU and CPU to perform synchronization. 
	};

	class LSDeviceDX12
	{
	private:
		std::array<FrameContext, FRAME_COUNT>					m_frameContext = {};
		uint32_t												m_frameIndex;
		float													m_aspectRatio;

		// pipeline objects
		ComPtr<ID3D12Device4>									m_pDevice;
		ComPtr<ID3D12DescriptorHeap>							m_pRtvDescHeap;
		ComPtr<ID3D12DescriptorHeap>							m_pSrvDescHeap;
		ComPtr<ID3D12CommandAllocator>							m_pCommandAllocator; // local command allocator for non-frame resource related jobs.
		ComPtr<ID3D12CommandAllocator>							m_pCommandAllocator2; // local command allocator for non-frame resource related jobs.
		ComPtr<ID3D12CommandQueue>								m_pCommandQueue;
		ComPtr<IDXGISwapChain4>									m_pSwapChain = nullptr;
		//ComPtr<ID3D12GraphicsCommandList>						m_pCommandList; // Records drawing or state chaning calls for execution later by the GPU - Set states, draw calls - think the D3D11::ImmediateContext 
		ComPtr<ID3D12CommandAllocator>							m_pBundleAllocator;
		ComPtr<ID3D12GraphicsCommandList>						m_pBundleList;
		ComPtr<ID3D12RootSignature>								m_pRootSignature; // Used with shaders to determine input and variables
		ComPtr<ID3D12RootSignature>								m_pRootSignature2; // Used with shaders to determine input and variables - texture_effect.hlsl
		ComPtr<ID3D12PipelineState>								m_pPipelineState; // Defines our pipeline's state - primitive topology, render targets, shaders, etc. 
		ComPtr<ID3D12PipelineState>								m_pPipelineStatePT; // Defines our pipeline's state - primitive topology, render targets, shaders, etc. 
		HANDLE													m_hSwapChainWaitableObject = nullptr;
		std::array<ComPtr<ID3D12Resource>, FRAME_COUNT>			m_mainRenderTargetResource = {};// Our Render Target resources
		D3D12_CPU_DESCRIPTOR_HANDLE								m_mainRenderTargetDescriptor[FRAME_COUNT] = {};
		CD3DX12_VIEWPORT										m_viewport;
		CD3DX12_RECT											m_scissorRect;

		// App resources
		ComPtr<ID3D12Resource>									m_vertexBuffer = nullptr;
		ComPtr<ID3D12Resource>									m_vertexBufferPT = nullptr;
		//ComPtr<ID3D12Resource>									m_uploadBuffer = nullptr;
		ComPtr<ID3D12Resource>									m_texture = nullptr;
		D3D12_VERTEX_BUFFER_VIEW								m_vertexBufferView;
		D3D12_VERTEX_BUFFER_VIEW								m_vertexBufferViewPT;
		// Synchronization Objects
		ComPtr<ID3D12Fence>										m_fence;// Helps us sync between the GPU and CPU
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
			ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), featureLevel, IID_PPV_ARGS(&m_pDevice)));

			// [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef _DEBUG
			if (pdx12Debug)
			{
				ComPtr<ID3D12InfoQueue> pInfoQueue = nullptr;
				m_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			}
#endif
			// Create command queue - This is a FIFO structure used to send commands to the GPU
			{
				D3D12_COMMAND_QUEUE_DESC desc = {};
				desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				desc.NodeMask = 1;

				if (m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pCommandQueue)) != S_OK)
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

			m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(swapchainDesc1.Width), static_cast<float>(swapchainDesc1.Height));
			// Scissor Rect is the actual drawing area of what will be rendered. A viewport can be bigger than the scissor rect,
			// or you can use Scissor rects to specify specific regions to draw (like omitting UI areas that may never be drawn because 2D render systems would handle that)
			m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(swapchainDesc1.Width), static_cast<LONG>(swapchainDesc1.Height));
			m_aspectRatio = m_viewport.Width / m_viewport.Height;
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
				//desc.NodeMask = 1;

				if (m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pRtvDescHeap)) != S_OK)
					return false;
				// Handles have a size that varies by GPU, so we have to ask for the Handle size on the GPU before processing
				m_rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}

			// a descriptor heap for the Constant Buffer View/Shader Resource View/Unordered Access View types (this one is just the SRV)
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				desc.NumDescriptors = 1;

				if (m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pSrvDescHeap)) != S_OK)
					return false;
			}

			CreateRenderTarget();

			bool isSuccess = LoadAssets();
			if (isSuccess)
			{
				LoadVertexDataToGpu();
			}

			return isSuccess;
		}

		bool LoadAssets()
		{
			// Create an empty root signature for shader.hlsl
			{
				CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
				rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
				ThrowIfFailed(m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature)));
			}

			// Create a root signature for our texture_effect.hlsl
			// Create the root signature.
			{
				D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

				// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

				if (FAILED(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
				{
					featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
				}

				CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
				ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

				CD3DX12_ROOT_PARAMETER1 rootParameters[1];
				rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

				D3D12_STATIC_SAMPLER_DESC sampler = {};
				sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				sampler.MipLODBias = 0;
				sampler.MaxAnisotropy = 0;
				sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
				sampler.MinLOD = 0.0f;
				sampler.MaxLOD = D3D12_FLOAT32_MAX;
				sampler.ShaderRegister = 0;
				sampler.RegisterSpace = 0;
				sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
				rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
				ThrowIfFailed(m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature2)));
			}
			// Create pipeline states and associate to command allocators since we have an array of them
			for (auto& fc : m_frameContext)
			{
				for (auto i = 0u; i < Engine::NUM_CONTEXT; ++i)
				{
					ThrowIfFailed(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&fc.CommandAllocator[i])));
					ThrowIfFailed(m_pDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&fc.CommandList[i])));
					fc.CommandAllocator[i]->SetName(L"FC Command Allocator " + i);
					fc.CommandList[i]->SetName(L"FC Command List " + i);
				}
			}
			
			/*for (int i = 0; i < m_frameContext.size(); i++)
			{
				m_frameContext[i].CommandAllocator->SetName(L"Command Allocator" + i);
			}*/

			//m_pCommandList->SetName(L"Command List");

			// Create the pipeline state, which includes compiling and loading shaders.
			{
				ComPtr<ID3DBlob> vertexShader;
				ComPtr<ID3DBlob> pixelShader;
				ComPtr<ID3DBlob> vertexShader2;
				ComPtr<ID3DBlob> pixelShader2;

#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT compileFlags = 0;
#endif

				ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
				ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
				// Define the vertex input layout.
				D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				//TODO: Create another PSO that uses the root signature and input element descriptions for our second textured triangle
				// then see if we can display both on the scene at once. 
				// Describe and create the graphics pipeline state object (PSO).
				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
				psoDesc.pRootSignature = m_pRootSignature.Get();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState.DepthEnable = FALSE;
				psoDesc.DepthStencilState.StencilEnable = FALSE;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.SampleDesc.Count = 1;
				ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
				// Bundle Test // 
				{
					ThrowIfFailed(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_pBundleAllocator)));
					ThrowIfFailed(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_pBundleAllocator.Get(), m_pPipelineState.Get(), IID_PPV_ARGS(&m_pBundleList)));
				}

				// Create root signature for texture_effect.hlsl
				ThrowIfFailed(D3DCompileFromFile(L"texture_effect.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader2, nullptr));
				ThrowIfFailed(D3DCompileFromFile(L"texture_effect.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader2, nullptr));
				D3D12_INPUT_ELEMENT_DESC inputElementDescs2[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				psoDesc.InputLayout = { inputElementDescs2, _countof(inputElementDescs2) };
				psoDesc.pRootSignature = m_pRootSignature2.Get();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader2.Get());
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader2.Get());
				ThrowIfFailed(m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineStatePT)));
			}

			/*ThrowIfFailed(m_pDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_pCommandList)));*/
			{
				// A fence is used for synchronization
				if (m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)) != S_OK)
					return false;

				// Update the fence value, from startup, this should be 0, and thus the next frame we'll be creating will be the first frame (back buffer, as 0 is currently in front)
				m_frameContext[m_frameIndex].FenceValue++;

				m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (m_fenceEvent == nullptr)
				{
					ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
				}
			}

			return true;
		}

		//void LoadVertexDataToGpu()
		//{
		//	FrameContext* frameCon = &m_frameContext[FrameIndex()];
		//	// Reclaims the memory allocated by this allocator for our next usage
		//	ThrowIfFailed(frameCon->CommandAllocator->Reset());

		//	// Resets a command list to its initial state 
		//	ThrowIfFailed(m_pCommandList->Reset(frameCon->CommandAllocator.Get(), m_pPipelineState.Get()));

		//	// Create the vertex buffer.
		//	{
		//		// Define the geometry for a triangle.
		//		Vertex triangleVertices[] =
		//		{
		//			{ { -1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		//			{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		//			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		//		};
		//		const UINT vertexBufferSize = sizeof(triangleVertices);

		//		// We create a default and upload buffer. Using the upload buffer, we transfer the data from the CPU to the GPU (hence the name) but we do not use the buffer as reference.
		//		// We copy the data from our upload buffer to the default buffer, and the only differenc between the two is the staging - Upload vs Default.
		//		// Default types are best for static data that isn't changing.
		//		ComPtr<ID3D12Resource> uploadBuffer;
		//		m_vertexBuffer = CreateDefaultBuffer(m_pDevice.Get(), m_pCommandList.Get(), triangleVertices, sizeof(Vertex) * 3, uploadBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, L"default vb");

		//		// We must wait and insure the data has been copied before moving on 
		//		// After we execute the command list, we need to sync with the GPU and wait to create our buffer view
		//		m_pCommandList->Close();
		//		ExecuteCommandList();
		//		WaitForGpu();

		//		// Initialize the vertex buffer view.
		//		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		//		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		//		m_vertexBufferView.SizeInBytes = vertexBufferSize;

		//		ThrowIfFailed(frameCon->CommandAllocator->Reset());

		//		// Resets a command list to its initial state 
		//		ThrowIfFailed(m_pCommandList->Reset(frameCon->CommandAllocator.Get(), m_pPipelineStatePT.Get()));

		//		// Textured Triangle
		//		VertexPT triangleVerticesPT[] =
		//		{
		//			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
		//			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
		//			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
		//		};
		//		const UINT vertexBufferSize2 = sizeof(triangleVerticesPT);

		//		ComPtr<ID3D12Resource> textureUploadBuffer;
		//		m_vertexBufferPT = CreateDefaultBuffer(m_pDevice.Get(), m_pCommandList.Get(), triangleVerticesPT, sizeof(VertexPT) * 3, textureUploadBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, L"pt default vb");
		//		ComPtr<ID3D12Resource> textureUploadHeap;
		//		{
		//			CreateTileSampleTexture(m_pDevice.Get(), m_texture, 256u, 256u, 4u, textureUploadHeap, m_pCommandList, m_pSrvDescHeap);
		//		}
		//		m_pCommandList->Close();
		//		ExecuteCommandList();
		//		WaitForGpu();

		//		// Initialize the vertex buffer view.
		//		m_vertexBufferViewPT.BufferLocation = m_vertexBufferPT->GetGPUVirtualAddress();
		//		m_vertexBufferViewPT.StrideInBytes = sizeof(VertexPT);
		//		m_vertexBufferViewPT.SizeInBytes = vertexBufferSize2;
		//		// Bundle Test - The vertex buffer isn't iniitialized until here, and we are still in recording state from LoadAssets() call
		//		// So now we can just fulfill our commands and close it. 
		//		{
		//			m_pBundleList->SetGraphicsRootSignature(m_pRootSignature.Get());
		//			m_pBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//			m_pBundleList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		//			m_pBundleList->DrawInstanced(3, 1, 0, 0);
		//			ThrowIfFailed(m_pBundleList->Close());
		//		}
		//	}
		//}

		// Loads Data to GPU using temporary command list. 
		void LoadVertexDataToGpu()
		{
			ComPtr<ID3D12GraphicsCommandList> commandList;
			ThrowIfFailed(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(), m_pPipelineState.Get(), IID_PPV_ARGS(&commandList)));
			
			ComPtr<ID3D12GraphicsCommandList> commandList2;
			ThrowIfFailed(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator2.Get(), m_pPipelineStatePT.Get(), IID_PPV_ARGS(&commandList2)));

			// Create the vertex buffer.
			{
				// Define the geometry for a triangle.
				Vertex triangleVertices[] =
				{
					{ { -1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
					{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
					{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
				};
				const UINT vertexBufferSize = sizeof(triangleVertices);

				// We create a default and upload buffer. Using the upload buffer, we transfer the data from the CPU to the GPU (hence the name) but we do not use the buffer as reference.
				// We copy the data from our upload buffer to the default buffer, and the only differenc between the two is the staging - Upload vs Default.
				// Default types are best for static data that isn't changing.
				ComPtr<ID3D12Resource> uploadBuffer;
				//m_vertexBuffer = CreateDefaultBuffer(m_pDevice.Get(), commandList.Get(), triangleVertices, sizeof(Vertex) * 3, uploadBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, L"default vb");
				m_vertexBuffer = CreateDefaultBuffer(m_pDevice.Get(), commandList.Get(), triangleVertices, sizeof(Vertex) * 3, uploadBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"default vb");

				// We must wait and insure the data has been copied before moving on 
				// After we execute the command list, we need to sync with the GPU and wait to create our buffer view
				ThrowIfFailed(commandList->Close());
				
				//WaitForGpu();

				// Initialize the vertex buffer view.
				m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
				m_vertexBufferView.StrideInBytes = sizeof(Vertex);
				m_vertexBufferView.SizeInBytes = vertexBufferSize;

				//ThrowIfFailed(m_pCommandAllocator->Reset());

				//// Resets a command list to its initial state 
				//ThrowIfFailed(commandList->Reset(m_pCommandAllocator.Get(), m_pPipelineStatePT.Get()));

				// Textured Triangle
				VertexPT triangleVerticesPT[] =
				{
					{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
					{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
					{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
				};
				const UINT vertexBufferSize2 = sizeof(triangleVerticesPT);

				ComPtr<ID3D12Resource> textureUploadBuffer;
				m_vertexBufferPT = CreateDefaultBuffer(m_pDevice.Get(), commandList2.Get(), triangleVerticesPT, sizeof(VertexPT) * 3, textureUploadBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, L"pt default vb");
				ComPtr<ID3D12Resource> textureUploadHeap;
				{
					CreateTileSampleTexture(m_pDevice.Get(), m_texture, 256u, 256u, 4u, textureUploadHeap, commandList2, m_pSrvDescHeap);
				}
				commandList2->Close();
				ID3D12CommandList* ppCommandList[] = { commandList.Get(), commandList2.Get()};
				m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);
				//ExecuteCommandList();
				//WaitForGpu();

				// Initialize the vertex buffer view.
				m_vertexBufferViewPT.BufferLocation = m_vertexBufferPT->GetGPUVirtualAddress();
				m_vertexBufferViewPT.StrideInBytes = sizeof(VertexPT);
				m_vertexBufferViewPT.SizeInBytes = vertexBufferSize2;
				// Bundle Test - The vertex buffer isn't iniitialized until here, and we are still in recording state from LoadAssets() call
				// So now we can just fulfill our commands and close it. 
				{
					m_pBundleList->SetGraphicsRootSignature(m_pRootSignature.Get());
					m_pBundleList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					m_pBundleList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
					m_pBundleList->DrawInstanced(3, 1, 0, 0);
					ThrowIfFailed(m_pBundleList->Close());
				}
				// Wait for GPU work to finish
				{
					ThrowIfFailed(m_pDevice->CreateFence(m_fenceLastSignaledValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
					++m_fenceLastSignaledValue;

					m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);

					if (m_fenceEvent == nullptr)
					{
						ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
					}

					const auto waitForValue = m_fenceLastSignaledValue;
					ThrowIfFailed(m_pCommandQueue->Signal(m_fence.Get(), waitForValue));
					++m_fenceLastSignaledValue;

					ThrowIfFailed(m_fence->SetEventOnCompletion(waitForValue, m_fenceEvent));
					WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

				}
			}
		}

		/*constexpr UINT FrameIndex()
		{
			return m_frameIndex == 0 ? 0 : m_frameIndex % FRAME_COUNT;
		}*/

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
				m_pDevice->CreateRenderTargetView(m_mainRenderTargetResource[i].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, m_rtvDescriptorSize);
				m_mainRenderTargetDescriptor[i] = rtvHandle;
			}
			ThrowIfFailed(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator)));
			m_pCommandAllocator->SetName(L"Data Command Allocator");
			ThrowIfFailed(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator2)));
			m_pCommandAllocator2->SetName(L"Data Command Allocator 2");
		}

		void CheckFeatures([[maybe_unused]] std::string& s)
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
#ifdef _DEBUG
				OutputDebugString(text.c_str());
#else
				std::wcout << text << L"\n";
#endif
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
#ifdef _DEBUG
				OutputDebugString(text.c_str());
#else
				std::wcout << text << L"\n";
#endif
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
#ifdef _DEBUG
				OutputDebugString(text.c_str());
#else
				std::wcout << text << L"\n";
#endif
			}
		}

		void ExecuteCommandList(FrameContext* frameCon)
		{
			// Execut the command list
			/*ID3D12CommandList* ppCommandLists[] = { m_pCommandList.Get() };
			m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);*/
			ID3D12CommandList* ppCommandLists[] = { frameCon->CommandList[m_frameIndex].Get()};
			m_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		}

		// Waits for work on the GPU to finish before moving on to the next frame
		void WaitForGpu()
		{
			FrameContext* frameCon = &m_frameContext[m_frameIndex];

			// Signals the GPU the next upcoming fence value
			ThrowIfFailed(m_pCommandQueue->Signal(m_fence.Get(), frameCon->FenceValue));

			// Wait for the fence to be processes
			ThrowIfFailed(m_fence->SetEventOnCompletion(frameCon->FenceValue, m_fenceEvent));
			WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
			// Increment value to the next frame
			frameCon->FenceValue++;
		}

		void Render(const ColorRGBA& clearColor)
		{
			// Reset command allocator to claim memory used by it
			// Then reset the command list to its default state
			// Perform commands for the new state (just clear screen in this example)
			// Uses a resource barrier to manage transition of resource (Render Target) from one state to another
			// Close command list to execute the command

			auto frameCon = BeginRender();
			// Basic setup for drawing - Reset command list, set viewport to draw to, and clear the frame buffer
			ResetCommandList(frameCon, m_pPipelineState);
			SetViewport(frameCon);
			ClearRTV(clearColor, frameCon);
			// Draws the gradient triangle
			SetPipelineState(m_pPipelineState, frameCon);
			frameCon->CommandList[m_frameIndex]->ExecuteBundle(m_pBundleList.Get());

			/*SetRootSignature(m_pRootSignature);
			Draw(m_vertexBufferView, 3);*/
			// set the state of the pipeline for the textured triangle
			SetPipelineState(m_pPipelineStatePT, frameCon);
			SetRootSignature(m_pRootSignature2, frameCon);
			SetDescriptorHeaps(frameCon);
			Draw(m_vertexBufferViewPT, 3, frameCon);
			// Prepare to render to the render target
			PresentRTV(frameCon);
			CloseCommandList(frameCon);
			// Throw command list onto the command queue and prepare to send it off
			ExecuteCommandList(frameCon);
			ThrowIfFailed(m_pSwapChain->Present(1, 0));
			// Wait for next frame
			MoveToNextFrame();
		}



		FrameContext* BeginRender()
		{
			FrameContext* frameCon = &m_frameContext[m_frameIndex];
			// Reclaims the memory allocated by this allocator for our next usage
			ThrowIfFailed(frameCon->CommandAllocator[m_frameIndex]->Reset());
			return frameCon;
		}

		void ResetCommandList(FrameContext* frameCon, ComPtr<ID3D12PipelineState>& pipelineState)
		{
			// Resets a command list to its initial state 
			//ThrowIfFailed(m_pCommandList->Reset(frameCon->CommandAllocator.Get(), pipelineState.Get()));
			//ThrowIfFailed(m_pCommandList->Reset(frameCon->CommandAllocator[m_frameIndex].Get(), nullptr));
			ThrowIfFailed(frameCon->CommandList[m_frameIndex]->Reset(frameCon->CommandAllocator[m_frameIndex].Get(), nullptr));
		}

		void SetPipelineState(ComPtr<ID3D12PipelineState>& pipelineState, FrameContext* frameCon)
		{
			frameCon->CommandList[m_frameIndex]->SetPipelineState(pipelineState.Get());
		}

		void SetRootSignature(ComPtr<ID3D12RootSignature>& rootSignature, FrameContext* frameCon)
		{
			frameCon->CommandList[m_frameIndex]->SetGraphicsRootSignature(rootSignature.Get());
		}

		void SetDescriptorHeaps(FrameContext* frameCon)
		{
			ID3D12DescriptorHeap* ppHeaps[] = { m_pSrvDescHeap.Get() };
			frameCon->CommandList[m_frameIndex]->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			frameCon->CommandList[m_frameIndex]->SetGraphicsRootDescriptorTable(0, m_pSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
		}

		void SetViewport(FrameContext* frameCon)
		{
			// Set Viewport
			frameCon->CommandList[m_frameIndex]->RSSetViewports(1, &m_viewport);
			frameCon->CommandList[m_frameIndex]->RSSetScissorRects(1, &m_scissorRect);
		}

		void ClearRTV(const ColorRGBA& clearColor, FrameContext* frameCon)
		{
			// This will prep the back buffer as our render target and prepare it for transition
			auto backbufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_mainRenderTargetResource[backbufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			frameCon->CommandList[m_frameIndex]->ResourceBarrier(1, &barrier);

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
			frameCon->CommandList[m_frameIndex]->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
			// Similar to D3D11 - this is our command for drawing. For now, testing triangle drawing through MSDN example code
			const float color[] = { clearColor.r, clearColor.g, clearColor.b, clearColor.a };
			frameCon->CommandList[m_frameIndex]->ClearRenderTargetView(rtvHandle, color, 0, nullptr);

		}

		void SetRTV(FrameContext* frameCon)
		{
			// This will prep the back buffer as our render target and prepare it for transition
			auto backbufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_mainRenderTargetResource[backbufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			frameCon->CommandList[m_frameIndex]->ResourceBarrier(1, &barrier);

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_pRtvDescHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
			frameCon->CommandList[m_frameIndex]->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		}

		void Draw(D3D12_VERTEX_BUFFER_VIEW& bufferView, uint64_t vertices, FrameContext* frameCon, std::optional<uint64_t> instances = 1u)
		{
			frameCon->CommandList[m_frameIndex]->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			frameCon->CommandList[m_frameIndex]->IASetVertexBuffers(0, 1, &bufferView);
			frameCon->CommandList[m_frameIndex]->DrawInstanced(vertices, instances.value(), 0, 0);
		}

		void PresentRTV(FrameContext* frameCon)
		{
			auto backbufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();

			// Indicate that the back buffer will now be used to present.
			auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(m_mainRenderTargetResource[backbufferIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			frameCon->CommandList[m_frameIndex]->ResourceBarrier(1, &barrier2);
		}

		void CloseCommandList(FrameContext* frameCon)
		{
			frameCon->CommandList[m_frameIndex]->Close();
		}

		void MoveToNextFrame()
		{
			// Get frame context and send to the command queu our fence value 
			auto frameCon = &m_frameContext[m_frameIndex];
			ThrowIfFailed(m_pCommandQueue->Signal(m_fence.Get(), frameCon->FenceValue));

			// Update frame index
			//m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			m_frameIndex = (m_frameIndex + 1) % Engine::FRAME_COUNT;

			if (m_fence->GetCompletedValue() < frameCon->FenceValue)
			{
				ThrowIfFailed(m_fence->SetEventOnCompletion(frameCon->FenceValue, m_fenceEvent));
				WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
			}

			m_frameContext[m_frameIndex].FenceValue = frameCon->FenceValue + 1;
		}

		void OnDestroy()
		{
			WaitForGpu();

			CloseHandle(m_fenceEvent);
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
		m_pImpl->OnDestroy();
	}

	void LSDevice::Render(const ColorRGBA& clearColor)
	{
		m_pImpl->Render(clearColor);
	}
}