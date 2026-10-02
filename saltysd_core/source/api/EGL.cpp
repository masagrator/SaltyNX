#include "EGL.hpp"
#include "LogoGL.hpp"

namespace EGL {
	constexpr EGLint MIN_SWAP_INTERVAL = 0;
	constexpr EGLint MAX_SWAP_INTERVAL = 4;

	EGLBoolean Interval(EGLDisplay display, EGLint interval) {
		EGLBoolean result = EGL_FALSE;
		if (!changeFPS) {
			result = eglSwapInterval_0(display, interval);
			changedFPS = false;
			if (result == EGL_TRUE) {
				(Shared -> FPSmode) = std::clamp(interval, MIN_SWAP_INTERVAL, MAX_SWAP_INTERVAL);
			}
		}
		else if (interval < 0) {
			interval *= -1;
			if ((Shared -> FPSmode) != interval) {
				result = eglSwapInterval_0(display, interval);
				if (result == EGL_TRUE)
					(Shared -> FPSmode) = interval;
			}
			changedFPS = true;
		}
		return result;
	}

	namespace Logo {
		PFNEGLQUERYSURFACEPROC eglQuerySurface_0 = nullptr;
		PFNEGLGETCURRENTCONTEXTPROC eglGetCurrentContext_0 = nullptr;
		bool done = false;
		uint64_t startTick = 0;
		uint64_t endTick = 0;

		void* Resolve(const char* name) {
			void* address = eglGetProcAddress_0 ? (void*)eglGetProcAddress_0(name) : nullptr;
			if (!address) address = (void*)SaltySDCore_FindSymbolBuiltin(name);
			return address;
		}

		// Right before the real eglSwapBuffers: the back buffer holds the finished frame.
		void Draw(EGLDisplay display, EGLSurface surface) {
			if (!eglQuerySurface_0) {
				eglQuerySurface_0 = (PFNEGLQUERYSURFACEPROC)Resolve("eglQuerySurface");
				eglGetCurrentContext_0 = (PFNEGLGETCURRENTCONTEXTPROC)Resolve("eglGetCurrentContext");
				if (!eglQuerySurface_0 || !eglGetCurrentContext_0) { done = true; return; }
			}
			const EGLContext context = eglGetCurrentContext_0();
			const uint64_t now = Utils::_getSystemTick();
			if (endTick == 0) {
				startTick = now;
				endTick = now + systemtickfrequency * ::Logo::DURATION_SECONDS;
			}
			if (now >= endTick && Shared->frameNumber >= ::Logo::MIN_FRAMES) {
				LogoGL::Release(context);
				done = true;
				return;
			}
			EGLint width = 0, height = 0;
			if (!eglQuerySurface_0(display, surface, EGL_WIDTH, &width) || !eglQuerySurface_0(display, surface, EGL_HEIGHT, &height)) return;
			const float time = (float)((double)(now - startTick) / (double)systemtickfrequency);
			if (!LogoGL::Draw(context, width, height, time, Resolve)) done = true;
		}
	}

	EGLBoolean Swap(EGLDisplay display, EGLSurface surface) {

		if (NX_FPS_Math::starttick == 0) [[unlikely]] {
			(Shared -> API) = 2;
			NX_FPS_Math::starttick = Utils::_getSystemTick();
			NX_FPS_Math::starttick2 = NX_FPS_Math::starttick;
		}

		NX_FPS_Math::PreFrame();

		if (!Logo::done) Logo::Draw(display, surface);

		const EGLBoolean result = eglSwapBuffers_0(display, surface);
		if (result == EGL_TRUE)
			 NX_FPS_Math::PostFrame();
		
		const auto new_fpslock = NX_FPS_Math::new_fpslock;

		if (!new_fpslock) {
			NX_FPS_Math::FPStiming = 0;
			NX_FPS_Math::FPSlock = 0;
			changeFPS = false;
		}
		else {
			changeFPS = true;
			auto currentRefreshRate = (Shared -> currentRefreshRate);
			NX_FPS_Math::FPSlock = ((*sharedOperationMode == 1) ? (Shared -> FPSlockedDocked) : (Shared -> FPSlocked));
			auto FPSmode = (Shared -> FPSmode);
			const uint32_t rr = currentRefreshRate ? currentRefreshRate : 60;
			uint32_t threshold;
			int interval;
			if      (new_fpslock <= rr / 4) {interval = 4; threshold = rr / 4;}
			else if (new_fpslock <= rr / 3) {interval = 3; threshold = rr / 3;}
			else if (new_fpslock <= rr / 2) {interval = 2; threshold = rr / 2;}
			else                            {interval = 1; threshold = rr;    }

			if (FPSmode != interval) {
				EGL::Interval(display, interval * -1);
			}
			if (new_fpslock != threshold) {
				NX_FPS_Math::FPStiming = (systemtickfrequency/new_fpslock) - 6000;
			}
			else NX_FPS_Math::FPStiming = 0;
		}

		return result;
	}

	namespace Common {
		NOINLINE void ViewportArrayv(GLuint first, GLsizei count, const GLfloat* v, PFNGLVIEWPORTARRAYVPROC pointer) {
			if (resolutionLookup) for (GLsizei i = 0; i < count; i++) {
				const GLfloat* viewport = v + i * 4;
				if (viewport[3] > 1.f && viewport[2] > 1.f && viewport[0] == 0.f && viewport[1] == 0.f) {
					NX_FPS_Math::addResToViewports((uint32_t)viewport[2], (uint32_t)viewport[3]);
				}
			}
			return pointer(first, count, v);
		}

