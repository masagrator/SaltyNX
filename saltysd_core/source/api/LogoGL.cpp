#include "LogoGL.hpp"
#include "Lz4.hpp"
#include <cstring>
#include <glad/gl.h>

namespace LogoGL {
	namespace {
		// Capabilities that change how our draw (or the copy) comes out. The last three are desktop GL only.
		constexpr GLenum kCaps[] = {
			GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_RASTERIZER_DISCARD,
			GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE, GL_SAMPLE_MASK, GL_POLYGON_OFFSET_FILL,
			GL_FRAMEBUFFER_SRGB, GL_COLOR_LOGIC_OP, GL_CLIP_DISTANCE0, // desktop only (the shader doesn't write gl_ClipDistance)
		};
		constexpr int kCapCount = sizeof(kCaps) / sizeof(kCaps[0]);
		constexpr int kDesktopOnlyCaps = 3;

		#define LOGO_GL_FUNCTIONS(X) \
			X(GetString, GETSTRING) \
			X(GetIntegerv, GETINTEGERV) \
			X(GetIntegeri_v, GETINTEGERI_V) \
			X(GetInteger64i_v, GETINTEGER64I_V) \
			X(GetBooleanv, GETBOOLEANV) \
			X(IsEnabled, ISENABLED) \
			X(Enable, ENABLE) \
			X(Disable, DISABLE) \
			X(CreateShader, CREATESHADER) \
			X(ShaderSource, SHADERSOURCE) \
			X(CompileShader, COMPILESHADER) \
			X(GetShaderiv, GETSHADERIV) \
			X(DeleteShader, DELETESHADER) \
			X(CreateProgram, CREATEPROGRAM) \
			X(AttachShader, ATTACHSHADER) \
			X(LinkProgram, LINKPROGRAM) \
			X(GetProgramiv, GETPROGRAMIV) \
			X(DeleteProgram, DELETEPROGRAM) \
			X(UseProgram, USEPROGRAM) \
			X(GenVertexArrays, GENVERTEXARRAYS) \
			X(DeleteVertexArrays, DELETEVERTEXARRAYS) \
			X(BindVertexArray, BINDVERTEXARRAY) \
			X(GenBuffers, GENBUFFERS) \
			X(DeleteBuffers, DELETEBUFFERS) \
			X(BindBuffer, BINDBUFFER) \
			X(BindBufferBase, BINDBUFFERBASE) \
			X(BindBufferRange, BINDBUFFERRANGE) \
			X(BufferData, BUFFERDATA) \
			X(GenTextures, GENTEXTURES) \
			X(DeleteTextures, DELETETEXTURES) \
			X(BindTexture, BINDTEXTURE) \
			X(ActiveTexture, ACTIVETEXTURE) \
			X(TexStorage2D, TEXSTORAGE2D) \
			X(TexParameteri, TEXPARAMETERI) \
			X(BindSampler, BINDSAMPLER) \
			X(GenFramebuffers, GENFRAMEBUFFERS) \
			X(DeleteFramebuffers, DELETEFRAMEBUFFERS) \
			X(BindFramebuffer, BINDFRAMEBUFFER) \
			X(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D) \
			X(CheckFramebufferStatus, CHECKFRAMEBUFFERSTATUS) \
			X(BlitFramebuffer, BLITFRAMEBUFFER) \
			X(Viewport, VIEWPORT) \
			X(ColorMask, COLORMASK) \
			X(PolygonMode, POLYGONMODE) \
			X(DrawArrays, DRAWARRAYS)

		#define LOGO_GL_MEMBER(name, upper) PFNGL##upper##PROC name;
		struct Functions {
			LOGO_GL_FUNCTIONS(LOGO_GL_MEMBER)
		} gl{};
		#undef LOGO_GL_MEMBER

		#define LOGO_GL_NAME(name, upper) "gl" #name "\0"
		constexpr char kFunctionNames[] = LOGO_GL_FUNCTIONS(LOGO_GL_NAME);
		#undef LOGO_GL_NAME
		constexpr size_t CountNames(const char* names, size_t size) {
			size_t count = 0;
			for (size_t i = 0; i + 1 < size; i++) if (names[i] == '\0') count++;
			return count;
		}
		static_assert(CountNames(kFunctionNames, sizeof(kFunctionNames)) == sizeof(Functions) / sizeof(void*),
			"kFunctionNames must name every member of Functions");

#ifndef LOGO_GL_EXTERNAL_SOURCES
		// Minified + LZ4 packed by logo/pack_shader.py (run by make), unpacked only while the program is built.
		const unsigned char vertexPacked[] = {
			#embed "../../logo/vert.glsl.lz4"
		};
		const unsigned char fragmentPacked[] = {
			#embed "../../logo/frag.glsl.lz4"
		};
#endif

		// Replaces the "#version 450" line of the sources. FB_Y_DOWN false: the copy of the default
		// framebuffer has its rows bottom to top.
		constexpr const char kDesktopHeader[] = "#version 450\n#define FB_Y_DOWN false\n";
		constexpr const char kEsHeader[] =
			"#version 320 es\n"
			"precision highp float;\nprecision highp int;\nprecision highp sampler2D;\n"
			"#define FB_Y_DOWN false\n";

