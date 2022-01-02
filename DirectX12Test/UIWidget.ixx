module;
#include <functional>

export module UI:Widget;

export import Object;

namespace UI
{
	export enum class WidgetEvent
	{
		ENTER,
		LEAVE,
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
		Box2D Bounds;
	};

	export class Widget : public AObject
	{
	public:
		WidgetState m_State;

		virtual ~Widget() = default;

		virtual void onClick([[maybe_unused]] float x, [[maybe_unused]] float y)
		{

		}

		virtual void onClickRelease([[maybe_unused]] float x, [[maybe_unused]] float y)
		{

		}

		virtual void onEnter([[maybe_unused]] float x, [[maybe_unused]] float y)
		{

		}
		
		virtual void onExit([[maybe_unused]] float x, [[maybe_unused]] float y)
		{

		}

		virtual void onEvent([[maybe_unused]] WidgetEvent we, [[maybe_unused]] WidgetArgs wArgs)
		{

		}

	};
}