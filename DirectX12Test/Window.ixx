module;

#include <cstdint>
#include <string_view>
#include <functional>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <WinUser.h>
#include <string>
#include <system_error>
#include <format>
#include <iostream>
#include <array>
#include <wrl/client.h>
#include <dwrite_3.h>
#include <d2d1.h>
#include <vector>
#pragma comment(lib, "d2d1")
#pragma comment(lib, "Dwrite")

#ifndef UNICODE
#define UNICODE
#endif

#if defined CreateWindow
#undef CreateWindow
#endif

export module Window;
import UI;
export import DX12Device;

namespace Application
{
	static LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

	void ThrowIfFailed(HRESULT hr, std::string_view msg)
	{
		if (FAILED(hr))
		{
			std::runtime_error(msg.data());
		}
	}

	export class LSWindow
	{
	public:
		LSWindow() = default;
		LSWindow(LSWindow&) = default;
		LSWindow(LSWindow&&) = default;	

		LSWindow& operator=(const LSWindow& other)
		{
			m_width = other.m_width;
			m_height = other.m_height;
			m_bIsClosing = other.m_bIsClosing;
			m_dpi = other.m_dpi;
			m_hwnd = other.m_hwnd;
			m_hInstance = other.m_hInstance;
			m_msg = other.m_msg;
			m_mousePoint = other.m_mousePoint;
			m_pFactory = other.m_pFactory;
			m_pRenderTarget = other.m_pRenderTarget;
			m_title = other.m_title;
			m_onLMBDown = other.m_onLMBDown;
			m_onLMBUp = other.m_onLMBUp;
			m_onMouseMove = other.m_onMouseMove;
			return *this;
		}
		LSWindow& operator=(LSWindow&& other)
		{
			m_width = other.m_width;
			m_height = other.m_height;
			m_bIsClosing = other.m_bIsClosing;
			m_dpi = other.m_dpi;
			m_hwnd = other.m_hwnd;
			m_hInstance = other.m_hInstance;
			m_msg = other.m_msg;
			m_mousePoint = other.m_mousePoint;
			m_pFactory = other.m_pFactory;
			m_pRenderTarget = other.m_pRenderTarget;
			m_title = std::move(other.m_title);
			m_onLMBDown = other.m_onLMBDown;
			m_onLMBUp = other.m_onLMBUp;
			m_onMouseMove = other.m_onMouseMove;
			return *this;
		}

		~LSWindow()
		{
			m_bIsClosing = true;
			UnregisterClass(TEXT("DX12 Test"), m_hInstance);
		}

		void run()
		{
			while (!m_bIsClosing)
			{
				poll();
			}
		}

		void poll()
		{
			if (PeekMessage(&m_msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&m_msg);
				DispatchMessage(&m_msg);
			}
		}

		void initWindow(uint32_t width, uint32_t height, std::wstring_view title)
		{
			m_width = width;
			m_height = height;
			m_title = title.data();

			WNDCLASSEX wc = {};
			wc.style = CS_HREDRAW | CS_VREDRAW;
			wc.cbSize = sizeof(WNDCLASSEX);
			wc.cbWndExtra = 0;
			wc.cbClsExtra = 0;
			wc.lpfnWndProc = Application::WndProc;
			wc.lpszClassName = TEXT("DX12 Test");
			wc.lpszMenuName = 0;
			wc.hInstance = m_hInstance;
			wc.hCursor = LoadCursor(NULL, IDC_ARROW);
			wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);

			if (!RegisterClassEx(&wc))
			{
				std::runtime_error("Couldn't register the widnow class!");
			}

			m_hwnd = CreateWindowEx(0,
				TEXT("DX12 Test"),
				m_title,
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT,
				m_width, m_height,
				NULL,
				NULL,
				m_hInstance,
				this);

			if (m_hwnd == NULL)
			{
				throw std::runtime_error("Failed to create window!");
			}
			ShowWindow(m_hwnd, SW_SHOW);
		}

		void close()
		{
			m_bIsClosing = true;
		}

		bool isClosing()
		{
			return m_bIsClosing;
		}

