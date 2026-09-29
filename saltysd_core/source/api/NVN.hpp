#pragma once
#include "../NX-FPS-Common.hpp"

namespace NVN {
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
		void LoadShaderFiles();
	}
}
