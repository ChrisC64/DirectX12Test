module;

#include <string>
#include <cstdint>

export module Object;

export struct Position
{
	float x, y;
};

// All colliders are going to be boxes for now

export struct Box2D
{
	float left, top, right, bottom;
};

export struct AObject
{
	std::wstring m_name;
	uint32_t m_id;
	Position m_position;
	Box2D m_bounds;
};

export bool checkCollision(AObject* obj, Position&& pos)
{
	const auto& x = pos.x;
	const auto& y = pos.y;
	return ((x > obj->m_bounds.left && x < obj->m_bounds.right) &&
		(y > obj->m_bounds.top && y < obj->m_bounds.bottom));
}