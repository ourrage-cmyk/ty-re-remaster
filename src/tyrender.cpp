// tyrender.dll Γò¼├┤Γö£├ºΓö£Γòó TY HD renderer hook layer.
// Stage 1 (this file): GL call logger / frame anatomy capture + shader source dump & override.
// Loading: injected by TYLauncher.exe (see RESEARCH\sessions\2026-10-11_session3\decisions\D2_dll_loading_and_hooking.md).
// Hooking: TY.exe IAT entries (OPENGL32 / GDI32!SwapBuffers) + pointers handed out by wglGetProcAddress. No inline hooks.
// Output (game dir): tyrender.log, tyrender\dump\<hash>.<vs|fs>.glsl ; overrides read from tyrender\override\<hash>.glsl
// Config (game dir): tyrender.ini  [debug] CaptureFrame=N (0=off)  CaptureFrame2=N  DumpShaders=1  OverrideShaders=1
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include <vector>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <set>

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef unsigned char GLboolean, GLubyte;
typedef float GLfloat;
typedef char GLchar;
typedef void GLvoid;
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_VENDOR 0x1F00
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31

// ---------------------------------------------------------------- config / log
static char g_dir[MAX_PATH];          // game dir with trailing backslash
static FILE* g_log;
static int g_capFrame1, g_capFrame2, g_dumpShaders = 1, g_overrideShaders = 1;
static unsigned g_frame;              // frames presented so far
static bool g_cap;                    // capturing current frame
static unsigned g_draws;
static LARGE_INTEGER g_qpf, g_lastReport;

static void logf(const char* fmt, ...) {
    if (!g_log) return;
    va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
    fputc('\n', g_log);
    if (!g_cap) fflush(g_log);
}
#define CAP(...) do { if (g_cap) logf(__VA_ARGS__); } while (0)

static void LoadConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    g_capFrame1 = GetPrivateProfileIntA("debug", "CaptureFrame", 0, ini);
    g_capFrame2 = GetPrivateProfileIntA("debug", "CaptureFrame2", 0, ini);
    g_dumpShaders = GetPrivateProfileIntA("debug", "DumpShaders", 1, ini);
    g_overrideShaders = GetPrivateProfileIntA("debug", "OverrideShaders", 1, ini);
    char env[32];
    if (GetEnvironmentVariableA("TYRENDER_CAPTURE", env, sizeof env)) g_capFrame1 = atoi(env);
}

// ---------------------------------------------------------------- state shared with ao.inl
static GLuint g_curProg, g_curFBO;
static std::set<uint64_t> g_vpLocs;                 // (prog<<32 | loc) of uniforms named "VPMatrix"
static GLuint g_colorCopyTex; static GLenum g_colorCopyFmt; static int g_colorCopyW, g_colorCopyH;   // engine's last full-screen colour copy
static void RunAO(GLuint depthTex, int w, int h);
static int g_aoExp;
static bool TrackVP(GLint loc, GLboolean transpose, const GLfloat* v, float* colMajor);
static float g_jitterX, g_jitterY; static bool g_jitterOn;   // TAA sub-pixel jitter in NDC units (fx.inl)
static void PollAOFlags();
static void LoadAOConfig();
static void LoadFXConfig();
static void RunFX(int w, int h);
static unsigned g_sceneDepthFrame = ~0u, g_fxDoneFrame = ~0u; static bool g_fxOffFile;
static int g_screenW, g_screenH; static GLuint g_sceneDepthTex; static bool g_sceneModified;
static std::unordered_map<uint64_t, int> g_namedLocs;   // (prog<<32|loc) -> 1 shadowVPMatrix1, 2 shadowVPMatrix2, 3 shadowDir, 4 fogColour, 5 shadowTexture1, 6 shadowTexture2
static int g_aoDebug, g_giDebug;   // debug view selectors (set by flag files, see ao.inl / gi.inl)
static void AfterLink(GLuint p);
// characters: origins of skinned draws (modelMatrix translation) this frame, used by GI to keep characters from bleeding colour (gi.inl)
static float g_chars[8][3]; static int g_nChars, g_nCharsPrev; static unsigned g_charsFrame; static std::set<GLuint> g_skinProgs; static std::unordered_map<GLuint, GLint> g_modelLocOf;
static void AddChar(const GLfloat* v, GLboolean t) {
    if (g_charsFrame != g_frame) { g_nCharsPrev = g_nChars; g_nChars = 0; g_charsFrame = g_frame; }
    float p[3] = {t ? v[3] : v[12], t ? v[7] : v[13], t ? v[11] : v[14]};
    for (int i = 0; i < g_nChars; i++) if (fabsf(g_chars[i][0] - p[0]) + fabsf(g_chars[i][1] - p[1]) + fabsf(g_chars[i][2] - p[2]) < 20.0f) return;
    if (g_nChars < 8) { memcpy(g_chars[g_nChars], p, sizeof p); g_nChars++; }
}
static float g_fogC[4] = {0, 0, 1e9f, 0}; static bool g_haveFogC; static float g_progFog[8192][4]; static GLint g_progFogLoc1[8192]; static float g_lateFog = 1.0f;   // per program: last engine fogCoords (after override) + location+1; [fog] LateBoost   // last engine fogCoords (after override): AO fades with the same fog fraction
static float g_fogDistance = 1.0f, g_fogAmount = 1.0f;   // [fog] Distance / Amount: scale the engine fog (fogCoords) and the volumetric fog
static bool GIMaskBegin(); static void GIMaskEnd(); static void RunGI(GLuint depthTex, int w, int h); static void LoadGIConfig(); static void PollGIFlags();
static int g_pendingShadowUnit[2] = {-1, -1}; static GLuint g_shadowTex[2];   // engine cascade shadow maps (resolved at draw time)
static void GProf(int m); static void GProfNextFrame();
static void FogDiagDraw(); static bool LateFogBegin(); static void LateFogEnd();
static void RunAtmos(GLuint depthTex, int w, int h); static void LoadAtmosConfig(); static void PollAtmosFlags();
static void OnNamedUniformMat(int id, const GLfloat* v, GLboolean t); static void OnNamedUniform4(int id, const GLfloat* v);
static bool TessRewriteVS(GLuint shader, std::string& src); static void TessOnAttach(GLuint p, GLuint s); static void TessLink(GLuint p); static void LoadTessConfig();
static void LoadLightConfig(); static bool LightRewriteFS(std::string& s); static void LightOnLink(GLuint p); static void LightApply(GLuint p); static void EnsureLtDecl(std::string& s); static bool DebugCamApply(float* m); static void PollDebugCam(); static void MotionPoll(); static void MotionFrame(HDC dc);
static void LoadPbrConfig(); static bool PbrRewriteVS(std::string& s); static bool PbrRewriteFS(std::string& s); static void PbrOnLink(GLuint p); static void PbrApply(GLuint p); static void PbrUpdate();
static void TexOnUpload(GLuint id, int lvl, GLenum ifmt, int w, int h, const void* px, int bytes); static void LoadTexTags(); static void TexReport(); static void TexDrawPrep(); static void SpillLogDraw(); static int g_progDiffUnit[8192];
static bool DecalRewriteFS(std::string& s); static void TexOnLink(GLuint p);
static bool g_curProgTess; static unsigned g_tessBadDrawsExt; static bool IsTessProg(GLuint p); static void TessPatchMode();

