#pragma once
#include "Logo.hpp"

namespace LogoGL {
	typedef void* (*Resolver)(const char* name);

	bool Draw(const void* context, int width, int height, float time, Resolver resolve);

	// Deletes every GL object. The context that created them must be current.
	void Release(const void* context);
}
