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
out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec3 vBarycentric;
layout(location = 1) out vec2 vFbCoord;   // texel position of this vertex inside the framebuffer texture

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

const vec2  OFFSET  = vec2(-0.9999, -0.6200);   // left low bottom of the screen, just above Nintendo logo black bar
const float SCALE   = 0.1200;
const float SPACING = 1.0000;
const float ASPECT  = 1.7778;

// Texture rows run top to bottom on NVN and Vulkan. OpenGL defines FB_Y_DOWN false (bottom-up rows).
#ifndef FB_Y_DOWN
    #define FB_Y_DOWN true
#endif

const int GLYPH_COUNT = 18;

const int GLYPH_QUADS[GLYPH_COUNT] = int[](11, 3, 2, 2, 3, 3, 2, 12, 4, 10, 10, 8, 10, 12, 6, 14, 12, 1);
const int GLYPH_FIRST[GLYPH_COUNT] = int[](0, 11, 14, 16, 18, 21, 24, 26, 38, 42, 52, 62, 70, 80, 92, 98, 112, 124);
const float GLYPH_WIDTH[GLYPH_COUNT] = float[](0.600, 0.680, 0.520, 0.600, 0.680, 0.640, 0.640, 0.560, 0.180, 0.560, 0.560, 0.560, 0.560, 0.560, 0.560, 0.560, 0.560, 0.085);

const int TEXT_LEN = 18;
const int VERSION_FIRST_SLOT = 7;
const float VERSION_LEAD_GAP = 0.14;   // space between "X" and the first version digit

const int TEXT_SLOT_QUADS[TEXT_LEN] = int[](11, 3, 2, 2, 3, 3, 2, 14, 14, 14, 1, 14, 14, 14, 1, 14, 14, 14);
const float TEXT_SCALE[TEXT_LEN] = float[](1.00, 0.75, 0.75, 0.75, 0.75, 1.00, 1.00, 0.40, 0.40, 0.40, 0.40, 0.40, 0.40, 0.40, 0.40, 0.40, 0.40, 0.40);
const float TEXT_GAP[TEXT_LEN] = float[](0.00, 0.10, 0.10, 0.10, 0.10, 0.10, 0.10, 0.06, 0.06, 0.06, 0.06, 0.06, 0.06, 0.06, 0.06, 0.06, 0.06, 0.06);   // space before each glyph

