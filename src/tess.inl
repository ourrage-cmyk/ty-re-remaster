// tess.inl — Phong tessellation for the engine's 'standard' shader variants (rounds off low-poly silhouettes, e.g. Ty).
// How: at glShaderSource the vertex shader is rewritten (outputs renamed tcIn_*, extra output tcIn_tyWorld = worldPos);
// at glLinkProgram generated TCS/TES (#version 400) are attached; TES interpolates every varying, applies Phong tessellation in world
// space (Boubekeur & Alexa 2008) using the vertex normals, and re-projects with the program's own VPMatrix uniform.
// Triangle draws of tessellated programs are issued as GL_PATCHES. Any compile/link failure -> program relinked without tessellation.
// Config tyrender.ini [tess]: Enable=1 Scope=env-detail (REC round 2026-10-13: characters/skinned draws are NEVER tessellated and get no detail layer)
//   Scope=env-detail (default, also what old Scope=skin means): no tessellation, albedo-derived relief only in the fragment shader (pbr.inl)
//   Scope=env (alias: all): additionally tessellates environment geometry (inward-only displacement from the same albedo height field); Scope=off: no tessellation.
// Height field = albedo (multi-scale luminance, local-contrast normalised); see kTyHeightGLSL. No procedural noise.

#include <vector>
#include <map>
#include <regex>
struct TessVar { std::string type, name; };
static float g_tsReliefDepthFS = 3.0f, g_tsFsMipBias = 1.0f; static int g_tsFsSteps = 8;
static int g_tsEnable = 1, g_tsScopeAll = 0, g_tsMaxLevel = 8, g_tsDebug = 0; static float g_tsAlpha = 0.7f, g_tsAlphaWorld = 0.45f, g_tsTargetPx = 12.0f, g_tsEdgeWorld = 25.0f, g_tsFadeStart = 1500.0f, g_tsFadeEnd = 4000.0f, g_tsMaxSmooth = 3.0f, g_tsDisp = 2.5f, g_tsDispLod = 1.5f;
// --- world relief (session 5): texture-driven INWARD-only displacement, adaptive fractional tessellation, TCS frustum/back-face culling ---
static float g_tsTargetEdge = 4.0f, g_tsReliefDepth = 3.0f, g_tsGroundRelief = 1.0f, g_tsPropRelief = 0.7f, g_tsReliefFade0 = 700.0f, g_tsReliefFade1 = 1600.0f, g_tsReliefLod = 0.0f, g_tsEdgeFade = 2.5f;
static float g_tsReliefCurve = 1.0f, g_tsReliefKnee = 0.0f, g_tsReliefKneeSlope = 0.35f;   // relief shaping, see LoadTessConfig
static int g_tsWorldMax = 48, g_tsCull = 1, g_tsAB = 0; static float g_tsLive = 1.0f;   // ABToggle=1 (test aid): relief+PBR extras flip on/off every time the debug-camera file changes static float g_tsPhongInward = 1.0f;
struct TessLoc { GLint cam = -1, tp = -1, cull = -1, tr = -1, tr2 = -1, lod = -1; };
// #10 distance LOD for tessellation: [lod] TessEnable=0 (default, unchanged). 1 = the edge level falls smoothly to TessMinScale of its value
// between Start and End (view distance, world units). Shares Start/End/MinScale with the [lod] GI ramp; read in LoadTessConfig.
static int g_tsLodOn = 0; static float g_tsLodS = 1500.0f, g_tsLodE = 6000.0f, g_tsLodMin = 0.35f;
static std::unordered_map<GLuint, TessLoc> g_tessLoc; static std::unordered_map<GLuint, float> g_tessCullLast; static std::unordered_map<GLuint, unsigned> g_tessStamp;
static std::unordered_map<GLuint, std::vector<TessVar>> g_tessVS;     // VS shader id -> its output varyings (tessellation-ready)
static std::unordered_map<GLuint, std::vector<GLuint>> g_progShaders; // program -> attached shaders
static std::set<GLuint> g_tessProgs;                                   // programs linked with TCS/TES
static std::unordered_map<std::string, std::pair<GLuint, GLuint>> g_tessStages; // varying signature -> compiled (tcs, tes)
static unsigned g_tessBadDraws;
static int g_portTessVerGate = 1;   // [portability] TessVersionGate: 1 = skip tessellation stages below GL 4.0 (portable default), 0 = always try

