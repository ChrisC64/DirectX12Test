module;
#include <cstdint>
#include <vector>
#include <memory>
export module QuadTree;

namespace Data
{
	export struct Point
	{
		float x, y;
	};

	export struct Box
	{
		Point minPoint;
		Point maxPoint;

		bool InBounds(const Point& p)
		{
			return p.x >= minPoint.x && p.y >= minPoint.y
				&& p.x <= maxPoint.x && p.y <= maxPoint.y;
		}
	};

	export template <typename T>
		struct Node
	{
		std::shared_ptr<T> data;
		Box region;
		Point position;
	};

	export
	template <class T>
		struct QuadTree
	{
	private:
		Box region;
		uint32_t capacity;
		std::vector<std::shared_ptr<Node<T>>> nodes;

		std::unique_ptr<QuadTree<T>> topLeft;
		std::unique_ptr<QuadTree<T>> topRight;
		std::unique_ptr<QuadTree<T>> bottomLeft;
		std::unique_ptr<QuadTree<T>> bottomRight;

	public:
		QuadTree(Box r, uint32_t cap) : region(r),
			capacity(cap),
			nodes(),
			topLeft(nullptr),
			topRight(nullptr),
			bottomLeft(nullptr),
			bottomRight(nullptr)
		{
		}

		void subdivide()
		{
			const auto& minPoint = region.minPoint;
			const auto& maxPoint = region.maxPoint;
			const auto dist = Point{ .x = abs(maxPoint.x) - abs(minPoint.x), .y = abs(maxPoint.y) - abs(minPoint.y) };
			const auto halfPoint = Point{ .x = minPoint.x + dist.x * 0.5f, .y = minPoint.y + dist.y * 0.5f};

			// min = {min, hp}, max = {hp, max}
			topLeft = std::make_unique<QuadTree<T>>(Data::Box{ .minPoint = {minPoint.x, halfPoint.y},
				.maxPoint = {halfPoint.y, maxPoint.y} }, capacity);
			// min = hp, hp, max = max, max
			topRight = std::make_unique<QuadTree<T>>(Data::Box{ .minPoint = {halfPoint.x, halfPoint.y},
				.maxPoint = {maxPoint.x, maxPoint.x} }, capacity);
			// min = min, min, max = hp, hp
			bottomLeft = std::make_unique<QuadTree<T>>(Data::Box{ .minPoint = {minPoint.x, minPoint.y},
				.maxPoint = {halfPoint.y, halfPoint.y} }, capacity);
			// min = hp, min, max = max, hp
			bottomRight = std::make_unique<QuadTree<T>>(Data::Box{ .minPoint = {minPoint.x, halfPoint.y},
				.maxPoint = {maxPoint.y, halfPoint.y} }, capacity);
		}

		void Insert(std::shared_ptr<Node<T>>& data, const Point& p)
		{
			if (!data)
				return;

			if (!region.InBounds(p))
				return;

			if (nodes.size() >= capacity)
			{
				// subdivide and resize
				subdivide();
				balance();
				return;
			}
			nodes.emplace_back(data);
		}

		void balance()
		{
			for (auto n : nodes)
			{
				if (topLeft && topLeft->region.InBounds(n->position))
				{
					topLeft->Insert(n, n->position);
				}
				if (topRight && topRight->region.InBounds(n->position))
				{
					topRight->Insert(n, n->position);
				}
				if (bottomLeft && bottomLeft->region.InBounds(n->position))
				{
					bottomLeft->Insert(n, n->position);
				}
				if (bottomRight && bottomRight->region.InBounds(n->position))
				{
					bottomRight->Insert(n, n->position);
				}
			}
			nodes.clear();
		}
	};
}