// ---------------------------------------------------------------- IAT patching
static bool PatchIAT(HMODULE mod, const char* dll, const char* fn, void* hook, void** orig) {
    auto base = (BYTE*)mod;
    auto nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); d->Name; d++) {
        if (_stricmp((char*)(base + d->Name), dll)) continue;
        auto names = (IMAGE_THUNK_DATA*)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        auto iat = (IMAGE_THUNK_DATA*)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto ibn = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp((char*)ibn->Name, fn)) continue;
            DWORD old; VirtualProtect(&iat->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            *orig = (void*)iat->u1.Function;
            iat->u1.Function = (ULONG_PTR)hook;
            VirtualProtect(&iat->u1.Function, sizeof(void*), old, &old);
            return true;
        }
    }
    logf("[iat] NOT FOUND %s!%s", dll, fn);
    return false;
}

// ---------------------------------------------------------------- shader dump / override
static uint32_t Fnv1a(const std::string& s) { uint32_t h = 2166136261u; for (unsigned char c : s) { h ^= c; h *= 16777619u; } return h; }
static std::unordered_map<GLuint, GLenum> g_shaderType;
static bool ReadFileStr(const char* path, std::string& out) {
    FILE* f = fopen(path, "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? n : 0); if (n > 0) fread(&out[0], 1, n, f); fclose(f); return true;
}

// ---------------------------------------------------------------- shadow upgrade (source patch of standard/water/grass fragment shaders)
// Stock: cascade 1 = 2x2 or 4x4 box PCF with hard-coded 1/2048 texel, cascades 2/3 = single tap, shadows darken at most 50%.
// Patched: rotated Poisson PCF (per-pixel random rotation -> smooth with TAA) sized from the real map resolution, all cascades,
// strength configurable; shadow maps can be enlarged (MapScale) because the texel size is no longer hard-coded.
static int g_shPatch = 1, g_shMapScale = 2, g_shTaps = 16, g_shTemporal = 0, g_shGrid = 1;   // Temporal: 0 = stable per-pixel rotation, 1 = 8-frame cycle matching the TAA jitter
 static float g_shRadius = 1.4f, g_shStrength = 0.5f, g_shGrassStrength = 0.35f;
static std::set<GLuint> g_grassVS;                        // vertex shaders of the grass template (pushOutSpheres / grassTextureUVOffset)
static std::unordered_map<GLuint, GLint> g_shadowStrengthLoc; // program -> location of tyShadowStrength
static void LoadShadowConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_shPatch = GetPrivateProfileIntA("shadows", "Patch", 1, ini); g_shTemporal = GetPrivateProfileIntA("shadows", "Temporal", 0, ini); g_shGrid = GetPrivateProfileIntA("shadows", "Grid", 1, ini);
    g_shMapScale = GetPrivateProfileIntA("shadows", "MapScale", 2, ini); if (g_shMapScale < 1) g_shMapScale = 1; if (g_shMapScale > 4) g_shMapScale = 4;
    g_shTaps = GetPrivateProfileIntA("shadows", "Taps", 16, ini); if (g_shTaps < 4) g_shTaps = 4; if (g_shTaps > 32) g_shTaps = 32;
    GetPrivateProfileStringA("shadows", "Radius", "1.4", b, sizeof b, ini); g_shRadius = (float)atof(b);
    GetPrivateProfileStringA("shadows", "Strength", "0.5", b, sizeof b, ini); g_shStrength = (float)atof(b);
    GetPrivateProfileStringA("shadows", "GrassStrength", "0.35", b, sizeof b, ini); g_shGrassStrength = (float)atof(b);
}
static int ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
    int n = 0; size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); n++; }
    return n;
}
// shared per-program parameter block (set per program per frame by LightApply in light.inl):
//  tyLt[0] = shadow filter (x radius in map texels, y temporal noise offset, z edge taps, w probe radius factor)
//  tyLt[1] = shadow tint rgb (luminance 1), w tint amount     tyLt[2] = hemisphere sky rgb, w hemisphere amount
//  tyLt[3] = hemisphere ground rgb, w shadow resaturation     tyLt[4] = x foliage translucency, y static-mesh hemisphere scale
static void EnsureLtDecl(std::string& s) {
    if (s.find("uniform vec4 tyLt[") != std::string::npos) return;
    size_t m = s.rfind("void main"); if (m != std::string::npos) s.insert(m, "uniform vec4 tyLt[8];\n");
}
static bool PbrDefine(const std::string& s, const char* name, int val);
static bool PatchShadowShader(std::string& s) {
    if (s.find("sampler2DShadow shadowTexture1") == std::string::npos) return false;
    size_t mainPos = s.rfind("void main");
    if (mainPos == std::string::npos) return false;
    int n = 0;
    // cascade-1 box loops -> one iteration, PCF inside TyPCF
    n += ReplaceAll(s, "for (y = -1.5; y <= 1.5; y += 1.0)", "for (y = 0.0; y <= 0.0; y += 1.0)");
    n += ReplaceAll(s, "for (x = -1.5; x <= 1.5; x += 1.0)", "for (x = 0.0; x <= 0.0; x += 1.0)");
    n += ReplaceAll(s, "for (y = -0.5; y <= 0.5; y += 1.0)", "for (y = 0.0; y <= 0.0; y += 1.0)");
    n += ReplaceAll(s, "for (x = -0.5; x <= 0.5; x += 1.0)", "for (x = 0.0; x <= 0.0; x += 1.0)");
    n += ReplaceAll(s, "shadowCoeff1 = sum / 16.0;", "shadowCoeff1 = sum;");
    n += ReplaceAll(s, "shadowCoeff1 = sum / 4.0;", "shadowCoeff1 = sum;");
    n += ReplaceAll(s, "SHADOWTEXTURE(shadowTexture1, sampleUV)", "TyPCF(shadowTexture1, sampleUV)");
    n += ReplaceAll(s, "SHADOWTEXTURE(shadowTexture1, v_shadowCoord1)", "TyPCF(shadowTexture1, v_shadowCoord1)");
    n += ReplaceAll(s, "SHADOWTEXTURE(shadowTexture2, v_shadowCoord2)", "TyPCF(shadowTexture2, v_shadowCoord2)");
    n += ReplaceAll(s, "SHADOWTEXTURE(shadowTexture3, v_shadowCoord3)", "TyPCF(shadowTexture3, v_shadowCoord3)");
    n += ReplaceAll(s, "shadowAmount * 0.5", "shadowAmount * tyShadowStrength");   // set per program after link (grass gets its own value)
    if (!n) return false;
    // Filter: 4-tap probe (rotated cross, bilinear compare = 16 depth fetches). Fully lit / fully shadowed neighbourhoods (the vast majority of pixels) stop there.
    // Edge pixels take Vogel-spiral taps with a per-pixel interleaved-gradient rotation that changes every frame (TAA integrates it -> no stipple).
    EnsureLtDecl(s);
    s.insert(s.rfind("void main"),
        "uniform float tyShadowStrength;\n"
        // explicit LOD 0 (R5 root cause of S1): TyPCF runs in NON-UNIFORM control flow (engine cascade branch + our early-out), so implicit
        // derivatives are undefined in 2x2 quads where some pixels returned early -> the GPU picks a wrong LOD/filter for those taps and
        // writes isolated half-lit pixels: the dotted bright line inside shadows, parallel to the edge at the probe radius.
        "float TyS(sampler2DShadow t, vec3 c, vec2 zg, vec2 o) { vec3 p = vec3(c.xy + o, c.z + dot(zg, o)); return textureLod(t, p, 0.0); }\n"
        "float TyPCF(sampler2DShadow t, vec3 c) {\n"
        "  vec2 texel = tyLt[0].x / vec2(textureSize(t, 0));\n"
        "  vec2 zg = vec2(0.0);   // receiver-plane depth gradient (depth per shadow-map uv): offset taps compare against the receiver plane, not the centre depth\n"
        "  if (tyLt[6].x > 0.5) { vec3 ddx = dFdx(c), ddy = dFdy(c); float det = ddx.x * ddy.y - ddx.y * ddy.x;\n"
        "    if (abs(det) > 1e-14) { zg = vec2(ddy.y * ddx.z - ddx.y * ddy.z, ddx.x * ddy.z - ddy.x * ddx.z) / det;\n"
        "      c.z -= min(tyLt[6].w * (abs(zg.x) + abs(zg.y)) / float(textureSize(t, 0).x), tyLt[6].z);   // slope-scaled bias: HW PCF compares every texel against ONE reference depth, error = slope * texel\n"
        "      float gl = length(zg) * texel.x * 2.0; if (gl > tyLt[6].z) zg *= tyLt[6].z / gl; } }\n"
        "  float a = 6.2831853 * fract(52.9829189 * fract(dot(gl_FragCoord.xy + vec2(tyLt[0].y), vec2(0.06711056, 0.00583715))));\n"
        "  vec2 d = vec2(cos(a), sin(a)) * (texel * tyLt[0].w); vec2 e = vec2(-d.y, d.x);\n"
        "  if (tyLt[4].z < 0.5) {   // stable mode: fixed binomial grid of bilinear compares, no noise at all\n"
        "    // early-out probe must cover the whole filter footprint (diagonals + axes), otherwise pixels flip between early-out and filter along a thin dotted line parallel to the shadow edge\n"
        "    vec2 g = texel * 1.7; vec2 h = texel * 1.9;\n"
        "    float p1 = TyS(t, c, zg, g), p2 = TyS(t, c, zg, -g), p3 = TyS(t, c, zg, vec2(g.x, -g.y)), p4 = TyS(t, c, zg, vec2(-g.x, g.y));\n"
        "    float q1 = TyS(t, c, zg, vec2(h.x, 0.0)), q2 = TyS(t, c, zg, vec2(-h.x, 0.0)), q3 = TyS(t, c, zg, vec2(0.0, h.y)), q4 = TyS(t, c, zg, vec2(0.0, -h.y));\n"
        "    float s0 = p1 + p2 + p3 + p4 + q1 + q2 + q3 + q4;\n"
        "    if (s0 < 0.001 || s0 > 7.999) return s0 * 0.125;\n"
        "    // crack closing (R5, S1): occluder on BOTH opposite sides within ~2 texels = the pixel sees light through a 1-3 texel gap in the caster\n"
        "    // (seams between character mesh pieces) -> a dotted bright line inside the shadow. Morphological closing: keep it shadowed.\n"
        "    float cl = tyLt[4].w * max(max(min(p1, p2), min(p3, p4)), max(min(q1, q2), min(q3, q4)));\n"
        "    float acc = 0.0;\n"
        "    if (tyLt[0].z < 24.0) {   // cheaper 3x3 binomial (Medium)\n"
        "      const float W3[3] = float[3](0.25, 0.5, 0.25);\n"
        "      for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) acc += W3[i] * W3[j] * TyS(t, c, zg, vec2(float(i - 1), float(j - 1)) * (texel * 1.15));\n"
        "    } else {\n"
        "      const float W[4] = float[4](0.125, 0.375, 0.375, 0.125), O[4] = float[4](-1.5, -0.5, 0.5, 1.5);\n"
        "      for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) acc += W[i] * W[j] * TyS(t, c, zg, vec2(O[i], O[j]) * texel);\n"
        "    }\n"
        "    return max(acc, cl);\n"
        "  }\n"
        "  float sum = TyS(t, c, zg, d) + TyS(t, c, zg, -d) + TyS(t, c, zg, e) + TyS(t, c, zg, -e);\n"
        "  if (sum < 0.001 || sum > 3.999) return sum * 0.25;\n"
        "  int N = int(tyLt[0].z); float invN = 1.0 / float(N);\n"
        "  float b = a + 0.7853982; vec2 q = vec2(cos(b), sin(b));\n"
        "  const vec2 GA = vec2(-0.7373688, 0.6754903);   // golden angle rotation\n"
        "  for (int i = 0; i < N; i++) {\n"
        "    sum += TyS(t, c, zg, q * sqrt((float(i) + 0.5) * invN) * texel);\n"
        "    q = vec2(q.x * GA.x - q.y * GA.y, q.x * GA.y + q.y * GA.x);\n"
        "  }\n"
        "  return sum / (float(N) + 4.0);\n"
        "}\n");
    if (PbrDefine(s, "SKIN", 1)) { ReplaceAll(s, "if (tyLt[6].x > 0.5) {", "c.z -= tyLt[7].x; if (false) {"); }   // skinned characters: screen derivatives of tiny skinned triangles are noise (hatching) -> no receiver-plane bias there
    return true;
}

