module;
#include <cstdint>
#include <cmath>
#include <d2d1.h>
#include <wrl/client.h>

export module Shapes;
export import UI;

namespace Shape
{
	export enum class SHAPE_TYPE
	{
		CIRCLE
	};

	constexpr double PI = 3.141592653589793238462643;

	export class IShape : public UI::Widget
	{
	public:
		virtual ~IShape() = default;

		virtual double Area() const = 0;
		virtual void Draw(ID2D1RenderTarget* pRenderTarget) = 0;

		virtual double Height() const
		{
			return m_height;
		}

		virtual double Width() const
		{
			return m_width;
		}
		
		virtual uint32_t Layer() const
		{
			return m_layer;
		}

	private:
		uint32_t m_layer = 0;
		double m_height;
		double m_width;
	};

	export class Circle : public IShape
	{
	public:
		Circle(Position&& length, Position&& centerPoint = { 0.0f, 0.0f }) : m_radiusPos(centerPoint),
			m_lengthPoints(length)
		{
			m_ellipse = D2D1::Ellipse(
				D2D1::Point2F(centerPoint.x, centerPoint.y), 
				static_cast<float>(length.x), 
				static_cast<float>(length.y) );
		}

		// Inherited via IShape
		virtual double Area() const override 
		{
			return 0.0;
		}

		void Render([[maybe_unused]] ID2D1RenderTarget* pRenderTarget) final
		{
			Draw(pRenderTarget);
		}

		virtual void Draw(ID2D1RenderTarget* pRenderTarget) override
		{
			if (!pRenderTarget)
				return;

			if (!m_pStrokeBrush || !m_pFillBrush)
			{
				CreateBrushes(pRenderTarget);
			}

			pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

			pRenderTarget->DrawEllipse(m_ellipse, m_pStrokeBrush.Get());
			pRenderTarget->FillEllipse(m_ellipse, m_pFillBrush.Get());

		}

		void SetRadius(Position radiusPos)
		{
			m_radiusPos = radiusPos;
			UpdateEllipseStruct();
		}

		const Position Radius() const
		{
			return m_radiusPos;
		}

		void SetLength(float x, float y)
		{
			m_lengthPoints = { x, y };
			//CalcRadius( { abs(m_radiusPos.x - length), abs(m_radiusPos.y - length) } );
			//CalcArea();
			UpdateEllipseStruct();
		}

		/*double Diameter() const
		{
			return m_radiusLength * 2;
		}*/
	private:
		Position m_radiusPos;
		Position m_lengthPoints;
		//float m_radiusLength;
		//float m_area;
		
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_pFillBrush;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_pStrokeBrush;
		D2D1_ELLIPSE m_ellipse;

		void CalcRadius(Position endPoint)
		{
			/*const auto xSquared = (endPoint.x - m_radiusPos.x) * (endPoint.x - m_radiusPos.x);
			const auto ySquared = (endPoint.y - m_radiusPos.y) * (endPoint.y - m_radiusPos.y);
			m_radiusLength = sqrt(xSquared + ySquared);*/
		}

		void CalcArea()
		{
			//m_area = PI * (m_radiusLength * m_radiusLength);
		}

		void UpdateEllipseStruct()
		{
			m_ellipse = D2D1::Ellipse(D2D1::Point2F(m_radiusPos.x, m_radiusPos.y), 
				static_cast<float>(m_lengthPoints.x),
				static_cast<float>(m_lengthPoints.y) );
		}

		void CreateBrushes(ID2D1RenderTarget* pRenderTarget)
		{
			if (!m_pFillBrush)
			{
				pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::BlueViolet), 
					m_pFillBrush.ReleaseAndGetAddressOf());
			}
			
			if (!m_pStrokeBrush)
			{
				pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gold), 
					m_pStrokeBrush.ReleaseAndGetAddressOf());
			}
		}
	};
}