		const void* ownerContext = nullptr;  // context the objects below belong to
		bool functionsLoaded = false;
		bool ready = false;
		bool failed = false;
		bool isES = false;
		GLuint program = 0, vao = 0, ubo = 0, texture = 0, fbo = 0;
		int textureWidth = 0, textureHeight = 0;

		bool loadFunctions(Resolver resolve) {
			const char* name = kFunctionNames;
			for (size_t i = 0; i < sizeof(Functions) / sizeof(void*); i++, name += strlen(name) + 1) {
				void* address = resolve(name);
				// Everything except glPolygonMode (not in OpenGL ES) is required.
				if (!address && strcmp(name, "glPolygonMode") != 0) return false;
				memcpy(reinterpret_cast<char*>(&gl) + i * sizeof(void*), &address, sizeof(void*));
			}
			return true;
		}

		const char* skipVersionLine(const char* source) {
			const char* newline = strchr(source, '\n');
			return newline ? newline + 1 : source;
		}

		GLuint compile(GLenum type, const char* source) {
			GLuint shader = gl.CreateShader(type);
			if (!shader) return 0;
			const GLchar* parts[2] = { isES ? kEsHeader : kDesktopHeader, skipVersionLine(source) };
			gl.ShaderSource(shader, 2, parts, nullptr);
			gl.CompileShader(shader);
			GLint ok = 0;
			gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			if (!ok) {
				gl.DeleteShader(shader);
				return 0;
			}
			return shader;
		}

		GLuint compilePacked(GLenum type, const unsigned char* packed, size_t packedSize) {
			char* source = (char*)Lz4::Unpack(packed, packedSize, nullptr);
			if (!source) return 0;
			GLuint shader = compile(type, source); // glShaderSource copies the text
			free(source);
			return shader;
		}

		bool build() {
			GLuint vs = compilePacked(GL_VERTEX_SHADER, vertexPacked, sizeof(vertexPacked));
			GLuint fs = vs ? compilePacked(GL_FRAGMENT_SHADER, fragmentPacked, sizeof(fragmentPacked)) : 0;
			if (!vs || !fs) {
				if (vs) gl.DeleteShader(vs);
				if (fs) gl.DeleteShader(fs);
				return false;
			}
			program = gl.CreateProgram();
			gl.AttachShader(program, vs);
			gl.AttachShader(program, fs);
			gl.LinkProgram(program);
			gl.DeleteShader(vs); // flagged for deletion, freed together with the program
			gl.DeleteShader(fs);
			GLint linked = 0;
			gl.GetProgramiv(program, GL_LINK_STATUS, &linked);
			if (!linked) {
				gl.DeleteProgram(program);
				program = 0;
				return false;
			}
			gl.GenVertexArrays(1, &vao);   // core profiles can't draw without one
			gl.GenBuffers(1, &ubo);
			gl.GenFramebuffers(1, &fbo);
			return vao && ubo && fbo;
		}

		void deleteObjects() {
			if (program) gl.DeleteProgram(program);
			if (vao) gl.DeleteVertexArrays(1, &vao);
			if (ubo) gl.DeleteBuffers(1, &ubo);
			if (texture) gl.DeleteTextures(1, &texture);
			if (fbo) gl.DeleteFramebuffers(1, &fbo);
			program = vao = ubo = texture = fbo = 0;
			textureWidth = textureHeight = 0;
		}

		struct SavedState {
			GLint program, vao, activeTexture, texture2D, sampler;
			GLint uniformBuffer, indexedBuffer;
			GLint64 indexedStart, indexedSize;
			GLint drawFramebuffer, readFramebuffer;
			GLint viewport[4];
			GLboolean colorMask[4];
			GLint polygonMode[2];
			GLboolean caps[kCapCount];
		};