// ---------------------------------------------------------------- GL hooks (core GL 1.1 via IAT)
#define HOOK(ret, name, params) typedef ret (WINAPI* name##_t) params; static name##_t o_##name; static ret WINAPI h_##name params

HOOK(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h)) {
    // first 64x64 viewport after the scene depth copy = adaptive-brightness chain starts: 3D image done, HUD not drawn yet -> final post (fx.inl)
    if (w == 64 && h == 64 && g_sceneDepthFrame == g_frame && g_fxDoneFrame != g_frame) { g_fxDoneFrame = g_frame; GProf(7); RunFX(g_screenW, g_screenH); GProf(8); }
    if (g_shPatch && g_shMapScale > 1 && g_curFBO != 0 && w == 2048 && h == 2048) { w *= g_shMapScale; h *= g_shMapScale; }   // shadow pass into enlarged map
    CAP("glViewport %d %d %d %d", x, y, w, h); o_glViewport(x, y, w, h);
}
static GLenum g_blendS = 1, g_blendD = 0;   // last glBlendFunc (#9 spill log: additive draws)
static unsigned g_activeUnit; static GLuint g_bound2D[32];   // texture id bound to GL_TEXTURE_2D per unit (to name RT allocations)
HOOK(void, glBindTexture, (GLenum t, GLuint tex)) { if (t == 0x0DE1 && g_activeUnit < 32) g_bound2D[g_activeUnit] = tex; CAP("glBindTexture %#x %u", t, tex); o_glBindTexture(t, tex); }
HOOK(void, glClear, (GLbitfield m)) { CAP("glClear %#x", m); o_glClear(m); }
static unsigned g_stencilEnables; HOOK(void, glEnable, (GLenum c)) { if (c == 0x0B90) g_stencilEnables++; CAP("glEnable %#x", c); o_glEnable(c); }
HOOK(void, glDisable, (GLenum c)) { CAP("glDisable %#x", c); o_glDisable(c); }
HOOK(void, glBlendFunc, (GLenum s, GLenum d)) { CAP("glBlendFunc %#x %#x", s, d); g_blendS = s; g_blendD = d; o_glBlendFunc(s, d); }
HOOK(void, glDepthMask, (GLboolean f)) { CAP("glDepthMask %d", f); o_glDepthMask(f); }
HOOK(void, glColorMask, (GLboolean r, GLboolean g, GLboolean b, GLboolean a)) { CAP("glColorMask %d%d%d%d", r, g, b, a); o_glColorMask(r, g, b, a); }
HOOK(void, glDrawElements, (GLenum m, GLsizei n, GLenum t, const GLvoid* idx)) { g_draws++; TexDrawPrep(); SpillLogDraw(); FogDiagDraw(); bool lateFog = LateFogBegin();
    if (g_curProgTess) { if (m != 4) { g_tessBadDrawsExt++; return; } TessPatchMode(); m = 0x000E; }   // tessellated programs only accept GL_PATCHES
    for (int k = 0; k < 2; k++) if (g_pendingShadowUnit[k] >= 0) { if (g_pendingShadowUnit[k] < 32) g_shadowTex[k] = g_bound2D[g_pendingShadowUnit[k]]; g_pendingShadowUnit[k] = -1; } CAP("glDrawElements %#x n=%d", m, n); o_glDrawElements(m, n, t, idx); if (lateFog) LateFogEnd(); if (GIMaskBegin()) { o_glDrawElements(m, n, t, idx); GIMaskEnd(); } }
