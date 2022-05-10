module;
#include <string>
#include <memory>
#include <array>

export module DX12Device;

namespace LS
{
	export class LSDeviceDX12;

	export struct ColorRGBA
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;
	};

	template<class T, size_t Count>
	struct Vector
	{
		std::array<T, Count> Vec;
	};

	export struct Vertex
	{
		Vector<float, 3> position;
		Vector<float, 4> color;
	};
	
	export struct VertexPT
	{
		Vector<float, 4> position;
		Vector<float, 2> uv;
	};

	export class LSDevice
	{
	private:
		std::unique_ptr<LSDeviceDX12> m_pImpl;
	public:
		LSDevice();
		virtual ~LSDevice();
		bool CreateDevice(void* handle, uint32_t x = 0, uint32_t y = 0);
		void CheckFeatures(std::string& s);
		void CleanupDevice();
		void Render(const ColorRGBA& clearColor = {});
	};
}