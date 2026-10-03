#pragma once
#include <cstdint>
#include <cstddef>

namespace Logo {
	constexpr uint64_t DURATION_SECONDS = 3;
	constexpr uint32_t MIN_FRAMES = 90; // shown at least this many frames, even when the time ran out first
	constexpr int VERTEX_COUNT = 924; // fixed size draw, unused quads collapse in the vertex shader

	struct Params {
		float    time;        // seconds since the first frame the logo was drawn
		uint32_t version;     // major | minor << 8 | micro << 16 (bit pattern, read as uint in GLSL)
		uint32_t cropWidth;   // visible area in framebuffer texels
		uint32_t cropHeight;
		uint32_t cropX;
		uint32_t cropY;
		int32_t  regionX;     // framebuffer texel that is texel (0,0) of the sampled texture
		int32_t  regionY;     // (0,0) when the frame itself is sampled (NVN)
	};
	static_assert(sizeof(Params) == 32, "Params must match the std140 layout of the GLSL block");

	// Compile time parse of APP_VERSION ("major.minor.micro" + optional suffix, f.e. "2.1.0-beta").
	struct ParsedVersion {
		uint32_t packed;
		bool ok;
	};
	consteval ParsedVersion ParseVersion(const char* s) {
		uint32_t packed = 0;
		bool ok = true;
		for (int part = 0; part < 3; part++) {
			if (*s < '0' || *s > '9') {
				if (part == 0) ok = false;
				break;
			}
			uint32_t value = 0;
			while (*s >= '0' && *s <= '9') {
				value = value * 10 + (uint32_t)(*s - '0');
				if (value > 255) { ok = false; value = 255; }
				s++;
			}
			packed |= value << (8 * part);
			if (*s != '.') break;
			s++;
		}
		return {packed, ok};
	}
	constexpr ParsedVersion kVersion = ParseVersion(APP_VERSION);
	static_assert(kVersion.ok, "APP_VERSION must look like <major>.<minor>.<micro>[suffix], every number 0-255");

	// GL and Vulkan can't sample the image they draw into, so the area under the text is copied first.
	constexpr int REGION_W_NUM = 1, REGION_W_DEN = 2;
	constexpr int REGION_H_NUM = 1, REGION_H_DEN = 4;

	struct Region {
		int x, y, width, height;
	};
	// yDown: rows of the image run top to bottom (Vulkan), otherwise bottom to top (OpenGL).
	constexpr Region BottomLeftRegion(int width, int height, bool yDown) {
		const int w = width * REGION_W_NUM / REGION_W_DEN;
		const int h = height * REGION_H_NUM / REGION_H_DEN;
		return { 0, yDown ? height - h : 0, w > 0 ? w : 1, h > 0 ? h : 1 };
	}
}