HOOK(void, glDrawArrays, (GLenum m, GLint f, GLsizei n)) { g_draws++; TexDrawPrep();
    if (g_curProgTess) { if (m != 4) { g_tessBadDrawsExt++; return; } TessPatchMode(); m = 0x000E; } CAP("glDrawArrays %#x first=%d n=%d", m, f, n); o_glDrawArrays(m, f, n); if (GIMaskBegin()) { o_glDrawArrays(m, f, n); GIMaskEnd(); } }
HOOK(void, glTexImage2D, (GLenum t, GLint lvl, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum fmt, GLenum ty, const GLvoid* px)) {
    // enlarge the 2048^2 cascade shadow maps (texel size is read from textureSize() in patched shaders)
    if (g_shPatch && g_shMapScale > 1 && !px && ifmt == 0x81A7 && w == 2048 && h == 2048) { w *= g_shMapScale; h *= g_shMapScale; logf("[shadows] shadow map enlarged to %dx%d", w, h); }
    // always log render-target-like allocations (no pixel data) so we see RT sizes at init
    if (g_cap || !px) logf("glTexImage2D tex=%u t=%#x lvl=%d ifmt=%#x %dx%d fmt=%#x type=%#x data=%s frame=%u", g_activeUnit < 32 ? g_bound2D[g_activeUnit] : 0, t, lvl, ifmt, w, h, fmt, ty, px ? "yes" : "null", g_frame);
    o_glTexImage2D(t, lvl, ifmt, w, h, b, fmt, ty, px);
    if (px && t == 0x0DE1 && g_activeUnit < 32) TexOnUpload(g_bound2D[g_activeUnit], lvl, ifmt, w, h, px, 0);
}
HOOK(void, glCopyTexImage2D, (GLenum t, GLint lvl, GLenum ifmt, GLint x, GLint y, GLsizei w, GLsizei h, GLint b)) {
    if (g_cap || g_frame < 3) logf("glCopyTexImage2D t=%#x ifmt=%#x src=%d,%d %dx%d frame=%u", t, ifmt, x, y, w, h, g_frame);
    o_glCopyTexImage2D(t, lvl, ifmt, x, y, w, h, b);
    // engine grabs scene colour then depth from FB0 right before bloom (F3 anatomy) -> inject AO after the depth copy
    if (t == 0x0DE1 && lvl == 0 && g_curFBO == 0 && g_activeUnit < 32) {
        if (ifmt == 0x1907 || ifmt == 0x1908) { g_colorCopyTex = g_bound2D[g_activeUnit]; g_colorCopyFmt = ifmt; g_colorCopyW = w; g_colorCopyH = h; }
        else if (ifmt == 0x1902 || ifmt == 0x81A5 || ifmt == 0x81A6 || ifmt == 0x81A7) {
            g_sceneDepthFrame = g_frame; g_screenW = w; g_screenH = h; g_sceneDepthTex = g_bound2D[g_activeUnit];
            g_sceneModified = false;
            PbrUpdate();
            GProf(1); RunAO(g_bound2D[g_activeUnit], w, h); GProf(2);
            GProf(3); if (!g_aoDebug) RunGI(g_bound2D[g_activeUnit], w, h); GProf(4);            // debug views are exclusive: nothing layered on top
            GProf(5); if (!g_aoDebug && !g_giDebug) RunAtmos(g_bound2D[g_activeUnit], w, h); GProf(6);
            if (g_sceneModified && g_colorCopyTex && !(g_aoExp & 1)) {   // engine capture texture must include our changes (bloom reads it)
                GLuint keep = g_bound2D[g_activeUnit];
                GProf(18);
                o_glBindTexture(0x0DE1, g_colorCopyTex); o_glCopyTexImage2D(0x0DE1, 0, g_colorCopyFmt, 0, 0, g_colorCopyW, g_colorCopyH, 0);
                GProf(19);
                o_glBindTexture(0x0DE1, keep);
            }
        }
    }
}
// Engine reads a 16x16 depth patch around the sun each frame (lens-flare occlusion, PBO readback). When that rectangle leaves the
// window (sun near the screen edge) the Intel driver crashes inside glReadPixels (verified 2026-10-11, crash at igxelpicd32+0x1a88cc,
// also with all tyrender features off). Out-of-bounds reads are skipped; the PBO keeps last frame's data (flare fades naturally).
static unsigned g_readSkips;
HOOK(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, GLvoid* p)) {
    CAP("glReadPixels %d,%d %dx%d fmt=%#x", x, y, w, h, f);
    if (g_curFBO == 0 && g_screenW > 0 && (x < 0 || y < 0 || x + w > g_screenW || y + h > g_screenH)) {
        if (g_readSkips++ < 20) logf("[fix] skipped out-of-bounds glReadPixels %d,%d %dx%d fmt=%#x (window %dx%d) frame=%u", x, y, w, h, f, g_screenW, g_screenH, g_frame);
        return;
    }
    o_glReadPixels(x, y, w, h, f, t, p);
}
HOOK(const GLubyte*, glGetString, (GLenum n)) { return o_glGetString(n); }

// ---------------------------------------------------------------- GL hooks (extensions via wglGetProcAddress)
HOOK(void, glBindFramebuffer, (GLenum t, GLuint fb)) { if (t == 0x8D40 || t == 0x8CA9) g_curFBO = fb; CAP("glBindFramebuffer %#x %u", t, fb); o_glBindFramebuffer(t, fb); }
HOOK(void, glFramebufferTexture2D, (GLenum t, GLenum att, GLenum tt, GLuint tex, GLint lvl)) {
    if (g_cap || g_frame < 3) logf("glFramebufferTexture2D %#x att=%#x textarget=%#x tex=%u lvl=%d frame=%u", t, att, tt, tex, lvl, g_frame); o_glFramebufferTexture2D(t, att, tt, tex, lvl); }
HOOK(void, glFramebufferRenderbuffer, (GLenum t, GLenum att, GLenum rt, GLuint rb)) {
    logf("glFramebufferRenderbuffer %#x att=%#x rb=%u frame=%u", t, att, rb, g_frame); o_glFramebufferRenderbuffer(t, att, rt, rb); }
HOOK(void, glRenderbufferStorage, (GLenum t, GLenum f, GLsizei w, GLsizei h)) { logf("glRenderbufferStorage fmt=%#x %dx%d", f, w, h); o_glRenderbufferStorage(t, f, w, h); }
HOOK(void, glRenderbufferStorageMultisample, (GLenum t, GLsizei s, GLenum f, GLsizei w, GLsizei h)) {
    logf("glRenderbufferStorageMultisample samples=%d fmt=%#x %dx%d", s, f, w, h); o_glRenderbufferStorageMultisample(t, s, f, w, h); }
HOOK(void, glBlitFramebuffer, (GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield m, GLenum fl)) {
    CAP("glBlitFramebuffer %d,%d,%d,%d -> %d,%d,%d,%d mask=%#x filter=%#x", a, b, c, d, e, f, g, h, m, fl); o_glBlitFramebuffer(a, b, c, d, e, f, g, h, m, fl); }