		NOINLINE void ViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h, PFNGLVIEWPORTINDEXEDFPROC pointer) {
			if (resolutionLookup && h > 1.f && w > 1.f && !x && !y) {
				NX_FPS_Math::addResToViewports((uint32_t)w, (uint32_t)h);
			}
			return pointer(index, x, y, w, h);
		}

		// v holds the one viewport being set (x, y, width, height), index only selects which one.
		NOINLINE void ViewportIndexedfv(GLuint index, const GLfloat* v, PFNGLVIEWPORTINDEXEDFVPROC pointer) {
			if (resolutionLookup && v[3] > 1.f && v[2] > 1.f && v[0] == 0.f && v[1] == 0.f) {
				NX_FPS_Math::addResToViewports((uint32_t)v[2], (uint32_t)v[3]);
			}
			return pointer(index, v);
		}
	}

	void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
		if (resolutionLookup && height > 1 && width > 1 && !x && !y) {
			NX_FPS_Math::addResToViewports((uint32_t)width, (uint32_t)height);
		}
		return glViewport_0(x, y, width, height);
	}

	void ViewportArrayv(GLuint first, GLsizei count, const GLfloat* v) {
		return EGL::Common::ViewportArrayv(first, count, v, glViewportArrayv_0);
	}

	void ViewportArrayvNV(GLuint first, GLsizei count, const GLfloat* v) {
		return EGL::Common::ViewportArrayv(first, count, v, glViewportArrayvNV_0);
	}

	void ViewportArrayvOES(GLuint first, GLsizei count, const GLfloat* v) {
		return EGL::Common::ViewportArrayv(first, count, v, glViewportArrayvOES_0);
	}

	void ViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
		return EGL::Common::ViewportIndexedf(index, x, y, w, h, glViewportIndexedf_0);
	}

	void ViewportIndexedfNV(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
		return EGL::Common::ViewportIndexedf(index, x, y, w, h, glViewportIndexedfNV_0);
	}

	void ViewportIndexedfOES(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
		return EGL::Common::ViewportIndexedf(index, x, y, w, h, glViewportIndexedfOES_0);
	}

	void ViewportIndexedfv(GLuint index, const GLfloat* v) {
		return EGL::Common::ViewportIndexedfv(index, v, glViewportIndexedfv_0);
	}

	void ViewportIndexedfvNV(GLuint index, const GLfloat* v) {
		return EGL::Common::ViewportIndexedfv(index, v, glViewportIndexedfvNV_0);
	}

	void ViewportIndexedfvOES(GLuint index, const GLfloat* v) {
		return EGL::Common::ViewportIndexedfv(index, v, glViewportIndexedfvOES_0);
	}

	__eglMustCastToProperFunctionPointerType GetProc(const char* eglName) {
		uintptr_t address = (uintptr_t)eglGetProcAddress_0(eglName);

		//They must be inside function otherwise 32-bit Core stucks at relocations
		std::array egl_replacements = {
			runtime_replace{"eglSwapInterval", (uintptr_t*)&eglSwapInterval_0, (void*)Interval},
			runtime_replace{"eglSwapBuffers", (uintptr_t*)&eglSwapBuffers_0, (void*)Swap},
			runtime_replace{"glViewport", (uintptr_t*)&glViewport_0, (void*)Viewport},
			runtime_replace{"glViewportArrayv", (uintptr_t*)&glViewportArrayv_0, (void*)ViewportArrayv},
			runtime_replace{"glViewportArrayvNV", (uintptr_t*)&glViewportArrayvNV_0, (void*)ViewportArrayvNV},
			runtime_replace{"glViewportArrayvOES", (uintptr_t*)&glViewportArrayvOES_0, (void*)ViewportArrayvOES},
			runtime_replace{"glViewportIndexedf", (uintptr_t*)&glViewportIndexedf_0, (void*)ViewportIndexedf},
			runtime_replace{"glViewportIndexedfNV", (uintptr_t*)&glViewportIndexedfNV_0, (void*)ViewportIndexedfNV},
			runtime_replace{"glViewportIndexedfOES", (uintptr_t*)&glViewportIndexedfOES_0, (void*)ViewportIndexedfOES},
			runtime_replace{"glViewportIndexedfv", (uintptr_t*)&glViewportIndexedfv_0, (void*)ViewportIndexedfv},
			runtime_replace{"glViewportIndexedfvNV", (uintptr_t*)&glViewportIndexedfvNV_0, (void*)ViewportIndexedfvNV},
			runtime_replace{"glViewportIndexedfvOES", (uintptr_t*)&glViewportIndexedfvOES_0, (void*)ViewportIndexedfvOES}
		};

		for (const auto& replacement : egl_replacements) {
			if (!strcmp(replacement.name, eglName)) {
				if (replacement.orig_ptr && *replacement.orig_ptr == 0) *replacement.orig_ptr = address;
				if (replacement.hook_ptr) return (__eglMustCastToProperFunctionPointerType)replacement.hook_ptr;
				break;
			}
		}
		return (__eglMustCastToProperFunctionPointerType)address;
	}
}