		LRESULT handleMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
		{
			switch (message)
			{
			case WM_CREATE:
			{
				createD2D();
				createD3D(hwnd);
				findDPIScale(hwnd);
			}
			break;
			case WM_PAINT:
			{
				onPaint3D();
				onPaint2D();
			}
			break;
			case WM_CLOSE:
				if (MessageBox(hwnd, L"Really quit?", L"My application", MB_OKCANCEL) == IDOK)
				{
					disacardGraphicsResources();
					m_bIsClosing = true;
					DestroyWindow(hwnd);
				}
				break;
			case WM_SIZE:
				resize();
				break;
			case WM_DESTROY:
				PostQuitMessage(0);
				break;
			case WM_SYSKEYDOWN:
				break;
			case WM_SYSCHAR:
				break;
			case WM_SYSKEYUP:
				break;
			case WM_KEYDOWN:
				if (GetAsyncKeyState(VK_ESCAPE) & HIGH_BIT)
				{
					m_bIsClosing = true;
					PostQuitMessage(0);
				}
				if (GetAsyncKeyState(VK_SPACE) & HIGH_BIT)
				{
					m_bIsSkewed = !m_bIsSkewed;
					InvalidateRect(hwnd, NULL, FALSE);
				}
				if (GetAsyncKeyState(VK_LEFT) & HIGH_BIT)
				{
					m_angles -= 1.0f;
					std::cout << "Angles: " << m_angles << "\n";
					InvalidateRect(hwnd, NULL, FALSE);
				}
				if (GetAsyncKeyState(VK_RIGHT) & HIGH_BIT)
				{
					m_angles += 1.0f;
					std::cout << "Angles: " << m_angles << "\n";
					InvalidateRect(hwnd, NULL, FALSE);
				}
				break;
			case WM_KEYUP:
				break;
			case WM_CHAR:
				break;
			case(WM_LBUTTONDOWN):
			{
				onLButtonDown(PixelToDipsX(GET_X_LPARAM(lparam)), PixelToDipsY(GET_Y_LPARAM(lparam)), static_cast<DWORD>(wparam));
			}
			break;
			case(WM_MBUTTONDOWN):
				break;
			case(WM_RBUTTONDOWN):
			{
				break;
			}
			case(WM_LBUTTONUP):
				onLButtonUp(PixelToDipsX(GET_X_LPARAM(lparam)), PixelToDipsY(GET_Y_LPARAM(lparam)), static_cast<DWORD>(wparam));
				break;
			case(WM_MBUTTONUP):
				break;
			case(WM_RBUTTONUP):
			{

			}
			break;
			case(WM_MOUSEMOVE):
			{
				int x = GET_X_LPARAM(lparam);
				int y = GET_Y_LPARAM(lparam);
				onMouseMove(PixelToDipsX(x), PixelToDipsY(y), static_cast<DWORD>(wparam));
				break;
			}
			case(WM_MOUSELEAVE):
			{
				break;
			}
			}

			return DefWindowProc(hwnd, message, wparam, lparam);
		}

		bool getKeyPressAsync(int key)
		{
			return GetAsyncKeyState(key) & HIGH_BIT;
		}

		void redraw(bool clearBackground = false)
		{
			InvalidateRect(m_hwnd, NULL, clearBackground);
		}

		void redraw(RECT rc, bool clearBackground = false)
		{
			InvalidateRect(m_hwnd, &rc, clearBackground);
		}

		void addText(const UI::LSText& text)
		{
			m_texts.emplace_back(text);
		}
		
		void addWidget(UI::Widget* widget)
		{
			if (!widget)
				return;
			m_widgets.emplace_back(widget);
		}

