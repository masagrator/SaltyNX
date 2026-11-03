#pragma once
#include "../NX-FPS-Common.hpp"
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#include <glad/egl.h>
#include <glad/gles2.h> // OpenGL ES 3.2 + GL_NV_viewport_array + GL_OES_viewport_array
#define GL_UNIFORM_BUFFER_UNIFIED_NV 0x936E

namespace EGL {
	typedef PFNGLVIEWPORTARRAYVOESPROC PFNGLVIEWPORTARRAYVPROC;
	typedef PFNGLVIEWPORTINDEXEDFOESPROC PFNGLVIEWPORTINDEXEDFPROC;
	typedef PFNGLVIEWPORTINDEXEDFVOESPROC PFNGLVIEWPORTINDEXEDFVPROC;

	inline PFNEGLSWAPBUFFERSPROC eglSwapBuffers_0;
	inline PFNEGLSWAPINTERVALPROC eglSwapInterval_0;
	inline PFNEGLGETPROCADDRESSPROC eglGetProcAddress_0;
	inline PFNEGLDESTROYCONTEXTPROC eglDestroyContext_0;
	inline PFNGLVIEWPORTPROC glViewport_0;
	inline PFNGLVIEWPORTARRAYVPROC glViewportArrayv_0;
	inline PFNGLVIEWPORTARRAYVNVPROC glViewportArrayvNV_0;
	inline PFNGLVIEWPORTARRAYVOESPROC glViewportArrayvOES_0;
	inline PFNGLVIEWPORTINDEXEDFPROC glViewportIndexedf_0;
	inline PFNGLVIEWPORTINDEXEDFNVPROC glViewportIndexedfNV_0;
	inline PFNGLVIEWPORTINDEXEDFOESPROC glViewportIndexedfOES_0;
	inline PFNGLVIEWPORTINDEXEDFVPROC glViewportIndexedfv_0;
	inline PFNGLVIEWPORTINDEXEDFVNVPROC glViewportIndexedfvNV_0;
	inline PFNGLVIEWPORTINDEXEDFVOESPROC glViewportIndexedfvOES_0;
	typedef void (*GLCapProc)(GLenum cap);
	inline GLCapProc glEnable_0;
	inline GLCapProc glDisable_0;
	inline GLCapProc glEnableClientState_0;
	inline GLCapProc glDisableClientState_0;

	EGLBoolean Interval(EGLDisplay display, EGLint interval);
	EGLBoolean Swap(EGLDisplay display, EGLSurface surface);
	EGLBoolean DestroyContext(EGLDisplay display, EGLContext context);
	void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
	void ViewportArrayv(GLuint first, GLsizei count, const GLfloat* v);
	void ViewportArrayvNV(GLuint first, GLsizei count, const GLfloat* v);
	void ViewportArrayvOES(GLuint first, GLsizei count, const GLfloat* v);
	void ViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
	void ViewportIndexedfNV(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
	void ViewportIndexedfOES(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
	void ViewportIndexedfv(GLuint index, const GLfloat* v);
	void ViewportIndexedfvNV(GLuint index, const GLfloat* v);
	void ViewportIndexedfvOES(GLuint index, const GLfloat* v);
	void Enable(GLenum cap);
	void Disable(GLenum cap);
	void EnableClientState(GLenum cap);
	void DisableClientState(GLenum cap);
	__eglMustCastToProperFunctionPointerType GetProc(const char* procname);

	namespace Logo {
		extern bool done; // finished, failed, or disabled by nologo.flag
	}
}