HOOK(void, glUseProgram, (GLuint p)) { g_curProg = p; g_curProgTess = IsTessProg(p); PbrApply(p); LightApply(p); CAP("glUseProgram %u", p); o_glUseProgram(p); }
HOOK(void, glActiveTexture, (GLenum u)) { g_activeUnit = u - 0x84C0; CAP("glActiveTexture %#x", u); o_glActiveTexture(u); }
HOOK(GLuint, glCreateShader, (GLenum type)) { GLuint s = o_glCreateShader(type); g_shaderType[s] = type; return s; }
HOOK(void, glAttachShader, (GLuint p, GLuint s)) { logf("glAttachShader prog=%u shader=%u", p, s); TessOnAttach(p, s); o_glAttachShader(p, s); }
HOOK(void, glLinkProgram, (GLuint p)) { logf("glLinkProgram %u", p); TessLink(p); AfterLink(p); }
HOOK(GLint, glGetUniformLocation, (GLuint p, const GLchar* n)) {
    GLint l = o_glGetUniformLocation(p, n);
    static std::set<std::pair<GLuint, std::string>> seen;
    if (seen.insert({p, n}).second) logf("uniform prog=%u '%s' -> %d", p, n, l);
    if (l >= 0 && !strncmp(n, "boneMats", 8)) g_skinProgs.insert(p);
    if (l >= 0 && !strcmp(n, "modelMatrix")) g_modelLocOf[p] = l;
    if (l >= 0 && !strcmp(n, "VPMatrix")) g_vpLocs.insert(((uint64_t)p << 32) | (uint32_t)l);
    static const char* names[] = {"", "shadowVPMatrix1", "shadowVPMatrix2", "shadowDir", "fogColour", "shadowTexture1", "shadowTexture2", "fogCoords", "diffuseTexture"};
    if (l >= 0) for (int i = 1; i < 9; i++) if (!strcmp(n, names[i])) g_namedLocs[((uint64_t)p << 32) | (uint32_t)l] = i;
    return l;
}
HOOK(void, glUniformMatrix4fv, (GLint l, GLsizei c, GLboolean t, const GLfloat* v)) {
    CAP("glUniformMatrix4fv loc=%d count=%d", l, c);
    if (c == 1 && g_curFBO == 0 && !g_skinProgs.empty() && g_skinProgs.count(g_curProg)) { auto mi = g_modelLocOf.find(g_curProg); if (mi != g_modelLocOf.end() && mi->second == l) AddChar(v, t); }
    float m[16];
    if (c == 1 && g_curFBO == 0 && !g_namedLocs.empty()) { auto it = g_namedLocs.find(((uint64_t)g_curProg << 32) | (uint32_t)l); if (it != g_namedLocs.end()) OnNamedUniformMat(it->second, v, t); }
    if (c == 1 && TrackVP(l, t, v, m)) {
        bool dc = DebugCamApply(m);   // debug free camera (tyrender\cam.txt), modifies m and g_sceneVP
        if (g_jitterOn) {
            // TAA: shift clip xy by jitter*w  (row0 += jx*row3, row1 += jy*row3), column-major
            for (int k = 0; k < 4; k++) { m[k * 4 + 0] += g_jitterX * m[k * 4 + 3]; m[k * 4 + 1] += g_jitterY * m[k * 4 + 3]; }
        }
        if (g_jitterOn || dc) { o_glUniformMatrix4fv(l, 1, 0, m); return; }
    }
    o_glUniformMatrix4fv(l, c, t, v);
}
static bool TestCompile(GLenum type, const std::string& src);   // defined after wglGetProcAddress hook

HOOK(void, glShaderSource, (GLuint s, GLsizei count, const GLchar* const* str, const GLint* len)) {
    std::string src;
    for (GLsizei i = 0; i < count; i++) src.append(str[i], (len && len[i] >= 0) ? (size_t)len[i] : strlen(str[i]));
    uint32_t h = Fnv1a(src);
    GLenum type = g_shaderType.count(s) ? g_shaderType[s] : 0;
    const char* ext = type == GL_VERTEX_SHADER ? "vs" : type == GL_FRAGMENT_SHADER ? "fs" : "xs";
    char path[MAX_PATH];
    if (g_dumpShaders) {
        snprintf(path, sizeof path, "%styrender\\dump\\%08x.%s.glsl", g_dir, h, ext);
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) { FILE* f = fopen(path, "wb"); if (f) { fwrite(src.data(), 1, src.size(), f); fclose(f); } }
    }
    std::string ov;
    snprintf(path, sizeof path, "%styrender\\override\\%08x.glsl", g_dir, h);
    bool useOv = g_overrideShaders && ReadFileStr(path, ov);
    if (type == GL_VERTEX_SHADER && (src.find("pushOutSpheres") != std::string::npos || src.find("grassTextureUVOffset") != std::string::npos)) g_grassVS.insert(s);
    if (!useOv && type == GL_VERTEX_SHADER) {
        std::string p = src;
        bool pb = PbrRewriteVS(p);
        if (pb && !TestCompile(type, p)) { p = src; pb = false; logf("[pbr] VS %08x rewrite failed to compile", h); }
        bool ts = TessRewriteVS(s, p);
        if (ts) logf("glShaderSource shader=%u hash=%08x [TESS VS]", s, h);
        if (pb) logf("glShaderSource shader=%u hash=%08x [PBR VS]", s, h);
        if (ts || pb) { ov = p; useOv = true; }
    }
    if (!useOv && type == GL_FRAGMENT_SHADER) {   // order: PBR (normals/spec) -> shadow patch (PCF, strength) -> light model; each stage is optional
        std::string p = src;
        bool pb = PbrRewriteFS(p), sh = g_shPatch && PatchShadowShader(p), lt = LightRewriteFS(p), dc = DecalRewriteFS(p);   // decal last: it rewrites the stock output line
        if (pb || sh || lt || dc) {
            if (TestCompile(type, p)) { ov = p; useOv = true; logf("glShaderSource shader=%u hash=%08x [FS:%s%s%s%s]", s, h, pb ? " PBR" : "", sh ? " SHADOW" : "", lt ? " LIGHT" : "", dc ? " DECAL" : ""); }
            else {
                logf("[fs] %08x combined rewrite failed to compile (pbr=%d shadow=%d light=%d) -> trying shadow patch alone", h, pb, sh, lt);
                std::string q = src; if (g_shPatch && PatchShadowShader(q) && TestCompile(type, q)) { ov = q; useOv = true; }
            }
        }
    }
    // any replacement must compile, otherwise the engine int3-crashes: fall back to the original source
    if (useOv && !TestCompile(type, ov)) { logf("override for %08x does not compile -> original used", h); useOv = false; }
    logf("glShaderSource shader=%u type=%s hash=%08x len=%u%s", s, ext, h, (unsigned)src.size(), useOv ? " [OVERRIDE]" : "");
    if (useOv) { const GLchar* p = ov.c_str(); o_glShaderSource(s, 1, &p, nullptr); }
    else o_glShaderSource(s, count, str, len);
}

