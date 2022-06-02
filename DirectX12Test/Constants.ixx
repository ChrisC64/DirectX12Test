export module Constants;
import <cstdint>;
export namespace Engine
{
	constinit const uint32_t FRAME_COUNT = 3;// Maximum Frame Latency
	constinit const uint32_t NUM_CONTEXT = 3;// Number of contexts - think of contexts like D3D11's immediate context. The graphic commands and other contexts we want to run simultaneously
	constinit const uint32_t THREAD_COUNT = 4;// Max threads to use for program
}