// Two vec4 per quad:  (TLx,TLy, TRx,TRy)  (BLx,BLy, BRx,BRy)
const vec4 FONT_QUADS[250] = vec4[](
    // glyph 0: S
    vec4(-0.180, 0.500, 0.300, 0.500), vec4(-0.180, 0.350, 0.300, 0.350),
    vec4(-0.180, 0.500,-0.180, 0.350), vec4(-0.300, 0.380,-0.300, 0.350),
    vec4(-0.300, 0.350,-0.150, 0.350), vec4(-0.300, 0.045,-0.150, 0.045),
    vec4(-0.300, 0.045,-0.150, 0.045), vec4(-0.180,-0.075,-0.150,-0.075),
    vec4(-0.180, 0.075, 0.180, 0.075), vec4(-0.180,-0.075, 0.180,-0.075),
    vec4( 0.150, 0.075, 0.180, 0.075), vec4( 0.150,-0.045, 0.300,-0.045),
    vec4( 0.150,-0.045, 0.300,-0.045), vec4( 0.150,-0.350, 0.300,-0.350),
    vec4( 0.180,-0.350, 0.300,-0.350), vec4( 0.180,-0.500, 0.300,-0.380),
    vec4(-0.300,-0.350, 0.180,-0.350), vec4(-0.300,-0.500, 0.180,-0.500),
    vec4(-0.300,-0.250,-0.150,-0.250), vec4(-0.300,-0.350,-0.150,-0.350),
    vec4( 0.150, 0.350, 0.300, 0.350), vec4( 0.150, 0.250, 0.300, 0.250),
    // glyph 1: A
    vec4(-0.100, 0.500, 0.100, 0.500), vec4(-0.340,-0.500,-0.140,-0.500),
    vec4(-0.100, 0.500, 0.100, 0.500), vec4( 0.140,-0.500, 0.340,-0.500),
    vec4(-0.240,-0.080, 0.240,-0.080), vec4(-0.280,-0.260, 0.280,-0.260),
    // glyph 2: L
    vec4(-0.260, 0.500,-0.060, 0.500), vec4(-0.260,-0.500,-0.060,-0.500),
    vec4(-0.260,-0.300, 0.260,-0.300), vec4(-0.260,-0.500, 0.260,-0.500),
    // glyph 3: T
    vec4(-0.300, 0.500, 0.300, 0.500), vec4(-0.300, 0.300, 0.300, 0.300),
    vec4(-0.100, 0.300, 0.100, 0.300), vec4(-0.100,-0.500, 0.100,-0.500),
    // glyph 4: Y
    vec4(-0.340, 0.500,-0.140, 0.500), vec4(-0.100, 0.000, 0.100, 0.000),
    vec4( 0.140, 0.500, 0.340, 0.500), vec4(-0.100, 0.000, 0.100, 0.000),
    vec4(-0.100, 0.000, 0.100, 0.000), vec4(-0.100,-0.500, 0.100,-0.500),
    // glyph 5: N
    vec4(-0.320, 0.500,-0.160, 0.500), vec4(-0.320,-0.500,-0.160,-0.500),
    vec4( 0.160, 0.500, 0.320, 0.500), vec4( 0.160,-0.500, 0.320,-0.500),
    vec4(-0.320, 0.500,-0.160, 0.500), vec4( 0.160,-0.500, 0.320,-0.500),
    // glyph 6: X
    vec4(-0.320, 0.500,-0.160, 0.500), vec4( 0.160,-0.500, 0.320,-0.500),
    vec4( 0.160, 0.500, 0.320, 0.500), vec4(-0.320,-0.500,-0.160,-0.500),
    // glyph 7: 0
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    vec4(-0.280,-0.105,-0.190,-0.015), vec4(-0.280,-0.305,-0.190,-0.395),
    vec4(-0.190,-0.015,-0.100,-0.105), vec4(-0.190,-0.395,-0.100,-0.305),
    vec4(-0.280, 0.305,-0.190, 0.395), vec4(-0.280, 0.105,-0.190, 0.015),
    vec4(-0.190, 0.395,-0.100, 0.305), vec4(-0.190, 0.015,-0.100, 0.105),
    // glyph 8: 1
    vec4(-0.090, 0.305, 0.000, 0.395), vec4(-0.090, 0.105, 0.000, 0.015),
    vec4( 0.000, 0.395, 0.090, 0.305), vec4( 0.000, 0.015, 0.090, 0.105),
    vec4(-0.090,-0.105, 0.000,-0.015), vec4(-0.090,-0.305, 0.000,-0.395),
    vec4( 0.000,-0.015, 0.090,-0.105), vec4( 0.000,-0.395, 0.090,-0.305),
    // glyph 9: 2
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    vec4(-0.280,-0.105,-0.190,-0.015), vec4(-0.280,-0.305,-0.190,-0.395),
    vec4(-0.190,-0.015,-0.100,-0.105), vec4(-0.190,-0.395,-0.100,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    // glyph 10: 3
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    // glyph 11: 4
    vec4(-0.280, 0.305,-0.190, 0.395), vec4(-0.280, 0.105,-0.190, 0.015),
    vec4(-0.190, 0.395,-0.100, 0.305), vec4(-0.190, 0.015,-0.100, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    // glyph 12: 5
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4(-0.280, 0.305,-0.190, 0.395), vec4(-0.280, 0.105,-0.190, 0.015),
    vec4(-0.190, 0.395,-0.100, 0.305), vec4(-0.190, 0.015,-0.100, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    // glyph 13: 6
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4(-0.280, 0.305,-0.190, 0.395), vec4(-0.280, 0.105,-0.190, 0.015),
    vec4(-0.190, 0.395,-0.100, 0.305), vec4(-0.190, 0.015,-0.100, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    vec4(-0.280,-0.105,-0.190,-0.015), vec4(-0.280,-0.305,-0.190,-0.395),
    vec4(-0.190,-0.015,-0.100,-0.105), vec4(-0.190,-0.395,-0.100,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    // glyph 14: 7
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    // glyph 15: 8
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    vec4(-0.280,-0.105,-0.190,-0.015), vec4(-0.280,-0.305,-0.190,-0.395),
    vec4(-0.190,-0.015,-0.100,-0.105), vec4(-0.190,-0.395,-0.100,-0.305),
    vec4(-0.280, 0.305,-0.190, 0.395), vec4(-0.280, 0.105,-0.190, 0.015),
    vec4(-0.190, 0.395,-0.100, 0.305), vec4(-0.190, 0.015,-0.100, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    // glyph 16: 9
    vec4(-0.085, 0.500, 0.085, 0.500), vec4(-0.175, 0.410, 0.175, 0.410),
    vec4(-0.175, 0.410, 0.175, 0.410), vec4(-0.085, 0.320, 0.085, 0.320),
    vec4( 0.100, 0.305, 0.190, 0.395), vec4( 0.100, 0.105, 0.190, 0.015),
    vec4( 0.190, 0.395, 0.280, 0.305), vec4( 0.190, 0.015, 0.280, 0.105),
    vec4( 0.100,-0.105, 0.190,-0.015), vec4( 0.100,-0.305, 0.190,-0.395),
    vec4( 0.190,-0.015, 0.280,-0.105), vec4( 0.190,-0.395, 0.280,-0.305),
    vec4(-0.085,-0.320, 0.085,-0.320), vec4(-0.175,-0.410, 0.175,-0.410),
    vec4(-0.175,-0.410, 0.175,-0.410), vec4(-0.085,-0.500, 0.085,-0.500),
    vec4(-0.280, 0.305,-0.190, 0.395), vec4(-0.280, 0.105,-0.190, 0.015),
    vec4(-0.190, 0.395,-0.100, 0.305), vec4(-0.190, 0.015,-0.100, 0.105),
    vec4(-0.085, 0.090, 0.085, 0.090), vec4(-0.175, 0.000, 0.175, 0.000),
    vec4(-0.175, 0.000, 0.175, 0.000), vec4(-0.085,-0.090, 0.085,-0.090),
    // glyph 17: .
    vec4(-0.0425,-0.415,0.0425,-0.415), vec4(-0.0425,-0.500,0.0425,-0.500)
);

// Quad corners: 0=TL 1=TR 2=BL 3=BR  ->  triangles (TL,TR,BL) (TR,BR,BL)
const int CORNER_OF_SLOT[6] = int[](0, 1, 2, 1, 3, 2);

// Glyph id shown in text slot i for a packed version (-1 = nothing, e.g. absent leading digit)
// ver: byte 0 = major, byte 1 = minor, byte 2 = micro
int slotGlyph(int i, uint ver) {
    int k = i - VERSION_FIRST_SLOT;                    // 0..10 : d d d . d d d . d d d
    int part  = k < 3 ? 0 : (k < 7 ? 1 : 2);           // 0 major, 1 minor, 2 micro
    int digit = k - (part == 0 ? 0 : (part == 1 ? 4 : 8));   // 0 hundreds, 1 tens, 2 ones
    int v = int((ver >> uint(8 * part)) & 255u);
    int ones     = 7 + v % 10;
    int tens     = v >= 10  ? 7 + (v / 10) % 10 : -1;
    int hundreds = v >= 100 ? 7 + v / 100 : -1;
    int g = digit == 2 ? ones : (digit == 1 ? tens : hundreds);
    g = (k == 3 || k == 7) ? 17 : g;                   // dots
    return i < VERSION_FIRST_SLOT ? i : g;             // letters: glyph id == slot
}

// Position (in text space, before global scale) of vertex number vid.
// Text is left aligned: x = 0 is the left edge of the "S".
vec2 textVertex(int vid, float spacing, uint ver) {
    int quadIdx = vid / 6;
    int corner  = CORNER_OF_SLOT[vid - quadIdx * 6];

    int   t = 0, tAcc = 0, acc = 0;
    bool  found = false;
    float cursor = 0.0, left = 0.0;
    bool  first = true, versionStarted = false;
    for (int i = 0; i < TEXT_LEN; i++) {
        int  n    = TEXT_SLOT_QUADS[i];
        bool mine = !found && quadIdx < acc + n;
        int  gi   = slotGlyph(i, ver);
        bool present = gi >= 0;
        float gap = first ? 0.0 : ((i >= VERSION_FIRST_SLOT && !versionStarted) ? VERSION_LEAD_GAP : TEXT_GAP[i]);

        t     = mine ? i : t;
        tAcc  = mine ? acc : tAcc;
        left  = mine ? cursor + gap : left;             // only used when the slot is present
        found = found || mine;
        acc  += n;

        cursor += present ? gap + GLYPH_WIDTH[max(gi, 0)] * TEXT_SCALE[i] : 0.0;
        versionStarted = versionStarted || (present && i >= VERSION_FIRST_SLOT);
        first = first && !present;
    }

    int  g     = slotGlyph(t, ver);
    int  gs    = max(g, 0);
    int  local = quadIdx - tAcc;
    bool used  = g >= 0 && local < GLYPH_QUADS[gs];      // unused quad -> degenerate

    // Corner of the quad: 0/1 = top (xy/zw), 2/3 = bottom (xy/zw)
    int  q  = GLYPH_FIRST[gs] + min(local, GLYPH_QUADS[gs] - 1);
    vec4 qv = FONT_QUADS[q * 2 + (corner >> 1)];
    vec2 pos = (corner & 1) == 0 ? qv.xy : qv.zw;

    // Per-slot scale from the baseline (small caps, small digits)
    float s = TEXT_SCALE[t];
    pos.y = (pos.y + 0.5) * s - 0.5;
    pos.x *= s;
    pos.x += (left + 0.5 * GLYPH_WIDTH[gs] * s) * spacing;
    return used ? pos : vec2(0.0);
}
// ---- EMBEDDED GEOMETRY (end) ---------------------------------------------

void main() {
    int tv = VERTEX_ID - (VERTEX_ID / 3) * 3;
    vBarycentric = vec3(tv == 0 ? 1.0 : 0.0, tv == 1 ? 1.0 : 0.0, tv == 2 ? 1.0 : 0.0);

    vec2 pos = textVertex(VERTEX_ID, SPACING, uVersion);

    pos *= SCALE;
    pos.x /= ASPECT;
    pos += OFFSET;

    // NDC -> texel coordinates. The viewport maps NDC -1..1 onto the crop rectangle, so the
    // position inside the crop plus the crop origin is the texel of the framebuffer texture.
    vec2 uv = vec2(pos.x * 0.5 + 0.5, FB_Y_DOWN ? 0.5 - pos.y * 0.5 : pos.y * 0.5 + 0.5);
    vFbCoord = uv * vec2(float(uCropWidth), float(uCropHeight)) + vec2(float(uCropX), float(uCropY));

    // OpenGL/NVN clip space Y points up, Vulkan's points down.
#ifdef VULKAN
    gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);
#else
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
#endif
}