static void FogOverride(GLfloat* f) {   // engine fog: v_fog = x + clamp((viewDepth - z) * w) * y
    f[2] *= g_fogDistance; if (g_fogDistance > 0) f[3] /= g_fogDistance;
    f[1] = fminf(f[1] * g_fogAmount, 1.0f - f[0]);
    if (f[1] > 0.0f && f[3] > 0.0f) { memcpy(g_fogC, f, sizeof g_fogC); g_haveFogC = true; }
}
HOOK(void, glUniform4fv, (GLint l, GLsizei c, const GLfloat* v)) {
    if (c == 1 && !g_namedLocs.empty()) {
        auto it = g_namedLocs.find(((uint64_t)g_curProg << 32) | (uint32_t)l);
        if (it != g_namedLocs.end()) {
            if (it->second == 7) { GLfloat f[4] = {v[0], v[1], v[2], v[3]}; FogOverride(f); if (g_curProg < 8192) { memcpy(g_progFog[g_curProg], f, sizeof f); g_progFogLoc1[g_curProg] = l + 1; } o_glUniform4fv(l, 1, f); return; }
            if (g_curFBO == 0) OnNamedUniform4(it->second, v);
        }
    }
    o_glUniform4fv(l, c, v);
}
HOOK(void, glUniform4f, (GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d)) {
    if (!g_namedLocs.empty()) { auto it = g_namedLocs.find(((uint64_t)g_curProg << 32) | (uint32_t)l); if (it != g_namedLocs.end()) {
        GLfloat v[4] = {a, b, c, d};
        if (it->second == 7) { FogOverride(v); if (g_curProg < 8192) { memcpy(g_progFog[g_curProg], v, sizeof v); g_progFogLoc1[g_curProg] = l + 1; } o_glUniform4f(l, v[0], v[1], v[2], v[3]); return; }
        if (g_curFBO == 0) OnNamedUniform4(it->second, v); } }
    o_glUniform4f(l, a, b, c, d);
}
HOOK(void, glUniform1i, (GLint l, GLint v)) {
    if (!g_namedLocs.empty()) { auto it = g_namedLocs.find(((uint64_t)g_curProg << 32) | (uint32_t)l); if (it != g_namedLocs.end() && (it->second == 5 || it->second == 6)) g_pendingShadowUnit[it->second - 5] = v;
        if (it != g_namedLocs.end() && it->second == 8 && g_curProg < 8192) g_progDiffUnit[g_curProg] = v; }
    o_glUniform1i(l, v);
}

typedef PROC(WINAPI* wglGetProcAddress_t)(LPCSTR);
static wglGetProcAddress_t o_wglGetProcAddress;
#include "tex.inl"
struct ProcHook { const char* name; void* hook; void** orig; };
#define PH(n) { #n, (void*)h_##n, (void**)&o_##n }
#define PHA(alias, n) { alias, (void*)h_##n, (void**)&o_##n }
static ProcHook g_procHooks[] = {
    PH(glBindFramebuffer), PHA("glBindFramebufferEXT", glBindFramebuffer),
    PH(glFramebufferTexture2D), PHA("glFramebufferTexture2DEXT", glFramebufferTexture2D),
    PH(glFramebufferRenderbuffer), PHA("glFramebufferRenderbufferEXT", glFramebufferRenderbuffer),
    PH(glRenderbufferStorage), PHA("glRenderbufferStorageEXT", glRenderbufferStorage),
    PH(glRenderbufferStorageMultisample), PHA("glRenderbufferStorageMultisampleEXT", glRenderbufferStorageMultisample),
    PH(glBlitFramebuffer), PHA("glBlitFramebufferEXT", glBlitFramebuffer),
    PH(glCompressedTexImage2D), PHA("glCompressedTexImage2DARB", glCompressedTexImage2D),
    PH(glUseProgram), PHA("glUseProgramObjectARB", glUseProgram),
    PH(glActiveTexture), PHA("glActiveTextureARB", glActiveTexture),
    PH(glCreateShader), PHA("glCreateShaderObjectARB", glCreateShader),
    PH(glAttachShader), PHA("glAttachObjectARB", glAttachShader),
    PH(glLinkProgram), PHA("glLinkProgramARB", glLinkProgram),
    PH(glGetUniformLocation), PHA("glGetUniformLocationARB", glGetUniformLocation),
    PH(glUniformMatrix4fv), PHA("glUniformMatrix4fvARB", glUniformMatrix4fv),
    PH(glShaderSource), PHA("glShaderSourceARB", glShaderSource),
    PH(glUniform4fv), PHA("glUniform4fvARB", glUniform4fv), PH(glUniform4f), PHA("glUniform4fARB", glUniform4f), PH(glUniform1i), PHA("glUniform1iARB", glUniform1i),
};

static PROC WINAPI h_wglGetProcAddress(LPCSTR name) {
    PROC p = o_wglGetProcAddress(name);
    for (auto& e : g_procHooks) {
        if (strcmp(e.name, name)) continue;
        if (!p) break;
        if (!*e.orig) *e.orig = (void*)p;
        logf("[proc] %s -> hooked", name);
        return (PROC)e.hook;
    }
    logf("[proc] %s -> %p", name, (void*)p);
    return p;
}

// compile a candidate source in a scratch shader object; logs the info log on failure
static bool TestCompile(GLenum type, const std::string& src) {
    typedef void(WINAPI* cs_t)(GLuint); typedef void(WINAPI* giv_t)(GLuint, GLenum, GLint*); typedef void(WINAPI* gil_t)(GLuint, GLsizei, GLsizei*, GLchar*); typedef void(WINAPI* ds_t)(GLuint);
    static cs_t compile = (cs_t)o_wglGetProcAddress("glCompileShader"); static giv_t getiv = (giv_t)o_wglGetProcAddress("glGetShaderiv");
    static gil_t getlog = (gil_t)o_wglGetProcAddress("glGetShaderInfoLog"); static ds_t del = (ds_t)o_wglGetProcAddress("glDeleteShader");
    if (!compile || !getiv || !o_glCreateShader) return true;   // cannot test -> trust
    GLuint t = o_glCreateShader(type); const GLchar* p = src.c_str();
    o_glShaderSource(t, 1, &p, nullptr); compile(t);
    GLint ok = 0; getiv(t, 0x8B81, &ok);
    if (!ok && getlog) { char buf[2048]; getlog(t, sizeof buf, nullptr, buf); logf("[compile test] FAILED:\n%s", buf); }
    if (del) del(t);
    return ok != 0;
}

typedef HGLRC(WINAPI* wglCreateContext_t)(HDC);
static wglCreateContext_t o_wglCreateContext;
static HGLRC WINAPI h_wglCreateContext(HDC dc) { HGLRC c = o_wglCreateContext(dc); logf("wglCreateContext dc=%p -> %p", (void*)dc, (void*)c); return c; }
typedef BOOL(WINAPI* wglMakeCurrent_t)(HDC, HGLRC);
static wglMakeCurrent_t o_wglMakeCurrent;
static BOOL WINAPI h_wglMakeCurrent(HDC dc, HGLRC c) {
    BOOL r = o_wglMakeCurrent(dc, c);
    logf("wglMakeCurrent dc=%p ctx=%p -> %d", (void*)dc, (void*)c, r);
    static bool once;
    if (r && c && !once && o_glGetString) {
        once = true;
        logf("GL_VENDOR   %s", (const char*)o_glGetString(GL_VENDOR));
        logf("GL_RENDERER %s", (const char*)o_glGetString(GL_RENDERER));
        logf("GL_VERSION  %s", (const char*)o_glGetString(GL_VERSION));
        logf("GLSL        %s", (const char*)o_glGetString(GL_SHADING_LANGUAGE_VERSION));
    }
    return r;
}

