module;

#include <string_view>
#include <system_error>
#include <wrl/client.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <vector>

#pragma comment(lib, "Dwrite")

export module UI:Text;

namespace UI
{
	void ThrowIfFailed(HRESULT hr, std::string_view msg)
	{
		if (FAILED(hr))
		{
			std::runtime_error(msg.data());
		}
	}

	export class LSText
	{
	public:
		LSText(std::wstring_view text, 
			IDWriteFactory* pWriteFactory,
			const RECT& bounds = {0, 0, 100, 100},
			std::wstring_view font = L"Verdana", 
			float fontSize = 16.0f,
			DWRITE_FONT_WEIGHT weight = { DWRITE_FONT_WEIGHT_NORMAL },
			DWRITE_FONT_STYLE styles = { DWRITE_FONT_STYLE_NORMAL },
			DWRITE_FONT_STRETCH stretches = { DWRITE_FONT_STRETCH_NORMAL }) : m_text(text.data()),
			m_fontType(font.data()),
			m_fontSize(fontSize),
			m_weight(weight),
			m_style(styles),
			m_stretch(stretches),
			m_boundRect(bounds)
		{
			pWriteFactory->CreateTextFormat(m_fontType.c_str(), NULL, 
				m_weight, m_style, m_stretch, m_fontSize, 
				m_locale.c_str(), 
				m_pTextFormat.ReleaseAndGetAddressOf());
			m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
			m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		}

		void Render(ID2D1RenderTarget* pRenderTarget)
		{
			if (!pRenderTarget)
				return;

			createBrushes(pRenderTarget);
			// Insure we are in identity transform (none) before applying our positional values for the text
			pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
			// Rectangle border of draw bounds
			D2D1_RECT_F rec{};
			rec.left = m_boundRect.left;
			rec.top = m_boundRect.top;
			rec.right = m_boundRect.right;
			rec.bottom = m_boundRect.bottom;
			// Show textbox border and draw text
			pRenderTarget->DrawRectangle(rec, m_pFill.Get());
			pRenderTarget->FillRectangle(rec, m_pBorder.Get());
			pRenderTarget->DrawTextW(m_text.c_str(),
				m_text.size() - 1,
				m_pTextFormat.Get(),
				D2D1::RectF(m_boundRect.left, m_boundRect.top, m_boundRect.right, m_boundRect.bottom),
				m_pStroke.Get()
			);
		}

		void discardGraphicResources()
		{
			m_pStroke = nullptr;
		}

		RECT getBoundaries()
		{
			return m_boundRect;
		}

	private:
		Microsoft::WRL::ComPtr<IDWriteTextFormat> m_pTextFormat = nullptr;
		Microsoft::WRL::ComPtr<ID2D1Brush> m_pStroke = nullptr;
		Microsoft::WRL::ComPtr<ID2D1Brush> m_pBorder = nullptr;
		Microsoft::WRL::ComPtr<ID2D1Brush> m_pFill = nullptr;
		std::wstring m_locale = L"en-us";
		float m_fontSize = 16.0f;
		std::wstring m_text = L"";
		std::wstring m_fontType = L"Verdana";
		DWRITE_FONT_WEIGHT m_weight;
		DWRITE_FONT_STYLE m_style;
		DWRITE_FONT_STRETCH m_stretch;
		RECT m_boundRect = { 0, 0, 100, 100 };

		void createBrushes(ID2D1RenderTarget* pRenderTarget)
		{
			if (!m_pStroke || !m_pBorder || !m_pFill)
			{
				// Default brushes
				Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pTemp;
				Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pTempFill;
				Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pTempBorder;
				auto hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &pTemp);
				hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &pTempFill);
				hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.20f, 0.20f), &pTempBorder);
				ThrowIfFailed(hr, "Failed to create solid defualt brush");
				hr = pTemp.As(&m_pStroke);
				hr = pTempFill.As(&m_pFill);
				hr = pTempBorder.As(&m_pBorder);
				ThrowIfFailed(hr, "Failed to create brush");
			}
		}

	};
}