static void LoadTessConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_tsEnable = GetPrivateProfileIntA("tess", "Enable", 1, ini); g_tsDebug = GetPrivateProfileIntA("tess", "Debug", 0, ini);
    GetPrivateProfileStringA("tess", "Scope", "env-detail", b, sizeof b, ini); g_tsScopeAll = !_stricmp(b, "all") || !_stricmp(b, "env");   // skin (legacy) / env-detail / off -> no geometry tessellation
    GetPrivateProfileStringA("tess", "Alpha", "0.7", b, sizeof b, ini); g_tsAlpha = (float)atof(b);
    GetPrivateProfileStringA("tess", "TargetPx", "12", b, sizeof b, ini); g_tsTargetPx = (float)atof(b);
    GetPrivateProfileStringA("tess", "AlphaWorld", "0.45", b, sizeof b, ini); g_tsAlphaWorld = (float)atof(b);
    GetPrivateProfileStringA("tess", "EdgeLength", "25", b, sizeof b, ini); g_tsEdgeWorld = (float)atof(b);
    GetPrivateProfileStringA("tess", "FadeStart", "1500", b, sizeof b, ini); g_tsFadeStart = (float)atof(b);
    GetPrivateProfileStringA("tess", "FadeEnd", "4000", b, sizeof b, ini); g_tsFadeEnd = (float)atof(b);
    GetPrivateProfileStringA("tess", "MaxSmoothing", "3", b, sizeof b, ini); g_tsMaxSmooth = (float)atof(b);        // world units (~cm)
    GetPrivateProfileStringA("tess", "Displacement", "2.5", b, sizeof b, ini); g_tsDisp = (float)atof(b);              // +-units from texture height
    GetPrivateProfileStringA("tess", "DisplacementLod", "1.5", b, sizeof b, ini); g_tsDispLod = (float)atof(b);
    // world relief keys
    GetPrivateProfileStringA("tess", "TargetEdge", "4", b, sizeof b, ini); g_tsTargetEdge = (float)atof(b); if (g_tsTargetEdge < 0.5f) g_tsTargetEdge = 0.5f;   // world units per sub-triangle edge on near surfaces
    GetPrivateProfileStringA("tess", "ReliefDepth", "3", b, sizeof b, ini); g_tsReliefDepth = (float)atof(b);          // max inward depth (world units) of cracks/valleys; 0 = old +-Displacement path
    // Shaping of the relief amount (texture darkness -> depth). Defaults reproduce the old curve exactly.
    //   ReliefCurve < 1 lifts weak relief (textures that were barely carved get deeper: the floor rises); > 1 pushes weak relief down.
    //   ReliefKnee > 0 (world units): depth above the knee grows only at ReliefKneeSlope of its rate, so strongly carved textures stop growing when ReliefDepth is raised.
    GetPrivateProfileStringA("tess", "ReliefCurve", "1.0", b, sizeof b, ini); g_tsReliefCurve = (float)atof(b); if (g_tsReliefCurve < 0.2f) g_tsReliefCurve = 0.2f; if (g_tsReliefCurve > 4.0f) g_tsReliefCurve = 4.0f;
    GetPrivateProfileStringA("tess", "ReliefKnee", "0", b, sizeof b, ini); g_tsReliefKnee = (float)atof(b); if (g_tsReliefKnee < 0.0f) g_tsReliefKnee = 0.0f;
    GetPrivateProfileStringA("tess", "ReliefKneeSlope", "0.35", b, sizeof b, ini); g_tsReliefKneeSlope = (float)atof(b); if (g_tsReliefKneeSlope < 0.0f) g_tsReliefKneeSlope = 0.0f; if (g_tsReliefKneeSlope > 1.0f) g_tsReliefKneeSlope = 1.0f;
    GetPrivateProfileStringA("tess", "GroundRelief", "1.0", b, sizeof b, ini); g_tsGroundRelief = (float)atof(b);      // multiplier on up-facing surfaces (sand, paths)
    GetPrivateProfileStringA("tess", "PropRelief", "0.7", b, sizeof b, ini); g_tsPropRelief = (float)atof(b);          // multiplier on walls/fence/rope/rock
    GetPrivateProfileStringA("tess", "ReliefFadeStart", "700", b, sizeof b, ini); g_tsReliefFade0 = (float)atof(b);
    GetPrivateProfileStringA("tess", "ReliefFadeEnd", "1600", b, sizeof b, ini); g_tsReliefFade1 = (float)atof(b);
    GetPrivateProfileStringA("tess", "ReliefLods", "0", b, sizeof b, ini); g_tsReliefLod = (float)atof(b);              // extra mip bias for the height sampling (+ = smoother/coarser relief)
    GetPrivateProfileStringA("tess", "EdgeFade", "2.5", b, sizeof b, ini); g_tsEdgeFade = (float)atof(b);                // >0: fade relief to 0 within N world units of patch borders (hides UV-seam cracks)
    g_tsLodOn = GetPrivateProfileIntA("lod", "TessEnable", 0, ini);
    GetPrivateProfileStringA("lod", "Start", "1500", b, sizeof b, ini); g_tsLodS = (float)atof(b);
    GetPrivateProfileStringA("lod", "End", "6000", b, sizeof b, ini); g_tsLodE = (float)atof(b); if (g_tsLodE < g_tsLodS + 1.0f) g_tsLodE = g_tsLodS + 1.0f;
    GetPrivateProfileStringA("lod", "MinScale", "0.35", b, sizeof b, ini); g_tsLodMin = (float)atof(b); if (g_tsLodMin < 0.0f) g_tsLodMin = 0.0f; if (g_tsLodMin > 1.0f) g_tsLodMin = 1.0f;
    g_tsReliefDepthFS = g_tsReliefDepth * 0.7f;   // fragment parallax uses 70% of ReliefDepth (stronger smears the paint at grazing angles)
    g_tsFsSteps = GetPrivateProfileIntA("tess", "ReliefSteps", 8, ini); if (g_tsFsSteps < 3) g_tsFsSteps = 3; if (g_tsFsSteps > 12) g_tsFsSteps = 12;   // parallax steps in the fragment shader
    GetPrivateProfileStringA("tess", "ReliefFeature", "1.0", b, sizeof b, ini); g_tsFsMipBias = (float)atof(b); if (g_tsFsMipBias < 0.5f) g_tsFsMipBias = 0.5f; // >1 = coarser features
    g_tsWorldMax = GetPrivateProfileIntA("tess", "WorldMaxLevel", 48, ini); if (g_tsWorldMax < 2) g_tsWorldMax = 2; if (g_tsWorldMax > 63) g_tsWorldMax = 63;
    g_tsAB = GetPrivateProfileIntA("tess", "ABToggle", 0, ini);
    g_tsCull = GetPrivateProfileIntA("tess", "Cull", 1, ini);                                                          // TCS frustum + back-face patch culling
    g_tsMaxLevel = GetPrivateProfileIntA("tess", "MaxLevel", 8, ini); if (g_tsMaxLevel < 1) g_tsMaxLevel = 1; if (g_tsMaxLevel > 32) g_tsMaxLevel = 32;
    g_portTessVerGate = GetPrivateProfileIntA("portability", "TessVersionGate", 1, ini);
}