// In-process snapshot (privacy-safe: only the game's own back buffer). Trigger: create <game>\tyrender\snap.req
// (its first line, if any, is the output name). Writes <game>\tyrender\snaps\<name>.bmp, then deletes snap.req.
typedef void(WINAPI* glGetIntegerv_t)(GLenum, GLint*);
static void Snapshot(HDC dc) {
    char req[MAX_PATH]; snprintf(req, sizeof req, "%styrender\\snap.req", g_dir);
    if (GetFileAttributesA(req) == INVALID_FILE_ATTRIBUTES) return;
    std::string name; ReadFileStr(req, name);
    while (!name.empty() && (name.back() == '\r' || name.back() == '\n' || name.back() == ' ')) name.pop_back();
    if (name.empty()) name = "snap_" + std::to_string(g_frame);
    static glGetIntegerv_t getI = (glGetIntegerv_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    GLint vp[4] = {0, 0, 0, 0}; getI(0x0BA2 /*GL_VIEWPORT*/, vp);
    RECT rc; HWND wnd = WindowFromDC(dc);
    int w = vp[2], h = vp[3];
    if (wnd && GetClientRect(wnd, &rc)) { w = rc.right; h = rc.bottom; }   // whole client area, not the last viewport
    if (w <= 0 || h <= 0) { DeleteFileA(req); return; }
    if (o_glBindFramebuffer) o_glBindFramebuffer(0x8CA8 /*GL_READ_FRAMEBUFFER*/, 0);
    std::string px((size_t)w * h * 3, 0);
    typedef void(WINAPI* glPixelStorei_t)(GLenum, GLint);
    static glPixelStorei_t ps = (glPixelStorei_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPixelStorei");
    ps(0x0D05 /*GL_PACK_ALIGNMENT*/, 1);
    o_glReadPixels(0, 0, w, h, 0x80E0 /*GL_BGR*/, 0x1401 /*GL_UNSIGNED_BYTE*/, &px[0]);
    char dir[MAX_PATH]; snprintf(dir, sizeof dir, "%styrender\\snaps", g_dir); CreateDirectoryA(dir, nullptr);
    char out[MAX_PATH]; snprintf(out, sizeof out, "%s\\%s.bmp", dir, name.c_str());
    int stride = (w * 3 + 3) & ~3;
    BITMAPFILEHEADER fh = {}; BITMAPINFOHEADER ih = {};
    ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = h; ih.biPlanes = 1; ih.biBitCount = 24; ih.biSizeImage = stride * h;
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + ih.biSizeImage;
    if (FILE* f = fopen(out, "wb")) {
        fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
        char pad[3] = {0, 0, 0};
        for (int y = 0; y < h; y++) { fwrite(&px[(size_t)y * w * 3], 1, w * 3, f); fwrite(pad, 1, stride - w * 3, f); }  // GL rows are bottom-up like BMP
        fclose(f);
    }
    DeleteFileA(req);
    logf("[snap] %s %dx%d frame=%u", out, w, h, g_frame);
}

#include "ao.inl"
#include "fx.inl"
#include "gi.inl"
#include "atmos.inl"
#include "tess.inl"
#include "pbr.inl"
#include "light.inl"
#include "debugcam.inl"
#include "gpuprof.inl"
#include "abshot.inl"
static void ApplyShadowStrength(GLuint p) {
    typedef void(WINAPI* pu1f_t)(GLuint, GLint, GLfloat); static pu1f_t pu = (pu1f_t)o_wglGetProcAddress("glProgramUniform1f");
    auto it = g_shadowStrengthLoc.find(p); if (it == g_shadowStrengthLoc.end() || it->second < 0 || !pu) return;
    bool grass = false; for (GLuint s : g_progShaders[p]) if (g_grassVS.count(s)) grass = true;
    pu(p, it->second, grass ? g_shGrassStrength : g_shStrength);
}
static void AfterLink(GLuint p) {
    GLint loc = o_glGetUniformLocation ? o_glGetUniformLocation(p, "tyShadowStrength") : -1;
    if (loc >= 0) { g_shadowStrengthLoc[p] = loc; ApplyShadowStrength(p); }
    PbrOnLink(p); LightOnLink(p); TexOnLink(p);
}
static bool TessRewriteVS(GLuint shader, std::string& src) {
    std::vector<TessVar> vars; std::string orig = src;
    if (!RewriteVSForTess(src, vars)) return false;
    if (!TestCompile(GL_VERTEX_SHADER, src)) { src = orig; return false; }
    g_tessVS[shader] = vars; g_tessOrigVS[shader] = orig; return true;
}
static void TessOnAttach(GLuint p, GLuint s) { g_progShaders[p].push_back(s); }
static bool IsTessProg(GLuint p) { return !g_tessProgs.empty() && g_tessProgs.count(p); }
static void TessPatchMode() {
    TessDrawPrep();
    typedef void(WINAPI* pp_t)(GLenum, GLint); static pp_t pp = (pp_t)o_wglGetProcAddress("glPatchParameteri");
    static bool done; if (!done && pp) { pp(0x8E72 /*PATCH_VERTICES*/, 3); done = true; }
}
static void TessLink(GLuint p) {
    bool hasRewritten = false; for (GLuint s : g_progShaders[p]) if (g_tessVS.count(s)) hasRewritten = true;
    if (!hasRewritten) { o_glLinkProgram(p); return; }
    if (TessBeforeLink(p)) { o_glLinkProgram(p); TessAfterLink(p); return; }
    // stages unavailable -> put the original VS back so the plain link works
    typedef void(WINAPI* cs_t)(GLuint); static cs_t compile = (cs_t)o_wglGetProcAddress("glCompileShader");
    for (GLuint s : g_progShaders[p]) { auto o = g_tessOrigVS.find(s); if (o != g_tessOrigVS.end()) { const GLchar* q = o->second.c_str(); o_glShaderSource(s, 1, &q, nullptr); compile(s); g_tessVS.erase(s); } }
    o_glLinkProgram(p);
}

typedef BOOL(WINAPI* SwapBuffers_t)(HDC);
static SwapBuffers_t o_SwapBuffers;
static BOOL WINAPI h_SwapBuffers(HDC dc) {
    if (g_cap) { logf("=== END CAPTURE frame %u (draws=%u) ===", g_frame, g_draws); fflush(g_log); }
    MotionFrame(dc);
    if ((g_frame & 7) == 0) {
        Snapshot(dc); PollDebugCam(); MotionPoll(); PollAOFlags(); PollAtmosFlags(); PollGIFlags();
        char p[MAX_PATH]; snprintf(p, sizeof p, "%styrender\\post_off", g_dir); g_fxOffFile = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
        snprintf(p, sizeof p, "%styrender\\reload", g_dir);
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) { DeleteFileA(p); LoadAOConfig(); LoadFXConfig(); LoadAtmosConfig(); LoadGIConfig(); LoadShadowConfig(); LoadPbrConfig(); LoadTessConfig(); LoadLightConfig(); LoadShotConfig(); g_pbrStamp.clear(); g_tessStamp.clear();
            for (auto& kv : g_shadowStrengthLoc) ApplyShadowStrength(kv.first);
            { char ini[MAX_PATH], b[32]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
              GetPrivateProfileStringA("fog", "Distance", "1.0", b, sizeof b, ini); g_fogDistance = (float)atof(b);
              GetPrivateProfileStringA("fog", "Amount", "1.0", b, sizeof b, ini); g_fogAmount = (float)atof(b); GetPrivateProfileStringA("fog", "LateBoost", "1.0", b, sizeof b, ini); g_lateFog = (float)atof(b); }
            logf("[cfg] reloaded all"); }
    }   // poll the trigger file every 8 frames
    TexReport();
    GProf(9);                                      // GPU timestamp: end of this frame's commands (before present, so vsync waits are not counted)
    ABShotFrame(dc);                               // A/B hotkey (abshot.inl): reads the frame just drawn, F11 by default
    BOOL r = o_SwapBuffers(dc);
    GProfNextFrame();
    g_frame++;
    UpdateJitter();
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    if (g_frame == 1) g_lastReport = now;
    else if (g_frame % 600 == 0) {
        double ms = (now.QuadPart - g_lastReport.QuadPart) * 1000.0 / g_qpf.QuadPart / 600.0;
        logf("[frame %u] avg %.2f ms/frame (%.1f fps), draws last frame=%u, tess progs=%u, skipped non-triangle tess draws=%u", g_frame, ms, 1000.0 / ms, g_draws, (unsigned)g_tessProgs.size(), g_tessBadDrawsExt);
        g_lastReport = now;
    }
    g_draws = 0;
    g_cap = (g_capFrame1 && g_frame == (unsigned)g_capFrame1) || (g_capFrame2 && g_frame == (unsigned)g_capFrame2);
    // capture-on-demand: create <game>\tyrender\cap.req -> the next frame is logged call-by-call (TOOLS\capframe.ps1)
    if ((g_frame & 7) == 0) {
        char req[MAX_PATH]; snprintf(req, sizeof req, "%styrender\\cap.req", g_dir);
        if (GetFileAttributesA(req) != INVALID_FILE_ATTRIBUTES) { DeleteFileA(req); g_cap = true; }
    }
    if (g_cap) logf("=== BEGIN CAPTURE frame %u ===", g_frame);
    return r;
}

// ---------------------------------------------------------------- launcher clash guard (issue #13)
// Reads the game's own settings.ini and logs a warning when it asks for something the DLL already provides (SSAO).
// Warn only: settings.ini is never rewritten. Config tyrender.ini [clash] Check=1 (0 = off).
static void CheckGameSettingsClash() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    if (!GetPrivateProfileIntA("clash", "Check", 1, ini)) return;
    char docs[MAX_PATH] = "";
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) return;
    char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\TY the Tasmanian Tiger\\settings.ini", docs);
    FILE* f = fopen(path, "rb"); if (!f) { logf("[clash] no game settings.ini at %s", path); return; }
    static char buf[16384]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    for (char* line = strtok(buf, "\r\n"); line; line = strtok(nullptr, "\r\n")) {
        char key[64] = "", val[64] = "";
        if (sscanf(line, " %63[^= ] = %63s", key, val) != 2) continue;
        if (!_stricmp(key, "SSAO") && !_stricmp(val, "true"))
            logf("[clash] settings.ini has SSAO=true; the DLL's own AO runs too and the menu can stall. Set SSAO=false in settings.ini.");
    }
}

