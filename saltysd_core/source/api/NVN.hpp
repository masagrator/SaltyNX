#pragma once
#include "../NX-FPS-Common.hpp"

namespace NVN {
	struct Texture {
		char reserved[0xC0];
	};
	struct TextureView {
		char reserved[0x28];
	};
	struct TextureBuilder {
		char reserved[0x80];
	};
	struct Window {
		char reserved[0x180];
	};
	struct DeviceBuilder {
		char reserved[0x40];
	};
	struct Device {
		char reserved[0x3000];
	};
	struct CommandBuffer {
		char reserved[0xA0];
	};
	struct MemoryPool {
    	char reserved[0x100];
	};
	struct Sync {
		char reserved[0x40];
	};
	struct QueueBuilder {
    	char reserved[0x40];
	};
	struct Queue {
		char reserved[0x2000];
	};
	struct WindowBuilder {
		const char reserved[16];
		uint8_t numBufferedFrames;
		const char reserved2[47];
	};
	struct MemoryPoolBuilder {
		char reserved[0x40];
	};
	struct Program {
		char reserved[0xC0];
	};
	struct ShaderData {
		uint64_t data;
		const void* control;
	};
	struct DepthStencilState {
		char reserved[0x8];
	};
	struct ChannelMaskState {
		char reserved[0x4];
	};
	struct ColorState {
		char reserved[0x4];
	};
	struct BlendState {
		char reserved[0x8];
	};
	struct PolygonState {
		char reserved[0x4];
	};
	struct TexturePool {
		char reserved[0x20];
	};
	struct SamplerPool {
		char reserved[0x20];
	};
	struct SamplerBuilder {
		char reserved[0x60];
	};
	struct Sampler {
		char reserved[0x30];
	};

	struct Viewport {
		float x;
		float y;
		float width;
		float height;
	};
	struct Scissor {
		int x;
		int y;
		int width;
		int height;
	};
	struct CopyRegion {
		int x;
		int y;
		int z;
		int width;
		int height;
		int depth;
	};

	typedef int textureFlags;
	typedef int textureTarget;
	typedef int textureFormat;
	typedef int memoryPoolFlags;
	typedef uint64_t BufferAddress;
	typedef uint64_t CommandHandle;
	typedef uint64_t TextureHandle;

	struct Rectangle {
		int x;
		int y;
		int width;
		int height;
	};

	inline uintptr_t (*nvnBootstrapLoader_0)(const char* nvnName);
	uintptr_t BootstrapLoader_1(const char* nvnName);
	extern bool enableCounters;

	namespace TripleBuffer {
		constexpr int WINDOW_TEXTURES = 3;
		constexpr size_t TEXTURE_MEMORY_SIZE = (((1920 * 1152 * 4) * WINDOW_TEXTURES) + 0xFFF) & ~0xFFF;
		constexpr size_t RESERVED_MEMORY_SIZE = TEXTURE_MEMORY_SIZE;
		extern bool requested;
	}

	namespace Logo {
		extern bool done; // finished, failed, or disabled by nologo.flag
	}
}
