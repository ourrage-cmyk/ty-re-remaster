// light.inl — natural lighting for the engine's shaders (shadow tint, hemisphere ambient, foliage translucency) + shared per-program parameter block.
// LightRewriteFS(s): called in glShaderSource after the PBR + shadow-patch rewrites, on every fragment shader; returns true if modified.
//   * shadow tint:   the engine darkens shadowed pixels with a neutral `diffuse *= 1 - shadowAmount*0.5`. We multiply by a cool, saturated shade colour
//                    (derived from the level fog colour + sky blue) weighted by shadowAmount, and re-saturate, so shadows read as sky-lit shade instead of grey.
//   * hemisphere:    normal-based sky (cool, from above) / ground-bounce (warm, from below) modulation of the vertex/dynamic light on lit variants.
//   * foliage:       grass keeps its own saturated colour in shade and is lifted (light transmitted through leaves) instead of greying out.
// All values are uniforms (tyLt[6], set per program per frame by LightApply) -> live reload via tyrender\reload. See EnsureLtDecl in tyrender.cpp for the layout.
// Config tyrender.ini [light]: Enable=1 ShadowTint=1 ShadowTintAmount=0.45 TintFromFog=0.35 ShadowSaturation=0.25 Hemisphere=1 HemisphereAmount=0.30 HemisphereStatic=0.5
//                              Foliage=1 FoliageAmount=0.5
static int g_ltEnable = 1, g_ltShadowTint = 1, g_ltHemi = 1, g_ltFoliage = 1;
static float g_ltTintAmt = 1.0f, g_ltFogMix = 0.15f, g_ltResat = 0.0f, g_ltHemiAmt = 0.10f, g_ltHemiStatic = 0.5f, g_ltFoliageAmt = 0.10f, g_ltFill = 0.015f, g_ltCool = 0.22f, g_ltDepth = 0.12f; static int g_ltSkipSkin = 1;
static float g_shZOff = 0.0f, g_shSkinBias = 0.0f, g_shClose = 0.0f; static int g_ltView = 0, g_shPlane = 0; static float g_shPlaneClamp = 0.004f;
static std::unordered_map<GLuint, GLint> g_ltLoc; static std::unordered_map<GLuint, unsigned> g_ltStamp;
static float g_ltSky[3] = {0.50f, 0.66f, 1.0f}; static bool g_ltSkyInit;

static void LoadLightConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_ltEnable = GetPrivateProfileIntA("light", "Enable", 1, ini); g_ltShadowTint = GetPrivateProfileIntA("light", "ShadowTint", 1, ini);
    g_ltSkipSkin = GetPrivateProfileIntA("light", "SkipCharacters", 1, ini); g_ltHemi = GetPrivateProfileIntA("light", "Hemisphere", 1, ini); g_ltFoliage = GetPrivateProfileIntA("light", "Foliage", 1, ini);
#define LF(key, def, var) GetPrivateProfileStringA("light", key, def, b, sizeof b, ini); var = (float)atof(b);
    LF("ShadowTintAmount", "1.0", g_ltTintAmt) LF("TintFromFog", "0.15", g_ltFogMix) LF("ShadowSaturation", "0.0", g_ltResat) LF("ShadeCool", "0.22", g_ltCool) LF("ShadowDepth", "0.12", g_ltDepth)
    LF("HemisphereAmount", "0.10", g_ltHemiAmt) LF("HemisphereStatic", "0.5", g_ltHemiStatic) LF("FoliageAmount", "0.10", g_ltFoliageAmt) LF("ShadowFill", "0.015", g_ltFill)
#undef LF
    g_ltView = GetPrivateProfileIntA("debug", "ShadowView", 0, ini);   // restart-only: 1 = show the shadow term alone (white = lit, black = shadow)
    g_shPlane = GetPrivateProfileIntA("shadows", "PlaneBias", 1, ini);   // #103: receiver-plane bias on by default (set 0 to revert)
    GetPrivateProfileStringA("shadows", "PlaneClamp", "0.004", b, sizeof b, ini); g_shPlaneClamp = (float)atof(b);
    logf("[light] config (%s): view=%d plane=%d clamp=%.4f slices=%d mapscale=%d", ini, g_ltView, g_shPlane, g_shPlaneClamp, GetPrivateProfileIntA("ao", "Slices", -1, ini), GetPrivateProfileIntA("shadows", "MapScale", -1, ini));
    GetPrivateProfileStringA("shadows", "SlopeBias", "1.0", b, sizeof b, ini); g_shZOff = (float)atof(b);
    GetPrivateProfileStringA("shadows", "SkinBias", "0", b, sizeof b, ini); g_shSkinBias = (float)atof(b);
    GetPrivateProfileStringA("shadows", "CloseCracks", "0", b, sizeof b, ini); g_shClose = (float)atof(b);   // live, optional: 1 = fill 1-3 texel light gaps between casters (grid filter only); not needed for S1
    g_ltStamp.clear();
}

