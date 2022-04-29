module;
#include <functional>
#include <vector>
#include <memory>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

export module Application;

import Window;
import Shapes;
import QuadTree;

namespace Application
{
	export enum class LS_INPUT
	{
		ESCAPE,
		W,
		S,
		A,
		D
	};

	export class App
	{
	public:
		App(Application::LSWindow&& window) : m_quadTree(Data::Box{ .minPoint = Data::Point{.x = 0.0f, .y = 0.0f},
			.maxPoint = Data::Point{.x = static_cast<float>(window.Width()), .y = static_cast<float>(window.Height())}}, 10)
		{
			using namespace std::placeholders;
			window.RegisterLMBDown([=]([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] DWORD flags)
				{
					OnLMBDown(x, y, flags);
				});

			window.RegisterLMBUp([=]([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] DWORD flags)
				{
					OnLMBUp(x, y, flags);
				});

			window.RegisterMouseMove([=]([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] DWORD flags)
				{
					OnMouseMove(x, y, flags);
				});

			m_window = std::move(window);
		}
		
		App(uint32_t x, uint32_t y, std::wstring_view title) : m_window(), m_quadTree(Data::Box{ .minPoint = Data::Point{.x = 0.0f, .y = 0.0f},
			.maxPoint = Data::Point{.x = static_cast<float>(x), .y = static_cast<float>(y)}}, 10)
		{
			using namespace std::placeholders;
			m_window.initWindow(x, y, title);
			m_window.RegisterLMBDown([=]([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] DWORD flags)
				{
					OnLMBDown(x, y, flags);
				});

			m_window.RegisterLMBUp([=]([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] DWORD flags)
				{
					OnLMBUp(x, y, flags);
				});

			m_window.RegisterMouseMove([=]([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] DWORD flags)
				{
					OnMouseMove(x, y, flags);
				});
		}

		~App()
		{
			Shutdown();
		}

		void CreateShape(Shape::SHAPE_TYPE shape)
		{
			using enum Shape::SHAPE_TYPE;
			switch (shape)
			{
			case CIRCLE:
				//CreateCircle();
				break;
			default:
				break;
			}
		}

		void AddText(UI::LSText&& text)
		{
			m_texts.emplace_back(text);
		}

		void Run()
		{
			while (!m_window.isClosing())
			{
				m_window.onPaint3D();
				m_window.onPaint2D();
				m_window.poll();
			}
			Shutdown();
		}

		void Shutdown()
		{
			m_window.close();
			Cleanup();
		}

	private:
		std::function<void()> onRun;
		std::function<void(LS_INPUT key)> onKeyboard;
		LSWindow m_window;
		std::vector<UI::LSText> m_texts;
		std::vector<UI::Widget*> m_widgets;
		Shape::IShape* pCurrShape;
		Data::Point m_mouseClickDown;
		Data::Point m_mouseClickUp;
		Data::QuadTree<int> m_quadTree;

		void Cleanup()
		{
			for (auto i = 0u; i < m_widgets.size(); i++)
			{
				delete m_widgets[i];
			}
		}

		void CreateCircle()
		{
			const auto dist = Position{ .x = abs(m_mouseClickUp.x) - abs(m_mouseClickDown.x),
			.y = abs(m_mouseClickUp.y) - abs(m_mouseClickDown.y) };
			auto pCircle = new Shape::Circle{ {.x = dist.x, .y = dist.y}, 
				Position{.x = m_mouseClickDown.x, .y = m_mouseClickDown.y}
			};
			static int counter = 0;
			pCurrShape = pCircle;
			m_widgets.emplace_back(pCircle);
			m_window.addWidget(dynamic_cast<UI::Widget*>(pCircle));
			auto node = Data::Node<int>{ 
				.data = std::make_shared<int>(counter++), 
				.region = {}, 
				.position = {.x = static_cast<float>(dist.x),
					.y = static_cast<float>(dist.y) } 
			};
			auto pInt = std::make_shared<Data::Node<int>>(node);
			m_quadTree.Insert(pInt, { static_cast<float>(dist.x), static_cast<float>(dist.y) });
		}

		void OnLMBDown([[maybe_unused]] float dipPixelX, [[maybe_unused]] float dipPixelY, [[maybe_unused]] DWORD flags)
		{
			std::cout << "Application MLB Down callback!\n";
			m_mouseClickDown = Data::Point{ .x = dipPixelX, .y = dipPixelY };
		}

		void OnLMBUp([[maybe_unused]] float dipPixelX, [[maybe_unused]] float dipPixelY, [[maybe_unused]] DWORD flags)
		{
			std::cout << "Application MLB Up callback!\n";
			m_mouseClickUp = Data::Point{ .x = dipPixelX, .y = dipPixelY };
			CreateCircle();
			if (pCurrShape)
			{
				/*m_widgets.emplace_back(pCurrShape);
				m_window.addWidget(dynamic_cast<UI::Widget*>(pCurrShape));*/
				pCurrShape = nullptr;
			}
		}

		void OnMouseMove([[maybe_unused]] float dipPixelX, [[maybe_unused]] float dipPixelY, [[maybe_unused]] DWORD flags)
		{
			std::cout << "Application Mouse Move callback!\n";
			if (pCurrShape)
			{
				auto cast = dynamic_cast<Shape::Circle*>(pCurrShape);
				if (!cast)
					return;
				auto dist = Position{ abs(cast->Radius().x - dipPixelX), abs(cast->Radius().y - dipPixelY)};

				cast->SetLength(dist.x, dist.y);
			}
		}
	};
}