// GL version from GL_VERSION, parsed once. Unknown version (no glGetString hook yet) counts as "at least" so nothing is gated by accident.
static bool GLAtLeast(int wantMaj, int wantMin) {
    static bool parsed = false; static int gMaj = 0, gMin = 0;
    if (!parsed) {
        const char* v = o_glGetString ? (const char*)o_glGetString(0x1F02) : nullptr;
        int a = 0, b = 0; if (!v || sscanf(v, "%d.%d", &a, &b) != 2) return true;
        gMaj = a; gMin = b; parsed = true;
    }
    return gMaj > wantMaj || (gMaj == wantMaj && gMin >= wantMin);
}
// Tessellation needs these entry points (fetched via wglGetProcAddress, which can return NULL on some legacy or partial contexts).
// Checked once; without them tessellation stays off and the programs link as before.
static bool TessEntryPointsOk() {
    static int ok = -1;
    if (ok < 0) {
        ok = o_wglGetProcAddress && o_wglGetProcAddress("glCompileShader") && o_wglGetProcAddress("glGetShaderiv") && o_wglGetProcAddress("glGetShaderInfoLog") &&
             o_wglGetProcAddress("glGetProgramiv") && o_wglGetProcAddress("glGetProgramInfoLog") && o_wglGetProcAddress("glDetachShader") ? 1 : 0;
        if (!ok) logf("[tess] GL entry points missing (shader compile/query/detach) -> tessellation off");
    }
    return ok == 1;
}

// Rewrite a standard-template VS. Returns false if the variant is not suitable.
static bool RewriteVSForTess(std::string& s, std::vector<TessVar>& vars) {
    if (!g_tsEnable) return false;
    if (s.find("gl_Position = VPMatrix * worldPos;") == std::string::npos) return false;     // standard template only
    if (s.find("V_OUT lowp vec3 v_normal;") == std::string::npos) return false;              // need normals (LIT or SHADOWNORMS)
    std::smatch mm;
    std::regex rLit("#define LIT (\\d)"), rSn("#define SHADOWNORMS (\\d)"), rSkin("#define SKIN (\\d)");
    auto def = [&](const std::regex& r) { return std::regex_search(s, mm, r) ? mm[1].str() == "1" : false; };
    if (!(def(rLit) || def(rSn))) return false;                                              // v_normal declared but compiled out
    bool skinned = def(rSkin);
    if (skinned || !g_tsScopeAll) return false;                                                // characters are never tessellated; Scope env/all only
    if (def(std::regex("#define AREF (\\d)"))) return false;                          // alpha-tested foliage cards: no world tessellation
    // collect V_OUT declarations (only active ones matter, but renaming inactive ones is harmless; TES must only use active ones ->
    // we compile-test the result, and generated stages reference a varying only if its declaration line is outside a false #if: keep simple
    // by resolving #if blocks with the preprocessor: we let the GLSL compiler do it and use glGetProgram introspection? Too heavy ->
    // instead we strip declarations that sit inside #if blocks whose condition is false using the variant's #defines.)
    std::map<std::string, int> defs;
    static const std::regex rDef("#define (\\w+) (\\d+)");
    for (std::sregex_iterator it(s.begin(), s.end(), rDef), end; it != end; ++it) defs[(*it)[1]] = atoi((*it)[2].str().c_str());
    auto evalCond = [&](std::string c) -> bool {           // supports the template's forms: (A == n), (A > n), (A != n), ||, &&
        for (auto& kv : defs) c = std::regex_replace(c, std::regex("\\b" + kv.first + "\\b"), std::to_string(kv.second));
        // evaluate simple comparisons
        std::regex cmp("(\\d+)\\s*(==|!=|>=|<=|>|<)\\s*(\\d+)");
        for (std::smatch m2; std::regex_search(c, m2, cmp);) {
            int a = atoi(m2[1].str().c_str()), b = atoi(m2[3].str().c_str()); std::string op = m2[2]; bool r =
                op == "==" ? a == b : op == "!=" ? a != b : op == ">=" ? a >= b : op == "<=" ? a <= b : op == ">" ? a > b : a < b;
            c = m2.prefix().str() + (r ? "1" : "0") + m2.suffix().str();
        }
        c = std::regex_replace(c, std::regex("[()\\s]"), "");
        // OR of AND terms
        bool any = false; size_t st = 0;
        while (true) { size_t p = c.find("||", st); std::string term = c.substr(st, p == std::string::npos ? std::string::npos : p - st);
            bool all = true; size_t a0 = 0; while (true) { size_t q = term.find("&&", a0); std::string f = term.substr(a0, q == std::string::npos ? std::string::npos : q - a0); if (f != "1") all = false; if (q == std::string::npos) break; a0 = q + 2; }
            if (all) any = true; if (p == std::string::npos) break; st = p + 2; }
        return any;
    };
    // walk lines tracking #if/#else/#endif to find active V_OUT declarations (VS part ends at the first "void main")
    size_t mainPos = s.find("void main");
    std::vector<int> stack; bool active = true; std::vector<bool> act; act.push_back(true);
    std::vector<bool> taken;
    size_t pos = 0;
    while (pos < mainPos) {
        size_t e = s.find('\n', pos); if (e == std::string::npos) break;
        std::string line = s.substr(pos, e - pos);
        std::smatch lm;
        if (std::regex_search(line, lm, std::regex("^\\s*#if\\s+(.*)"))) { bool c = evalCond(lm[1]); act.push_back(act.back() && c); taken.push_back(c); }
        else if (std::regex_search(line, lm, std::regex("^\\s*#elif\\s+(.*)"))) { bool parent = act[act.size() - 2]; bool c = !taken.back() && evalCond(lm[1]); act.back() = parent && c; if (c) taken.back() = true; }
        else if (std::regex_search(line, std::regex("^\\s*#else"))) { bool parent = act[act.size() - 2]; act.back() = parent && !taken.back(); taken.back() = true; }
        else if (std::regex_search(line, std::regex("^\\s*#endif"))) { if (act.size() > 1) { act.pop_back(); taken.pop_back(); } }
        else if (act.back() && std::regex_search(line, lm, std::regex("^\\s*V_OUT\\s+(?:lowp\\s+|mediump\\s+|highp\\s+)?(\\w+)\\s+(\\w+)\\s*;"))) vars.push_back({lm[1], lm[2]});
        pos = e + 1;
    }
    (void)active; (void)stack;
    vars.push_back({skinned ? "#skin" : "#world", ""});   // pseudo entry: selects alpha in MakeTES, not a varying
    bool hasNormal = false; for (auto& v : vars) if (v.name == "v_normal") hasNormal = true;
    if (!hasNormal || vars.empty()) return false;
    // rename outputs in the VS (whole words), add world position output
    for (auto& v : vars) if (!v.name.empty()) s = std::regex_replace(s, std::regex("\\b" + v.name + "\\b"), "tcIn_" + v.name);
    size_t gp = s.find("gl_Position = VPMatrix * worldPos;");
    s.insert(gp + strlen("gl_Position = VPMatrix * worldPos;"), "\n  tcIn_tyWorld = worldPos.xyz;");
    s.insert(s.find("void main"), "out vec3 tcIn_tyWorld;\n");
    return true;
}

