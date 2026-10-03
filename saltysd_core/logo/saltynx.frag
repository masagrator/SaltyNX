#version 450
// One source for NVN (offline glslc), OpenGL / OpenGL ES (compiled at runtime, the #version line is
// replaced by the host) and Vulkan (glslangValidator -V, which defines VULKAN).
#ifdef VULKAN
    #define PARAMS_LAYOUT layout(std140, set = 0, binding = 0)
    #define FRAMEBUFFER_LAYOUT layout(set = 0, binding = 1)
    #define VERTEX_ID gl_VertexIndex
#else
    #define PARAMS_LAYOUT layout(std140, binding = 0)
    #define FRAMEBUFFER_LAYOUT layout(binding = 0)
    #define VERTEX_ID gl_VertexID
#endif

layout(location = 0) in  vec3 vBarycentric;
layout(location = 1) in  vec2 vFbCoord;
layout(location = 0) out vec4 fragColor;

PARAMS_LAYOUT uniform Params {
    highp float uTime;        // seconds since the first frame was drawn
    highp uint  uVersion;     // major | minor << 8 | micro << 16 (bitcast of the packed version)
    highp uint  uCropWidth;
    highp uint  uCropHeight;
    highp uint  uCropX;
    highp uint  uCropY;
    highp int   uRegionX;     // framebuffer texel that is texel (0,0) of uFramebuffer
    highp int   uRegionY;     // (0,0) on NVN, which samples the frame itself; GL/Vulkan sample a copy
};

FRAMEBUFFER_LAYOUT uniform highp sampler2D uFramebuffer;

const float MIN_CONTRAST = 0.5;

const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

void main() {
    ivec2 texel = clamp(ivec2(vFbCoord) - ivec2(uRegionX, uRegionY), ivec2(0), textureSize(uFramebuffer, 0) - ivec2(1));
    vec3 background = texelFetch(uFramebuffer, texel, 0).rgb;

    vec3  inverse = vec3(1.0) - background;
    float lumBg   = dot(background, LUMA);
    float diff    = abs(dot(inverse, LUMA) - lumBg);          // == abs(1 - 2 * lumBg)

    float keep = clamp(diff / max(MIN_CONTRAST, 0.0001), 0.0, 1.0);
    keep = keep * keep * (3.0 - 2.0 * keep);                   // smoothstep
    vec3 extreme = vec3(lumBg > 0.5 ? 0.0 : 1.0);
	
	vec3 color = mix(extreme, inverse, keep);
	float fade = 1.0 - clamp((uTime - 2.5) / 0.5, 0.0, 1.0);

    fragColor = vec4(mix(background, color, fade), 1.0);
}