static bool LightRewriteFS(std::string& s) {
    if (!g_ltEnable) return false;
    if (g_dbgForce & 16) { static int once; if (!once++) logf("[fallback] FORCED invalid light shader rewrite -> must fall back"); s += "\n#error forced fallback test\n"; return true; }
    bool grass = s.find("uniform float alphaRef;") != std::string::npos && s.find("lightDirMatrix") == std::string::npos && s.find("diffuseColour.w = 1.0;") != std::string::npos;
    const char* kRecv = "diffuseColour.xyz *= lightReceive;";
    // debug view (live, [debug] ShadowView=1 + reload): shows the shadow term alone. Marker is consumed below (tint) or here (characters / no tint).
    const char* kView = " if (tyLt[6].y > 0.5) diffuseColour.xyz = vec3(1.0 - shadowAmount * 2.0);";
    if (s.find(kRecv) != std::string::npos && s.find("float shadowAmount") != std::string::npos) {
        EnsureLtDecl(s); ReplaceAll(s, kRecv, std::string(kRecv) + kView); }
    bool doTint = g_ltShadowTint && s.find(kRecv) != std::string::npos && s.find("float shadowAmount") != std::string::npos;
    bool doHemi = g_ltHemi && s.find("diffuseColour *= light;") != std::string::npos && s.find("P_IN lowp vec3 v_normal;") != std::string::npos && !grass;
    if (g_ltSkipSkin && PbrDefine(s, "SKIN", 1)) { doTint = false; doHemi = false; }
    if (g_dbgWater & 8) { doTint = false; doHemi = false; }   // DebugWater bit 8 (issue #5 bisect): all programs   // characters keep the engine shading (no tint)
    if (!doTint && !doHemi) return s.find(kView) != std::string::npos;
    EnsureLtDecl(s);
    std::string h = "const vec3 TY_LUM = vec3(0.2126, 0.7152, 0.0722);\n";
    if (doTint) {
        h += "vec3 TyShade(vec3 c, float sa, float lr) {\n"
             "  sa = clamp(sa, 0.0, 1.0);\n"
             "  c *= vec3(max(lr - sa * tyLt[5].w, 0.0)) * mix(vec3(1.0), tyLt[1].rgb, sa * tyLt[1].w);          // cool, sky-lit shade\n"
             "  float l = dot(c, TY_LUM);\n"
             "  c = max(mix(vec3(l), c, 1.0 + tyLt[3].w * sa), 0.0);                  // shade keeps (gains) saturation\n  return c + tyLt[5].rgb * sa;                                          // cool sky fill light (red surfaces get a violet-blue shade)\n"
             "}\n"
             "vec3 TyShadeFoliage(vec3 c, float sa, float lr) {\n"
             "  sa = clamp(sa, 0.0, 1.0); float f = tyLt[4].x;\n"
             "  c *= vec3(mix(lr, 1.0, 0.25 * f)) * mix(vec3(1.0), tyLt[1].rgb, sa * tyLt[1].w * 0.5);   // leaves transmit light: shade is lifted\n"
             "  float l = dot(c, TY_LUM);\n"
             "  c = mix(vec3(l), c, 1.0 + tyLt[3].w * sa);\n"
             "  return max(c, 0.0);\n"
             "}\n";
    }
    if (doHemi) {
        h += "vec3 TyHemi(vec3 n, float k) {\n"
             "  float u = clamp(n.y, -1.0, 1.0);\n"
             "  vec3 hc = mix(tyLt[3].rgb, tyLt[2].rgb, u * 0.5 + 0.5) * (1.0 + 0.12 * u);\n"
             "  return mix(vec3(1.0), hc, tyLt[2].w * k);\n"
             "}\n";
    }
    s.insert(s.rfind("void main"), h);
    if (doTint) ReplaceAll(s, kRecv, grass && g_ltFoliage ? "diffuseColour.xyz = TyShadeFoliage(diffuseColour.xyz, shadowAmount, lightReceive);"
                                                          : "diffuseColour.xyz = TyShade(diffuseColour.xyz, shadowAmount, lightReceive);");
    if (doHemi) {
        const char* nrm = s.find("vec3 tyN = normalize(v_normal);") != std::string::npos ? "tyN" : "normalize(v_normal)";   // PBR-bumped normal when present
        std::string blk = std::string("#if (LIT == 1) || (SHADOWNORMS == 1)\n  light.xyz *= TyHemi(") + nrm + ", LIT == 1 ? 1.0 : tyLt[4].y);\n#endif\n  diffuseColour *= light;";
        size_t p = s.find("diffuseColour *= light;"); if (p != std::string::npos) s.replace(p, strlen("diffuseColour *= light;"), blk);
    }
    return true;
}