		void onPaint2D()
		{
			auto hr = createGraphicsResources();
			ThrowIfFailed(hr, "Failed to create graphic resources");

			PAINTSTRUCT ps;
			BeginPaint(m_hwnd, &ps);

			m_pRenderTarget->BeginDraw();
			m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));

			for (auto&& text : m_texts)
			{
				text.Render(m_pRenderTarget.Get());
			}

			for (auto&& widget : m_widgets)
			{
				widget->Render(m_pRenderTarget.Get());
			}

			hr = m_pRenderTarget->EndDraw();

			if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
			{
				disacardGraphicsResources();
			}

			EndPaint(m_hwnd, &ps);
		}

		void onPaint3D()
		{
			m_device3d.Render();
		}

		void enableCapture()
		{
			SetCapture(m_hwnd);
		}

		void disableCapture()
		{
			ReleaseCapture();
		}

		// GETTERS // 
		uint32_t Width() const
		{
			return m_width;
		}
		
		uint32_t Height() const
		{
			return m_height;
		}

		std::wstring_view Title() const
		{
			return m_title;
		}

		IDWriteFactory* getWriteFactory()
		{
			return m_pWriteFactory.Get();
		}

		ID2D1RenderTarget* GetRenderTarget()
		{
			return m_pRenderTarget.Get();
		}

		void RegisterLMBDown(std::function<void(float x, float y, DWORD flags)>&& cb)
		{
			m_onLMBDown = cb;
		}
		
		void RegisterLMBUp(std::function<void(float x, float y, DWORD flags)>&& cb)
		{
			m_onLMBUp = cb;
		}
		
		void RegisterMouseMove(std::function<void(float x, float y, DWORD flags)>&& cb)
		{
			m_onMouseMove = cb;
		}

		D2D1_POINT_2F getNormCoords(uint32_t x, uint32_t y)
		{
			return D2D1::Point2F( static_cast<float>(x) / m_width, static_cast<float>(y) / m_height);
		}

		HWND getHandle()
		{
			return m_hwnd;
		}

	private:
		uint32_t	m_width;
		uint32_t	m_height;
		LPCWSTR		m_title;
		HINSTANCE	m_hInstance{};

		//D2D Stuff //
		Microsoft::WRL::ComPtr<ID2D1Factory> m_pFactory = nullptr;
		Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget = nullptr;
		Microsoft::WRL::ComPtr<IDWriteFactory> m_pWriteFactory = nullptr;
		Microsoft::WRL::ComPtr<IDXGIFactory> m_pDxgiFactory = nullptr;
		
		std::function<void(float dipPixelX, float dipPixelY, DWORD flags)> m_onLMBDown;
		std::function<void(float dipPixelX, float dipPixelY, DWORD flags)> m_onLMBUp;
		std::function<void(float dipPixelX, float dipPixelY, DWORD flags)> m_onMouseMove;
		HWND		m_hwnd;
		MSG			m_msg;
		UINT		m_dpi;
		bool		m_bIsClosing = false;

		D2D1_POINT_2F   m_mousePoint{ 0.0f, 0.0f };
		const UINT HIGH_BIT = 0x8000;

		// Manipulation stuff //
		bool		m_bIsSkewed = false;
		float		m_angles = 0.0f;
		std::vector<UI::LSText> m_texts;
		std::vector<UI::Widget*> m_widgets;
		LS::LSDevice m_device3d;
		void createD2D()
		{
			auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_pFactory.ReleaseAndGetAddressOf());
			ThrowIfFailed(hr, "Failed to create D2D Factory");
			hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pWriteFactory), reinterpret_cast<IUnknown**>(m_pWriteFactory.ReleaseAndGetAddressOf()));
			ThrowIfFailed(hr, "Failed to create write factory");
			ThrowIfFailed(hr, "Failed to create text format!");
		}

		void createD3D(HWND hwnd)
		{
			if (!m_device3d.CreateDevice(reinterpret_cast<void*>(hwnd), m_width, m_height))
			{
				throw std::runtime_error("Failed to create the d3d device\n");
			}
		}

		HRESULT createGraphicsResources()
		{
			if (!m_pRenderTarget)
			{
				RECT rc;
				GetClientRect(m_hwnd, &rc);

				auto size = D2D1::SizeU(rc.right, rc.bottom);

				auto hr = m_pFactory->CreateHwndRenderTarget(
					D2D1::RenderTargetProperties(),
					D2D1::HwndRenderTargetProperties(m_hwnd, size),
					&m_pRenderTarget);

				ThrowIfFailed(hr, "Failed to create render target for HWND");
				return hr;
			}
			return S_OK;

		}

		void disacardGraphicsResources()
		{
			m_pRenderTarget = nullptr;
			for (auto& text : m_texts)
			{
				text.discardGraphicResources();
			}
		}

		void resize()
		{
			if (m_pRenderTarget)
			{
				RECT rc;
				GetClientRect(m_hwnd, &rc);

				auto size = D2D1::SizeU(rc.right, rc.bottom);
				m_pRenderTarget->Resize(size);
				InvalidateRect(m_hwnd, NULL, FALSE);
			}
		}

		void findDPIScale(HWND hwnd)
		{
			m_dpi = GetDpiForWindow(hwnd);
		}
		// Converts pixel coordinates to Device Independent Pixel (DIP)
		template <typename T>
		float PixelToDipsX(T x)
		{
			return static_cast<float>(x) / (m_dpi / 96.0f);
		}

		template <typename T>
		float PixelToDipsY(T y)
		{
			return static_cast<float>(y) / (m_dpi / 96.0f);
		}

		void onLButtonDown(float dipPixelX, float dipPixelY, DWORD flags)
		{
			m_mousePoint = { dipPixelX, dipPixelY };

			if (m_onLMBDown)
			{
				m_onLMBDown(dipPixelX, dipPixelY, flags);
			}
		}

		void onMouseMove(float dipPixelX, float dipPixelY, DWORD flags)
		{
			m_mousePoint = { dipPixelX, dipPixelY };

			if (m_onMouseMove)
			{
				m_onMouseMove(dipPixelX, dipPixelY, flags);
			}
		}

		void onLButtonUp(float dipPixelX, float dipPixelY, [[maybe_unused]] DWORD flags)
		{
			m_mousePoint = { dipPixelX, dipPixelY };

			if (m_onLMBUp)
			{
				m_onLMBUp(dipPixelX, dipPixelY, flags);
			}
		}
	};

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
	{
		Application::LSWindow* pWindow = nullptr;
		if (message == WM_NCCREATE)
		{
			CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lparam);
			pWindow = reinterpret_cast<Application::LSWindow*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));

			if (pWindow)
			{
				return pWindow->handleMessage(hwnd, message, wparam, lparam);
			}
			else
			{
				return DefWindowProc(hwnd, message, wparam, lparam);
			}
		}
		else
		{
			pWindow = reinterpret_cast<Application::LSWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (pWindow)
			{
				pWindow->handleMessage(hwnd, message, wparam, lparam);
			}
			else
			{
				return DefWindowProc(hwnd, message, wparam, lparam);
			}
		}
	}
}