// ---------------------------------------------------------------- init
static void Install() {
    HMODULE exe = GetModuleHandleA(nullptr);
#define IAT_GL(n) PatchIAT(exe, "OPENGL32.dll", #n, (void*)h_##n, (void**)&o_##n)
    IAT_GL(glViewport); IAT_GL(glBindTexture); IAT_GL(glClear); IAT_GL(glEnable); IAT_GL(glDisable);
    IAT_GL(glBlendFunc); IAT_GL(glDepthMask); IAT_GL(glColorMask); IAT_GL(glDrawElements); IAT_GL(glDrawArrays);
    IAT_GL(glTexImage2D); IAT_GL(glCopyTexImage2D); IAT_GL(glReadPixels); IAT_GL(glGetString);
    IAT_GL(wglGetProcAddress); IAT_GL(wglCreateContext); IAT_GL(wglMakeCurrent);
    PatchIAT(exe, "GDI32.dll", "SwapBuffers", (void*)h_SwapBuffers, (void**)&o_SwapBuffers);
    TexInstallHooks(exe);
}

// Dummy export so a patched TY.exe can import tyrender.dll (SRC\tyrender\patch_imports.ps1)
extern "C" __declspec(dllexport) void tyrender_init() {}

// Sanity check: the IAT value we saved must point into a loaded module (else imports were not snapped yet)
static void CheckOrig(const char* n, void* p) {
    HMODULE m = nullptr; char path[MAX_PATH] = "?";
    if (p && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m)) GetModuleFileNameA(m, path, MAX_PATH);
    logf("[iat] %s orig=%p module=%s", n, p, path);
}

// crash diagnostics: log the first fatal exception (code, address, module+offset, last GL activity) and flush
static volatile const char* g_lastStage = "init";
static LONG WINAPI CrashVEH(EXCEPTION_POINTERS* e) {
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_BREAKPOINT && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != 0xC0000409) return EXCEPTION_CONTINUE_SEARCH;
    static LONG once; if (InterlockedExchange(&once, 1)) return EXCEPTION_CONTINUE_SEARCH;
    void* addr = e->ExceptionRecord->ExceptionAddress; HMODULE m = nullptr; char mod[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m)) GetModuleFileNameA(m, mod, MAX_PATH);
    logf("!!! EXCEPTION %08lx at %p (%s +0x%lx) frame=%u stage=%s prog=%u tessProg=%d fbo=%u", code, addr, mod, (unsigned long)((BYTE*)addr - (BYTE*)m), g_frame, (const char*)g_lastStage, g_curProg, (int)g_curProgTess, g_curFBO);
    if (code == EXCEPTION_ACCESS_VIOLATION) logf("    access %s address %p", e->ExceptionRecord->ExceptionInformation[0] ? "write" : "read", (void*)e->ExceptionRecord->ExceptionInformation[1]);
    void* bt[24]; USHORT n = RtlCaptureStackBackTrace(0, 24, bt, nullptr);
    for (USHORT i = 0; i < n; i++) { HMODULE bm = nullptr; char bmod[MAX_PATH] = "?"; if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)bt[i], &bm)) GetModuleFileNameA(bm, bmod, MAX_PATH); logf("    #%u %p %s +0x%lx", i, bt[i], strrchr(bmod, '\\') ? strrchr(bmod, '\\') + 1 : bmod, (unsigned long)((BYTE*)bt[i] - (BYTE*)bm)); }
    if (g_log) fflush(g_log);
    return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(inst);
    GetModuleFileNameA(nullptr, g_dir, MAX_PATH);
    if (char* s = strrchr(g_dir, '\\')) s[1] = 0;
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%styrender.log", g_dir); g_log = fopen(p, "w");
    snprintf(p, sizeof p, "%styrender", g_dir); CreateDirectoryA(p, nullptr);
    snprintf(p, sizeof p, "%styrender\\dump", g_dir); CreateDirectoryA(p, nullptr);
    snprintf(p, sizeof p, "%styrender\\override", g_dir); CreateDirectoryA(p, nullptr);
    QueryPerformanceFrequency(&g_qpf);
    AddVectoredExceptionHandler(1, CrashVEH);
    LoadConfig();
    CheckGameSettingsClash();
    LoadAOConfig();
    LoadFXConfig();
    LoadShadowConfig();
    LoadAtmosConfig();
    LoadGIConfig();
    { char ini[MAX_PATH], b[32]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
      GetPrivateProfileStringA("fog", "Distance", "1.0", b, sizeof b, ini); g_fogDistance = (float)atof(b);
      GetPrivateProfileStringA("fog", "Amount", "1.0", b, sizeof b, ini); g_fogAmount = (float)atof(b); GetPrivateProfileStringA("fog", "LateBoost", "1.0", b, sizeof b, ini); g_lateFog = (float)atof(b); }
    LoadTessConfig();
    LoadPbrConfig(); LoadLightConfig(); LoadShotConfig(); LoadTexTags();
    logf("tyrender stage1 loaded. game dir %s  capture=%d,%d dump=%d override=%d", g_dir, g_capFrame1, g_capFrame2, g_dumpShaders, g_overrideShaders);
    Install();
    CheckOrig("SwapBuffers", (void*)o_SwapBuffers);
    CheckOrig("wglGetProcAddress", (void*)o_wglGetProcAddress);
    CheckOrig("glDrawElements", (void*)o_glDrawElements);
    logf("hooks installed");
    return TRUE;
}