// Albedo-derived height field shared by the fragment parallax (pbr.inl) and the tessellation displacement (TES).
// Weber-style local contrast of the luminance at two/three scales: dark thin lines (cracks, grout) -> negative, painted lettering / strands -> +/- by their own luma.
static const char* kTyHeightGLSL = R"GLSL(
float TyLh(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
float TyBand(float a, float b) { float x = (a - b) / (0.10 + b) * 3.0; return x / (1.0 + abs(x)); }
float TyH2(vec2 uv, vec2 gx, vec2 gy) {     // 2 taps (parallax loop)
  float a = TyLh(textureGrad(diffuseTexture, uv, gx, gy).rgb), b = TyLh(textureGrad(diffuseTexture, uv, gx * 6.0, gy * 6.0).rgb);
  return TyBand(a, b); }
float TyH3(vec2 uv, vec2 gx, vec2 gy) {     // 3 taps (normals, displacement)
  float a = TyLh(textureGrad(diffuseTexture, uv, gx, gy).rgb), b = TyLh(textureGrad(diffuseTexture, uv, gx * 6.0, gy * 6.0).rgb), c = TyLh(textureGrad(diffuseTexture, uv, gx * 24.0, gy * 24.0).rgb);
  return clamp(TyBand(a, b) + 0.4 * TyBand(b, c), -1.0, 1.0); }
float TyDepth(float h) { return clamp(0.25 - 0.75 * h, 0.0, 1.0); }   // 0 = top of the relief volume (raised), 1 = deepest groove; flat = 0.25
)GLSL";

static std::string TessSignature(const std::vector<TessVar>& vars) { std::string k; for (auto& v : vars) k += v.type + " " + v.name + ";"; return k; }

static std::string MakeTCSLegacy(const std::vector<TessVar>& vars) {
    int sw = g_screenW > 0 ? g_screenW : 1280, sh = g_screenH > 0 ? g_screenH : 720;
    std::string s = "#version 400\nlayout(vertices = 3) out;\n";
    for (auto& v : vars) if (!v.name.empty()) s += "in " + v.type + " tcIn_" + v.name + "[];\nout " + v.type + " teIn_" + v.name + "[];\n";
    s += "in vec3 tcIn_tyWorld[];\nout vec3 teIn_tyWorld[];\n";
    char buf[1400];
    snprintf(buf, sizeof buf,
        "// levels from WORLD-space edge length: identical for every camera position/orientation -> vertices never slide\n"
        "float EdgeLevel(vec3 a, vec3 b) { return clamp(floor(distance(a, b) / %.2f) + 1.0, 1.0, %d.0); }\n"
        "void main() {\n", g_tsEdgeWorld, g_tsMaxLevel);
    (void)sw; (void)sh;
    s += buf;
    for (auto& v : vars) if (!v.name.empty()) s += "  teIn_" + v.name + "[gl_InvocationID] = tcIn_" + v.name + "[gl_InvocationID];\n";
    s += "  teIn_tyWorld[gl_InvocationID] = tcIn_tyWorld[gl_InvocationID];\n"
         "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
         "  if (gl_InvocationID == 0) {\n"
         "    vec4 p0 = gl_in[0].gl_Position, p1 = gl_in[1].gl_Position, p2 = gl_in[2].gl_Position;\n"
         "    // cull patches fully outside the frustum (cheap) -> level 0 discards them\n"
         "    bool outside = (p0.x > p0.w && p1.x > p1.w && p2.x > p2.w) || (p0.x < -p0.w && p1.x < -p1.w && p2.x < -p2.w) ||\n"
         "                   (p0.y > p0.w && p1.y > p1.w && p2.y > p2.w) || (p0.y < -p0.w && p1.y < -p1.w && p2.y < -p2.w);\n"
         "    vec3 w0 = tcIn_tyWorld[0], w1 = tcIn_tyWorld[1], w2 = tcIn_tyWorld[2];\n"
         "    float e0 = EdgeLevel(w1, w2), e1 = EdgeLevel(w2, w0), e2 = EdgeLevel(w0, w1);\n"
         "    if (outside) { e0 = 0.0; e1 = 0.0; e2 = 0.0; }\n"
         "    gl_TessLevelOuter[0] = e0; gl_TessLevelOuter[1] = e1; gl_TessLevelOuter[2] = e2;\n"
         "    gl_TessLevelInner[0] = max(e0, max(e1, e2));\n"
         "  }\n}\n";
    return s;
}

