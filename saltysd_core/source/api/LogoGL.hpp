#pragma once
#include "Logo.hpp"

namespace LogoGL {
	typedef void* (*Resolver)(const char* name);

	bool Draw(const void* context, int width, int height, float time, Resolver resolve);

	// Deletes every GL object. The context that created them must be current.
	void Release(const void* context);

	// The context is being destroyed and its objects with it: they are only forgotten (no GL calls).
	// A context created afterwards can get the same handle, so ownerContext alone can't tell them apart.
	void ContextDestroyed(const void* context);
}
