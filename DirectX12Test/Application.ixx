module;
#include <functional>

export module Application;
export import Window;

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
		App(Application::LSWindow&& window)
		{
			m_window = window;
		}

	private:
		std::function<void()> onRun;
		std::function<void(LS_INPUT key)> onKeyboard;
		LSWindow m_window;
		
	};
}