static std::string MakeTESLegacy(const std::vector<TessVar>& vars) {
    std::string s = "#version 400\nlayout(triangles, equal_spacing, ccw) in;\nuniform mat4 VPMatrix;\n";
    bool hasUV = false; for (auto& v : vars) if (v.name == "v_uv") hasUV = true;
    if (hasUV) s += "uniform sampler2D diffuseTexture;\n";   // same uniform as the fragment shader -> same texture unit
    bool skin = false; for (auto& v : vars) if (v.type == "#skin") skin = true;
    for (auto& v : vars) if (!v.name.empty()) s += "in " + v.type + " teIn_" + v.name + "[];\nout " + v.type + " " + v.name + ";\n";
    s += "in vec3 teIn_tyWorld[];\n";
    char buf[900];
    snprintf(buf, sizeof buf,
        "vec3 PhongProj(vec3 p, vec3 q, vec3 n) { return q - dot(q - p, n) * n; }\n"
        "void main() {\n"
        "  vec3 b = gl_TessCoord;\n"
        "  vec3 P0 = teIn_tyWorld[0], P1 = teIn_tyWorld[1], P2 = teIn_tyWorld[2];\n"
        "  vec3 N0 = normalize(teIn_v_normal[0]), N1 = normalize(teIn_v_normal[1]), N2 = normalize(teIn_v_normal[2]);\n"
        "  vec3 p = b.x * P0 + b.y * P1 + b.z * P2;\n"
        "  vec3 ph = b.x * PhongProj(P0, p, N0) + b.y * PhongProj(P1, p, N1) + b.z * PhongProj(P2, p, N2);\n"
        "  float dist = (VPMatrix * vec4(p, 1.0)).w;                       // view depth: smooth, so the fade never pops\n"
        "  float a = %.3f * (1.0 - smoothstep(%.1f, %.1f, dist));\n"
        "  vec3 off = (ph - p) * a;\n"
        "  float ol = length(off); if (ol > %.2f) off *= %.2f / ol;          // never move geometry more than a few units (no fusing / sinking)\n"
        "  vec3 w = p + off;\n", skin ? g_tsAlpha : g_tsAlphaWorld, g_tsFadeStart, g_tsFadeEnd, g_tsMaxSmooth, g_tsMaxSmooth);
    s += buf;
    buf[0] = 0;
    if (!skin && hasUV && g_tsDisp > 0) {
        // texture-driven relief: luminance of the surface texture as height, +-Displacement units along the normal, faded with distance
        char d2[1400];
        snprintf(d2, sizeof d2,
            "  vec2 tuv = b.x * teIn_v_uv[0] + b.y * teIn_v_uv[1] + b.z * teIn_v_uv[2];\n"
            "  // high-pass height: fine texture detail only (paint lines / large colour areas must not rise)\n"
            "  vec3 hc0 = textureLod(diffuseTexture, tuv, %.2f).rgb, hc1 = textureLod(diffuseTexture, tuv, %.2f + 3.0).rgb;\n"
            "  // hue-edge suppression: painted shapes (hue differs from the local average) must not rise, only same-hue grain (rock, wood, dirt)\n"
            "  float hkeep = 1.0 - smoothstep(0.10, 0.40, length(hc0 / (dot(hc0, vec3(0.299, 0.587, 0.114)) + 0.08) - hc1 / (dot(hc1, vec3(0.299, 0.587, 0.114)) + 0.08)));\n"
            "  float hgt = dot(hc0 - hc1, vec3(0.299, 0.587, 0.114)) * 2.0 * hkeep + 0.5;\n"
            "  vec3 nI = normalize(b.x * N0 + b.y * N1 + b.z * N2);\n"
            "  w += nI * ((hgt - 0.5) * 2.0 * %.2f * (1.0 - smoothstep(%.1f, %.1f, dist)));\n", g_tsDispLod, g_tsDispLod, g_tsDisp, g_tsFadeStart, g_tsFadeEnd);
        s += d2;
    }
    s += "  gl_Position = VPMatrix * vec4(w, 1.0);\n";
    s += buf;
    for (auto& v : vars) if (!v.name.empty()) s += "  " + v.name + " = b.x * teIn_" + v.name + "[0] + b.y * teIn_" + v.name + "[1] + b.z * teIn_" + v.name + "[2];\n";
    // Debug=1: darken vertices near sub-triangle edges (shows the generated tessellation density)
    bool hasColour = false; for (auto& v : vars) if (v.name == "v_colour") hasColour = true;
    if (g_tsDebug && hasColour) s += "  if (min(b.x, min(b.y, b.z)) < 0.02) v_colour.rgb *= 0.15;\n";
    s += "}\n";
    return s;
}


static bool TessVarsSkin(const std::vector<TessVar>& vars) { for (auto& v : vars) if (v.type == "#skin") return true; return false; }