static void LightOnLink(GLuint p) {
    GLint l = o_glGetUniformLocation(p, "tyLt");
    if (l >= 0) g_ltLoc[p] = l;
}
// per-frame parameter block (same for all programs); smoothed sky colour follows the engine's fog colour
static float g_ltBlock[8][4];
static unsigned g_ltBlockFrame = ~0u;
static void LightFrame() {
    if (g_ltBlockFrame == g_frame) return; g_ltBlockFrame = g_frame;
    float tgt[3] = {0.50f, 0.66f, 1.0f};
    if (g_haveFog && g_ltFogMix > 0.0f) {
        float f[3] = {g_fogColour[0], g_fogColour[1], g_fogColour[2]}, fl = 0.2126f * f[0] + 0.7152f * f[1] + 0.0722f * f[2];
        float fs[3]; for (int i = 0; i < 3; i++) fs[i] = std::min(std::max(fl + (f[i] - fl) * 1.8f, 0.05f), 1.0f);
        float mx = std::max(fs[0], std::max(fs[1], fs[2])); for (int i = 0; i < 3; i++) fs[i] /= mx;
        if (fs[2] < fs[0] * 1.1f) fs[2] = fs[0] * 1.1f;   // shade is never warmer than the sky
        for (int i = 0; i < 3; i++) tgt[i] = tgt[i] + (fs[i] - tgt[i]) * g_ltFogMix;
    }
    if (!g_ltSkyInit) { for (int i = 0; i < 3; i++) g_ltSky[i] = tgt[i]; g_ltSkyInit = true; }
    else for (int i = 0; i < 3; i++) g_ltSky[i] += (tgt[i] - g_ltSky[i]) * 0.05f;
    float lum = 0.2126f * g_ltSky[0] + 0.7152f * g_ltSky[1] + 0.0722f * g_ltSky[2];
    float T[3], skyM[3], gndM[3];
    for (int i = 0; i < 3; i++) { T[i] = 1.0f + (std::min(g_ltSky[i] / lum, 1.55f) - 1.0f) * g_ltCool; skyM[i] = 1.0f + (T[i] - 1.0f) * 0.40f; }
    float gl = 0.2126f * (1.0f / skyM[0]) + 0.7152f * (1.0f / skyM[1]) + 0.0722f * (1.0f / skyM[2]);
    gndM[0] = (1.0f / skyM[0]) / gl * 1.04f; gndM[1] = (1.0f / skyM[1]) / gl; gndM[2] = (1.0f / skyM[2]) / gl * 0.94f;   // complementary warm bounce
    bool taa = g_jitterOn;
    int taps = (taa && g_shTemporal) ? std::max(4, g_shTaps / 2) : g_shTaps;   // stable noise (Temporal=0) cannot be integrated by TAA -> keep full taps
    const bool ltOn = g_ltEnable && !g_abOff;   // A/B off frame (abshot.inl): the light features read as Enable=0
    g_ltBlock[0][0] = g_shRadius; g_ltBlock[0][1] = (taa && g_shTemporal) ? (float)(g_frame % 8) * 5.588238f : 0.0f; g_ltBlock[0][2] = (float)taps; g_ltBlock[0][3] = 0.8f;
    for (int i = 0; i < 3; i++) { g_ltBlock[1][i] = T[i]; g_ltBlock[2][i] = skyM[i]; g_ltBlock[3][i] = gndM[i]; }
    g_ltBlock[1][3] = ltOn && g_ltShadowTint ? g_ltTintAmt : 0.0f;
    g_ltBlock[2][3] = ltOn && g_ltHemi ? g_ltHemiAmt : 0.0f;
    g_ltBlock[3][3] = ltOn && g_ltShadowTint ? g_ltResat : 0.0f;
    g_ltBlock[4][0] = ltOn && g_ltFoliage ? g_ltFoliageAmt : 0.0f; g_ltBlock[4][1] = g_ltHemiStatic; g_ltBlock[4][2] = g_shGrid ? 0.0f : 1.0f; g_ltBlock[4][3] = g_shClose;
    for (int i = 0; i < 3; i++) g_ltBlock[5][i] = ltOn && g_ltShadowTint ? g_ltFill * g_ltSky[i] : 0.0f; g_ltBlock[5][3] = ltOn && g_ltShadowTint ? g_ltDepth : 0.0f;
    g_ltBlock[7][0] = g_shSkinBias; g_ltBlock[6][0] = g_shPlane ? 1.0f : 0.0f; g_ltBlock[6][1] = g_ltView ? 1.0f : 0.0f; g_ltBlock[6][2] = g_shPlaneClamp; g_ltBlock[6][3] = g_shZOff;
    static bool once; if (!once && g_haveFog) { once = true; logf("[light] fog=%.2f,%.2f,%.2f -> shade tint %.2f,%.2f,%.2f", g_fogColour[0], g_fogColour[1], g_fogColour[2], T[0], T[1], T[2]); }
}
static void LightApply(GLuint p) {
    auto it = g_ltLoc.find(p); if (it == g_ltLoc.end()) return;
    unsigned& st = g_ltStamp[p]; if (st == g_frame + 1) return; st = g_frame + 1;
    typedef void(WINAPI* pu4v_t)(GLuint, GLint, GLsizei, const GLfloat*);
    static pu4v_t pu = (pu4v_t)o_wglGetProcAddress("glProgramUniform4fv"); if (!pu) return;
    LightFrame();
    pu(p, it->second, 8, &g_ltBlock[0][0]);
}



