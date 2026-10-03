#pragma once
#include "../NX-FPS-Common.hpp"
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#include <glad/egl.h>
#include <glad/gles2.h> // OpenGL ES 3.2 + GL_NV_viewport_array + GL_OES_viewport_array

namespace EGL {
	typedef PFNGLVIEWPORTARRAYVOESPROC PFNGLVIEWPORTARRAYVPROC;
	typedef PFNGLVIEWPORTINDEXEDFOESPROC PFNGLVIEWPORTINDEXEDFPROC;
	typedef PFNGLVIEWPORTINDEXEDFVOESPROC PFNGLVIEWPORTINDEXEDFVPROC;

	inline PFNEGLSWAPBUFFERSPROC eglSwapBuffers_0;
	inline PFNEGLSWAPINTERVALPROC eglSwapInterval_0;
	inline PFNEGLGETPROCADDRESSPROC eglGetProcAddress_0;
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

	EGLBoolean Interval(EGLDisplay display, EGLint interval);
	EGLBoolean Swap(EGLDisplay display, EGLSurface surface);
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
	__eglMustCastToProperFunctionPointerType GetProc(const char* procname);

	namespace Logo {
		extern bool done; // finished, failed, or disabled by nologo.flag
	}
}