// World (non-skinned) TCS: levels from a WORLD-space edge rule (len / TargetEdge) limited by a screen-space rule (len*pxPerUnit/dist / TargetPx), both
// symmetric in the edge endpoints (shared edges get equal factors -> no cracks). fractional_odd spacing -> levels change smoothly with distance (no popping).
// Frustum + back-face culling set all outer levels to 0.
static std::string MakeTCSWorld(const std::vector<TessVar>& vars) {
    std::string s = "#version 400\nlayout(vertices = 3) out;\n";
    for (auto& v : vars) if (!v.name.empty()) s += "in " + v.type + " tcIn_" + v.name + "[];\nout " + v.type + " teIn_" + v.name + "[];\n";
    s += "in vec3 tcIn_tyWorld[];\nout vec3 teIn_tyWorld[];\n";
    s += g_tsCull ? "#define TY_CULL 1\n" : "#define TY_CULL 0\n";
    s += R"GLSL(
uniform vec3 tyTCam; uniform vec4 tyTP; uniform float tyTCull; uniform vec4 tyTL;   // tyTP: x px per world unit at distance 1, y target px, z target edge (world), w max level. tyTL: x Start y End z MinScale (x = 0: off)
float EdgeLevel(vec3 a, vec3 b) {
  float len = distance(a, b);
  float d = max(distance(0.5 * (a + b), tyTCam), 1.0);
  float lw = len / tyTP.z;
  float ls = (tyTP.x > 0.0 && tyTP.y > 0.0) ? len * tyTP.x / (d * tyTP.y) : lw;
  float lod = tyTL.x > 0.0 ? mix(1.0, tyTL.z, smoothstep(tyTL.x, tyTL.y, d)) : 1.0;   // #10 distance LOD: smooth, never a hard step
  return clamp(min(lw, ls) * lod, 1.0, tyTP.w);
}
void main() {
)GLSL";
    for (auto& v : vars) if (!v.name.empty()) s += "  teIn_" + v.name + "[gl_InvocationID] = tcIn_" + v.name + "[gl_InvocationID];\n";
    s += "  teIn_tyWorld[gl_InvocationID] = tcIn_tyWorld[gl_InvocationID];\n"
         "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n";
    s += R"GLSL(
  if (gl_InvocationID == 0) {
    vec4 p0 = gl_in[0].gl_Position, p1 = gl_in[1].gl_Position, p2 = gl_in[2].gl_Position;
    vec3 w0 = tcIn_tyWorld[0], w1 = tcIn_tyWorld[1], w2 = tcIn_tyWorld[2];
    float e0 = EdgeLevel(w1, w2), e1 = EdgeLevel(w2, w0), e2 = EdgeLevel(w0, w1);
    bool cull = false;
#if TY_CULL
    const float m = 0.12;   // widened planes: smoothing/relief may move the surface a little
    cull = (p0.x - p0.w * (1.0 + m) > 0.0 && p1.x - p1.w * (1.0 + m) > 0.0 && p2.x - p2.w * (1.0 + m) > 0.0) ||
           (p0.x + p0.w * (1.0 + m) < 0.0 && p1.x + p1.w * (1.0 + m) < 0.0 && p2.x + p2.w * (1.0 + m) < 0.0) ||
           (p0.y - p0.w * (1.0 + m) > 0.0 && p1.y - p1.w * (1.0 + m) > 0.0 && p2.y - p2.w * (1.0 + m) > 0.0) ||
           (p0.y + p0.w * (1.0 + m) < 0.0 && p1.y + p1.w * (1.0 + m) < 0.0 && p2.y + p2.w * (1.0 + m) < 0.0);
    if (!cull && tyTCull != 0.0 && p0.w > 0.0 && p1.w > 0.0 && p2.w > 0.0) {      // back-face: screen winding says back AND all vertex normals face away
      vec2 a = p0.xy / p0.w, b = p1.xy / p1.w, c = p2.xy / p2.w;
      float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
      bool nb = dot(tcIn_v_normal[0], tyTCam - w0) < 0.0 && dot(tcIn_v_normal[1], tyTCam - w1) < 0.0 && dot(tcIn_v_normal[2], tyTCam - w2) < 0.0;
      cull = (area * tyTCull < 0.0) && nb;
    }
#endif
    if (cull) { e0 = 0.0; e1 = 0.0; e2 = 0.0; }
    gl_TessLevelOuter[0] = e0; gl_TessLevelOuter[1] = e1; gl_TessLevelOuter[2] = e2;
    gl_TessLevelInner[0] = cull ? 0.0 : max(e0, max(e1, e2));
  }
}
)GLSL";
    return s;
}

