#pragma once
#include "Logo.hpp"
#include "NVN.hpp" // NVN types

// Logo renderer for NVN. Shaders are compiled offline (code + control sections) and embedded, the code
// sections are the shader memory pool itself. The frame is sampled directly (no copy needed), the draw is
// submitted on the game's queue right before present.
namespace LogoNVN {
	// Returns the address of an NVN function (nvnDeviceGetProcAddress).
	typedef void* (*Resolver)(const char* name);

	// Draws into target (width x height texels, crop = visible area, zero size = whole texture).
	// gameTexturePool / gameSamplerPool are rebound afterwards when not null.
	// Returns false when the logo can't be drawn on this device (it won't be retried).
	bool Draw(NVN::Device* device, const NVN::Queue* queue, const NVN::Texture* target, int width, int height,
	          const NVN::Rectangle& crop, float time,
	          const NVN::TexturePool* gameTexturePool, const NVN::SamplerPool* gameSamplerPool, Resolver resolve);
}
