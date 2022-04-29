module;
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string_view>
#include <memory>

export module DX12Device;

namespace LS
{
	export class LSDeviceDX12;

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
		void Render();
	};
}