// World TES: Phong smoothing that never lifts walkable (up-facing) surfaces, plus texture-driven INWARD relief (surface only ever moves DOWN into the
// original mesh: feet never sink, they hover by at most the local relief depth). Height = darkness / crack measure of the diffuse texture at several mips.
static std::string MakeTESWorld(const std::vector<TessVar>& vars) {
    std::string s = "#version 400\nlayout(triangles, fractional_odd_spacing, ccw) in;\nuniform mat4 VPMatrix;\n";
    bool hasUV = false; for (auto& v : vars) if (v.name == "v_uv") hasUV = true;
    if (hasUV) { s += "uniform sampler2D diffuseTexture;\n"; s += kTyHeightGLSL; }
    s += "uniform vec3 tyTCam; uniform vec4 tyTP; uniform vec4 tyTR; uniform vec4 tyTR2; uniform float tyTex;   // tyTex (shared with the FS): 1 = character texture, no displacement. tyTR: fade0 fade1 groundDepth propDepth; tyTR2: lodBias edgeFade\n";
    for (auto& v : vars) if (!v.name.empty()) s += "in " + v.type + " teIn_" + v.name + "[];\nout " + v.type + " " + v.name + ";\n";
    s += "in vec3 teIn_tyWorld[];\n";
    char buf[400];
    snprintf(buf, sizeof buf, "const float kAlpha = %.3f, kFade0 = %.1f, kFade1 = %.1f, kMaxSm = %.2f, kDisp = %.2f, kDispLod = %.2f, kCurve = %.3f, kKnee = %.2f, kKneeSlope = %.3f;\n", g_tsAlphaWorld, g_tsFadeStart, g_tsFadeEnd, g_tsMaxSmooth,
             g_tsReliefDepth > 0.0f ? 0.0f : g_tsDisp, g_tsDispLod, g_tsReliefCurve, g_tsReliefKnee, g_tsReliefKneeSlope);
    s += buf;
    s += R"GLSL(
vec3 PhongProj(vec3 p, vec3 q, vec3 n) { return q - dot(q - p, n) * n; }
float Lum(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
float TyH(vec3 i) { vec3 h = 50.0*fract(i*0.3183099+vec3(0.71,0.113,0.419)); return fract(h.x*h.y*h.z*(h.x+h.y+h.z)); }
float Nz3(vec3 x) { vec3 i = floor(x), f = fract(x); f = f*f*(3.0-2.0*f);
  return mix(mix(mix(TyH(i),TyH(i+vec3(1,0,0)),f.x),mix(TyH(i+vec3(0,1,0)),TyH(i+vec3(1,1,0)),f.x),f.y), mix(mix(TyH(i+vec3(0,0,1)),TyH(i+vec3(1,0,1)),f.x),mix(TyH(i+vec3(0,1,1)),TyH(i+vec3(1,1,1)),f.x),f.y), f.z) * 2.0 - 1.0; }
void main() {
  vec3 b = gl_TessCoord;
  vec3 P0 = teIn_tyWorld[0], P1 = teIn_tyWorld[1], P2 = teIn_tyWorld[2];
  vec3 N0 = normalize(teIn_v_normal[0]), N1 = normalize(teIn_v_normal[1]), N2 = normalize(teIn_v_normal[2]);
  vec3 p = b.x * P0 + b.y * P1 + b.z * P2;
  vec3 nI = normalize(b.x * N0 + b.y * N1 + b.z * N2);
  float dist = distance(p, tyTCam);
  vec3 ph = b.x * PhongProj(P0, p, N0) + b.y * PhongProj(P1, p, N1) + b.z * PhongProj(P2, p, N2);
  vec3 off = (ph - p) * (kAlpha * (1.0 - smoothstep(kFade0, kFade1, dist)));
  float ol = length(off); if (ol > kMaxSm) off *= kMaxSm / ol;
  float ground = smoothstep(0.35, 0.65, nI.y);
  off -= nI * (max(dot(off, nI), 0.0) * ground);          // walkable surfaces: smoothing may only go down
  vec3 w = p + off;
)GLSL";
    if (hasUV) s += R"GLSL(
  {
    vec2 u0 = teIn_v_uv[0], u1 = teIn_v_uv[1], u2 = teIn_v_uv[2];
    vec2 tuv = b.x * u0 + b.y * u1 + b.z * u2;
    float L = max(gl_TessLevelInner[0], 1.0);
    float aW = length(cross(P1 - P0, P2 - P0));
    float aT = abs((u1.x - u0.x) * (u2.y - u0.y) - (u2.x - u0.x) * (u1.y - u0.y));
    vec2 ts = vec2(textureSize(diffuseTexture, 0));
    float rf = (1.0 - smoothstep(tyTR.x, tyTR.y, dist)) * (1.0 - tyTex);
    if (tyTR.z + tyTR.w > 0.0 && rf > 0.0 && aT > 1e-12 && aW > 1e-3) {
      float tpu = sqrt(aT * ts.x * ts.y / aW);                         // texels per world unit
      float fp = max(tyTP.z, sqrt(aW) / L);                           // world size of one sub-triangle (never finer than TargetEdge: stable near, filtered far)
      float lod = clamp(log2(max(fp * tpu, 1.0)) + tyTR2.x, 0.0, 9.0);
      vec2 gx = vec2(exp2(lod) / ts.x, 0.0), gy = vec2(0.0, exp2(lod) / ts.y);
      float depthN = pow(clamp(-TyH3(tuv, gx, gy), 0.0, 1.0), kCurve);   // inward only: cracks/dark lines go down, nothing rises (feet never sink)
      float amp = mix(tyTR.w, tyTR.z, ground) * depthN;                 // world units
      if (kKnee > 0.0 && amp > kKnee) amp = kKnee + (amp - kKnee) * kKneeSlope;   // soft knee: strong relief stops growing past the knee
      float edgeF = tyTR2.y > 0.0 ? smoothstep(0.0, tyTR2.y, min(b.x, min(b.y, b.z)) * sqrt(aW)) : 1.0;   // world-unit fade width at patch borders
      w -= nI * (amp * rf * edgeF);
    }
  }
)GLSL";
    if (hasUV) s += R"GLSL(
  if (kDisp > 0.0) {   // legacy +-Displacement (ReliefDepth=0)
    vec2 tuv = b.x * teIn_v_uv[0] + b.y * teIn_v_uv[1] + b.z * teIn_v_uv[2];
    vec3 hc0 = textureLod(diffuseTexture, tuv, kDispLod).rgb, hc1 = textureLod(diffuseTexture, tuv, kDispLod + 3.0).rgb;
    float hkeep = 1.0 - smoothstep(0.10, 0.40, length(hc0 / (Lum(hc0) + 0.08) - hc1 / (Lum(hc1) + 0.08)));
    float hgt = (Lum(hc0) - Lum(hc1)) * 2.0 * hkeep;
    w += nI * (hgt * 2.0 * kDisp * (1.0 - smoothstep(kFade0, kFade1, dist)));
  }
)GLSL";
    s += "  gl_Position = VPMatrix * vec4(w, 1.0);\n";
    for (auto& v : vars) if (!v.name.empty()) s += "  " + v.name + " = b.x * teIn_" + v.name + "[0] + b.y * teIn_" + v.name + "[1] + b.z * teIn_" + v.name + "[2];\n";
    bool hasColour = false; for (auto& v : vars) if (v.name == "v_colour") hasColour = true;
    if (g_tsDebug && hasColour) s += "  if (min(b.x, min(b.y, b.z)) < 0.02) v_colour.rgb *= 0.15;\n";
    s += "}\n";
    return s;
}
static std::string MakeTCS(const std::vector<TessVar>& vars) { return TessVarsSkin(vars) ? MakeTCSLegacy(vars) : MakeTCSWorld(vars); }
static std::string MakeTES(const std::vector<TessVar>& vars) { return TessVarsSkin(vars) ? MakeTESLegacy(vars) : MakeTESWorld(vars); }

static GLuint CompileStage(GLenum type, const std::string& src) {
    if (g_dbgForce & 8) { static int once; if (!once++) logf("[fallback] FORCED tessellation stage compile failure -> programs relink without tessellation"); return 0; }
    typedef void(WINAPI* cs_t)(GLuint); typedef void(WINAPI* giv_t)(GLuint, GLenum, GLint*); typedef void(WINAPI* gil_t)(GLuint, GLsizei, GLsizei*, GLchar*);
    static cs_t compile = (cs_t)o_wglGetProcAddress("glCompileShader"); static giv_t getiv = (giv_t)o_wglGetProcAddress("glGetShaderiv"); static gil_t getlog = (gil_t)o_wglGetProcAddress("glGetShaderInfoLog");
    GLuint sh = o_glCreateShader(type); const GLchar* p = src.c_str(); o_glShaderSource(sh, 1, &p, nullptr); compile(sh);
    GLint ok = 0; getiv(sh, 0x8B81, &ok);
    if (!ok) { char b[2048]; getlog(sh, sizeof b, nullptr, b); logf("[tess] stage %#x compile FAILED:\n%s\n--- source ---\n%s", type, b, src.c_str()); return 0; }
    return sh;
}

