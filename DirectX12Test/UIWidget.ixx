module;
#include <functional>
#include <d2d1.h>

export module UI:Widget;

export import Object;

namespace UI
{
	export enum class WidgetEvent
	{
		ENTER,
		LEAVE,
		HOVER,
		CLICKED,
		HOLD,
		RELEASED
	};

	export enum class WidgetState
	{
		ACTIVE,
		DISABLED
	};

	export struct WidgetArgs
	{
		Position CursorPos;
	};

	export class Widget : public AObject
	{
	public:
		WidgetState m_State;

		virtual ~Widget() = default;

		virtual void onClick()
		{

		}

		virtual void onClickRelease()
		{

		}

		virtual void onEnter()
		{

		}
		
		virtual void onExit()
		{

		}

		virtual void onEvent([[maybe_unused]] WidgetEvent we, [[maybe_unused]] WidgetArgs wArgs)
		{

		}

		virtual void onMouseMove([[maybe_unused]] float x, [[maybe_unused]] float y)
		{

		}

		virtual void Render([[maybe_unused]] ID2D1RenderTarget* pRenderTarget)
		{

		}
	};
}