		void save(SavedState& s) {
			gl.GetIntegerv(GL_CURRENT_PROGRAM, &s.program);
			gl.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vao);
			gl.GetIntegerv(GL_ACTIVE_TEXTURE, &s.activeTexture);
			gl.ActiveTexture(GL_TEXTURE0);
			gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &s.texture2D);
			gl.GetIntegerv(GL_SAMPLER_BINDING, &s.sampler);
			gl.GetIntegerv(GL_UNIFORM_BUFFER_BINDING, &s.uniformBuffer);
			gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &s.indexedBuffer);
			gl.GetInteger64i_v(GL_UNIFORM_BUFFER_START, 0, &s.indexedStart);
			gl.GetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, 0, &s.indexedSize);
			gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s.drawFramebuffer);
			gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s.readFramebuffer);
			gl.GetIntegerv(GL_VIEWPORT, s.viewport);
			gl.GetBooleanv(GL_COLOR_WRITEMASK, s.colorMask);
			s.polygonMode[0] = s.polygonMode[1] = GL_FILL;
			if (!isES && gl.PolygonMode) gl.GetIntegerv(GL_POLYGON_MODE, s.polygonMode);
			const int caps = isES ? kCapCount - kDesktopOnlyCaps : kCapCount;
			for (int i = 0; i < caps; i++) s.caps[i] = gl.IsEnabled(kCaps[i]);
		}

		void restore(const SavedState& s) {
			const int caps = isES ? kCapCount - kDesktopOnlyCaps : kCapCount;
			for (int i = 0; i < caps; i++) {
				if (s.caps[i]) gl.Enable(kCaps[i]);
				else gl.Disable(kCaps[i]);
			}
			if (!isES && gl.PolygonMode) gl.PolygonMode(GL_FRONT_AND_BACK, (GLenum)s.polygonMode[0]);
			gl.ColorMask(s.colorMask[0], s.colorMask[1], s.colorMask[2], s.colorMask[3]);
			gl.Viewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
			gl.BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)s.readFramebuffer);
			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)s.drawFramebuffer);
			if (s.indexedBuffer && s.indexedSize > 0)
				gl.BindBufferRange(GL_UNIFORM_BUFFER, 0, (GLuint)s.indexedBuffer, (GLintptr)s.indexedStart, (GLsizeiptr)s.indexedSize);
			else
				gl.BindBufferBase(GL_UNIFORM_BUFFER, 0, (GLuint)s.indexedBuffer);
			gl.BindBuffer(GL_UNIFORM_BUFFER, (GLuint)s.uniformBuffer); // BindBufferBase/Range changed it too
			gl.BindSampler(0, (GLuint)s.sampler);
			gl.BindTexture(GL_TEXTURE_2D, (GLuint)s.texture2D);  // unit 0 is still active
			gl.ActiveTexture((GLenum)s.activeTexture);
			gl.BindVertexArray((GLuint)s.vao);
			gl.UseProgram((GLuint)s.program);
		}

		// (Re)creates the copy texture when the area size changes.
		bool ensureTexture(int width, int height) {
			if (texture && textureWidth == width && textureHeight == height) return true;
			if (texture) gl.DeleteTextures(1, &texture);
			texture = 0;
			gl.GenTextures(1, &texture);
			if (!texture) return false;
			gl.BindTexture(GL_TEXTURE_2D, texture);
			gl.TexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
			gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
			if (gl.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
			textureWidth = width;
			textureHeight = height;
			return true;
		}
	}

	bool Draw(const void* context, int width, int height, float time, Resolver resolve) {
		if (width <= 0 || height <= 0) return false;
		if (context != ownerContext) {
			ownerContext = context;
			ready = failed = false;
			program = vao = ubo = texture = fbo = 0;
			textureWidth = textureHeight = 0;
		}
		if (failed) return false;
		if (!functionsLoaded) {
			functionsLoaded = loadFunctions(resolve);
			if (!functionsLoaded) { failed = true; return false; }
		}
		if (!ready) {
			const char* version = (const char*)gl.GetString(GL_VERSION);
			if (!version) { failed = true; return false; }
			isES = strncmp(version, "OpenGL ES", 9) == 0;
		}

		SavedState saved;
		save(saved);
		bool ok = true;

		if (!ready) {
			ready = build();
			ok = ready;
		}

		const Logo::Region region = Logo::BottomLeftRegion(width, height, false);
		if (ok) ok = ensureTexture(region.width, region.height);

		if (ok) {
			const int caps = isES ? kCapCount - kDesktopOnlyCaps : kCapCount;
			for (int i = 0; i < caps; i++) gl.Disable(kCaps[i]);
			if (!isES && gl.PolygonMode) gl.PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			gl.ColorMask(1, 1, 1, 1);

			gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
			gl.BlitFramebuffer(region.x, region.y, region.x + region.width, region.y + region.height,
			                   0, 0, region.width, region.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

			Logo::Params params{};
			params.time = time;
			params.version = Logo::kVersion.packed;
			params.cropWidth = (uint32_t)width;
			params.cropHeight = (uint32_t)height;
			params.cropX = 0;
			params.cropY = 0;
			params.regionX = region.x;
			params.regionY = region.y;
			gl.BindBuffer(GL_UNIFORM_BUFFER, ubo);
			gl.BufferData(GL_UNIFORM_BUFFER, sizeof(params), &params, GL_DYNAMIC_DRAW);
			gl.BindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			gl.Viewport(0, 0, width, height);
			gl.UseProgram(program);
			gl.BindVertexArray(vao);
			gl.BindTexture(GL_TEXTURE_2D, texture); // unit 0
			gl.BindSampler(0, 0);                    // a game sampler could make the texture incomplete
			gl.DrawArrays(GL_TRIANGLES, 0, Logo::VERTEX_COUNT);
		}

		if (!ok) {
			deleteObjects();
			ready = false;
			failed = true;
		}
		restore(saved);
		return ok;
	}

	void Release(const void* context) {
		if (context != ownerContext || !functionsLoaded) return;
		deleteObjects();
		ready = false;
		ownerContext = nullptr;
	}
}