// called from the glLinkProgram hook before the real link. Returns true if TCS/TES were attached.
static bool TessBeforeLink(GLuint prog) {
    auto it = g_progShaders.find(prog); if (it == g_progShaders.end()) return false;
    const std::vector<TessVar>* vars = nullptr;
    for (GLuint sh : it->second) { auto t = g_tessVS.find(sh); if (t != g_tessVS.end()) vars = &t->second; }
    if (!vars) return false;
    if (g_portTessVerGate && !GLAtLeast(4, 0)) { static int once; if (!once++) logf("[tess] GL below 4.0: tessellation stages skipped (TessVersionGate=1)"); return false; }
    if (!TessEntryPointsOk()) return false;
    std::string sig = TessSignature(*vars);
    auto st = g_tessStages.find(sig);
    if (st == g_tessStages.end()) {
        GLuint tcs = CompileStage(0x8E88 /*TESS_CONTROL*/, MakeTCS(*vars)), tes = tcs ? CompileStage(0x8E87 /*TESS_EVALUATION*/, MakeTES(*vars)) : 0;
        st = g_tessStages.emplace(sig, std::make_pair(tcs, tes)).first;
    }
    if (!st->second.first || !st->second.second) return false;
    o_glAttachShader(prog, st->second.first); o_glAttachShader(prog, st->second.second);
    return true;
}
// called after the real link when TessBeforeLink returned true: verify, else detach + relink plain (VS still has renamed outputs, so
// the plain program would fail too -> in that case the VS must not have been rewritten; we keep a copy of the original source to recompile)
static std::unordered_map<GLuint, std::string> g_tessOrigVS;
static void TessAfterLink(GLuint prog) {
    typedef void(WINAPI* gpiv_t)(GLuint, GLenum, GLint*); typedef void(WINAPI* gpil_t)(GLuint, GLsizei, GLsizei*, GLchar*); typedef void(WINAPI* det_t)(GLuint, GLuint); typedef void(WINAPI* cs_t)(GLuint);
    static gpiv_t getiv = (gpiv_t)o_wglGetProcAddress("glGetProgramiv"); static gpil_t getlog = (gpil_t)o_wglGetProcAddress("glGetProgramInfoLog");
    static det_t detach = (det_t)o_wglGetProcAddress("glDetachShader"); static cs_t compile = (cs_t)o_wglGetProcAddress("glCompileShader");
    GLint ok = 0; getiv(prog, 0x8B82, &ok);
    if (ok) { g_tessProgs.insert(prog);
        TessLoc tl; tl.cam = o_glGetUniformLocation(prog, "tyTCam"); tl.tp = o_glGetUniformLocation(prog, "tyTP"); tl.cull = o_glGetUniformLocation(prog, "tyTCull");
        tl.tr = o_glGetUniformLocation(prog, "tyTR"); tl.tr2 = o_glGetUniformLocation(prog, "tyTR2"); tl.lod = o_glGetUniformLocation(prog, "tyTL"); g_tessLoc[prog] = tl; g_tessCullLast.erase(prog); g_tessStamp.erase(prog);
        logf("[tess] program %u linked WITH tessellation", prog); return; }
    char b[2048]; getlog(prog, sizeof b, nullptr, b); logf("[tess] program %u tessellated link FAILED -> restoring original:\n%s", prog, b);
    for (auto& kv : g_tessStages) { if (kv.second.first) detach(prog, kv.second.first); if (kv.second.second) detach(prog, kv.second.second); }
    for (GLuint sh : g_progShaders[prog]) { auto o = g_tessOrigVS.find(sh); if (o != g_tessOrigVS.end()) { const GLchar* p = o->second.c_str(); o_glShaderSource(sh, 1, &p, nullptr); compile(sh); g_tessVS.erase(sh); } }
    o_glLinkProgram(prog);
}

// per-frame static uniforms of a tessellated program (called from PbrApply, i.e. glUseProgram)
static float g_tsP11 = 0.0f;   // projection scale of the scene camera (set by PbrUpdate)
static void TessApply(GLuint p);   // defined in pbr.inl (needs the camera state)
// per tessellated draw: tell the TCS which screen winding the engine currently culls (0 = nothing culled -> no back-face patch culling)
static void TessDrawPrep() {
    if (!g_tsCull) return;
    auto it = g_tessLoc.find(g_curProg); if (it == g_tessLoc.end() || it->second.cull < 0) return;
    typedef unsigned char(WINAPI* ie_t)(GLenum); typedef void(WINAPI* gi_t)(GLenum, GLint*); typedef void(WINAPI* pu1_t)(GLuint, GLint, GLfloat);
    static HMODULE gl = GetModuleHandleA("opengl32.dll"); static ie_t isEn = gl ? (ie_t)GetProcAddress(gl, "glIsEnabled") : nullptr; static gi_t geti = gl ? (gi_t)GetProcAddress(gl, "glGetIntegerv") : nullptr;
    static pu1_t pu1 = (pu1_t)o_wglGetProcAddress("glProgramUniform1f"); if (!isEn || !geti || !pu1) return;
    float v = 0.0f;
    if (isEn(0x0B44 /*CULL_FACE*/)) { GLint mode = 0x0405, front = 0x0901; geti(0x0B45, &mode); geti(0x0B46, &front); if (mode == 0x0405 /*BACK*/) v = front == 0x0901 ? 1.0f : -1.0f; }
    auto lc = g_tessCullLast.find(g_curProg); if (lc != g_tessCullLast.end() && lc->second == v) return;
    g_tessCullLast[g_curProg] = v; pu1(g_curProg, it->second.cull, v);
}
