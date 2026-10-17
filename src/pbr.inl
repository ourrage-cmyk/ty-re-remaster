// pbr.inl — material upgrade for the engine's 'standard' lit variants (LIT=1, TEX=1), done by rewriting the shaders at glShaderSource time.
// No extra assets: the game has only a diffuse texture, so detail is derived from it per pixel.
//  * VS: adds a world-position varying (v_tyWorld).  FS: perturbs the lighting normal with Mikkelsen's unparametrised bump mapping
//    (screen-space derivatives of the albedo luminance, suppressed where hue changes = painted shapes), so the engine's own light model
//    (lightDirMatrix / omni lights) lights the micro relief.  Then adds GGX specular from the sun (shadowDir) + a Fresnel sky sheen.
//  * Per-program uniforms tyCam / tySunDir / tyPbr are set from glUseProgram (camera derived from the scene VP, sun from shadowDir).
// Config tyrender.ini [pbr]: Enable=1 NormalStrength=1.0 Specular=0.5 Roughness=0.65 HeightScale=3.0 FadeStart=1500 FadeEnd=3500 Debug=0
#include <array>
static int g_pbrEnable = 1, g_pbrDebug = 0, g_pbrLitProps = 1; static float g_pbrNormal = 1.0f, g_pbrSpec = 0.5f, g_pbrRough = 0.65f, g_pbrHeight = 3.0f, g_pbrFade0 = 1500.0f, g_pbrFade1 = 3500.0f;
static float g_pbrCam[3], g_pbrSun[3] = {0.3f, 0.8f, 0.3f}; static bool g_pbrHaveCam;
static float g_tsDetail = 1.0f, g_tsDetailScale = 1.0f, g_tsDetailTex = 1.0f, g_tsDetailDisp = 0.8f;
static float g_pbrLowBump = 0.6f, g_pbrCavity = 0.30f, g_pbrValleyRough = 0.30f, g_pbrSheen = 0.12f;
static std::unordered_map<GLuint, std::array<GLint, 7>> g_pbrLocs;   // program -> tyCam, tySunDir, tyPbr
static std::unordered_map<GLuint, unsigned> g_pbrStamp;              // program -> last frame uniforms were applied

static void LoadPbrConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_pbrEnable = GetPrivateProfileIntA("pbr", "Enable", 1, ini); g_pbrLitProps = GetPrivateProfileIntA("pbr", "LitProps", 1, ini); g_pbrDebug = GetPrivateProfileIntA("pbr", "Debug", 0, ini);   // LitProps=0: dynamically lit (LIT=1) non-skinned props stay stock too
    g_texWaterNoFresnel = GetPrivateProfileIntA("pbr", "SkyFresnelSkipWater", 0, ini);   // 1: water-tagged draws (emissive/transparent) get no sky Fresnel term; 0 = unchanged
#define PF(key, def, var) GetPrivateProfileStringA("pbr", key, def, b, sizeof b, ini); var = (float)atof(b);
    PF("NormalStrength", "1.0", g_pbrNormal) PF("Specular", "0.5", g_pbrSpec) PF("Roughness", "0.65", g_pbrRough) PF("HeightScale", "3.0", g_pbrHeight)
    PF("LowFreqBump", "0.6", g_pbrLowBump) PF("Cavity", "0.30", g_pbrCavity) PF("ValleyRough", "0.30", g_pbrValleyRough) PF("Sheen", "0.12", g_pbrSheen)
    PF("DetailStrength", "1.0", g_tsDetail) PF("DetailScale", "1.0", g_tsDetailScale) PF("DetailTexture", "1.0", g_tsDetailTex) PF("DetailDisp", "0.8", g_tsDetailDisp)
    PF("FadeStart", "1500", g_pbrFade0) PF("FadeEnd", "3500", g_pbrFade1)
#undef PF
}

static bool PbrDefine(const std::string& s, const char* name, int val) {
    std::string d = std::string("#define ") + name + " " + std::to_string(val);
    size_t p = s.find(d); return p != std::string::npos && !isdigit((unsigned char)(p + d.size() < s.size() ? s[p + d.size()] : ' '));
}
static bool PbrEligible(const std::string& s) {
    if (!g_pbrEnable) return false;
    if (PbrDefine(s, "SKIN", 1) && !g_texSkinTags) return false;    // skinned shaders stay stock unless the texture tags decide per draw (tyTex uniform, tex.inl)
    return PbrDefine(s, "LIT", 1) && PbrDefine(s, "TEX", 1) && (g_pbrLitProps || !PbrDefine(s, "LIT", 1));
}

// "lite": vertex-lit world surfaces (LIT=0) that still carry normals (SHADOWNORMS=1): sand/ground, fences, rocks. Gets only the detail layer (albedo/grain/fake relight), no GGX.
static bool PbrLite(const std::string& s) {
    return g_pbrEnable && PbrDefine(s, "LIT", 0) && PbrDefine(s, "SHADOWNORMS", 1) && PbrDefine(s, "TEX", 1) && PbrDefine(s, "AREF", 0) && (g_texSkinTags || PbrDefine(s, "SKIN", 0));
}
static bool PbrRewriteVS(std::string& s) {
    if (!PbrEligible(s) && !PbrLite(s)) return false;
    size_t gp = s.find("gl_Position = VPMatrix * worldPos;"); size_t mp = s.find("void main");
    if (gp == std::string::npos || mp == std::string::npos || s.find("V_OUT lowp vec3 v_normal;") == std::string::npos) return false;
    s.insert(gp + strlen("gl_Position = VPMatrix * worldPos;"), "\n  v_tyWorld = worldPos.xyz;");
    s.insert(mp, "V_OUT vec3 v_tyWorld;\n");
    return true;
}

static bool PbrRewriteFS(std::string& s) {
    bool lite = !PbrEligible(s) && PbrLite(s);
    if (!PbrEligible(s) && !lite) return false;
    if (s.find("#if (LIT==1) || (OMNI==1)") == std::string::npos) return false;
    if (!lite && (s.find("diffuseColour *= light;") == std::string::npos || s.find("normalize(v_normal)") == std::string::npos)) return false;
    char hdr[1500];
    snprintf(hdr, sizeof hdr,
        "in vec3 v_tyWorld;\nuniform vec3 tyCam; uniform vec3 tySunDir; uniform vec4 tyPbr; uniform vec4 tyPbr2; uniform vec4 tyPbr3; uniform vec4 tyRel; uniform vec4 tyRel2; uniform float tyTex; uniform float tyWater;\n"
        "// tyPbr: x normal strength, y specular, z roughness, w height scale; tyPbr2: x low-freq share kept in bump, y cavity, z valley roughness, w sheen;\n"
        "// tyPbr3.x relief strength; tyRel: x depth (world units) y ground mult z prop mult w 1=geometry already tessellated (parallax reduced); tyRel2: x fade0 y fade1 z max steps w mip bias\n"
        "float TyLum(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
        "float TyDist(float d) { return 1.0 - smoothstep(%.1f, %.1f, d); }\n", g_pbrFade0, g_pbrFade1);
    std::string hs = hdr;
    hs += kTyHeightGLSL;
    s.insert(s.rfind("void main"), hs);
    // 1) bumped normal, computed right before the lighting block so the engine's own lights use it
    const char* nblk = R"GLSL(
  vec3 tyN = normalize(v_normal);
  float tyCamDist = distance(v_tyWorld, tyCam);
  float tyOn = 1.0 - tyTex;   // tyTex=1: character / unknown skinned texture -> no bump, relief or specular (set per draw from the texture tags)
  float tyBumpW = TyDist(tyCamDist) * tyPbr.x * tyOn;
  float tyLumA = TyLum(diffuseColour.rgb); float tyChromaEdge = 0.0; float tyValley = 0.0;
  {
    vec3 chr = diffuseColour.rgb / (tyLumA + 0.08);
    tyChromaEdge = length(dFdx(chr)) + length(dFdy(chr));               // only used to raise roughness on painted hue edges
  }
  float tyRS = tyPbr3.x * tyOn * (1.0 - smoothstep(tyRel2.x, tyRel2.y, tyCamDist));
  if (tyRS > 0.01 && tyRel.x > 0.0) {   // ALBEDO-DERIVED RELIEF: height from the texture's own colour (cracks, paint edges, strands); no procedural noise
    vec3 dpx = dFdx(v_tyWorld), dpy = dFdy(v_tyWorld); vec2 ux = dFdx(v_uv), uy = dFdy(v_uv);
    float det = ux.x * uy.y - ux.y * uy.x;
    if (abs(det) > 1e-14) {
      float ir = 1.0 / det;
      vec3 T = (dpx * uy.y - dpy * ux.y) * ir, B = (dpy * ux.x - dpx * uy.x) * ir;   // world units per uv unit
      float lT = max(length(T), 1e-5), lB = max(length(B), 1e-5);
      vec2 ts = vec2(textureSize(diffuseTexture, 0));
      float foot = max(length(ux * ts), length(uy * ts));                           // pixel footprint in texels
      vec2 gx = ux * tyRel2.w, gy = uy * tyRel2.w;                                  // sampling gradients (mip bias)
      float gnd = smoothstep(0.35, 0.65, tyN.y);
      float D = tyRel.x * mix(tyRel.z, tyRel.y, gnd) * tyRS;                         // relief depth in world units
      vec3 V = normalize(tyCam - v_tyWorld); float NV = dot(V, tyN);
      vec2 uvP = v_uv; float hitD = 0.0;
      D *= smoothstep(0.06, 0.40, NV);                                             // fade the parallax out at grazing view angles (no streaking)
      if (NV > 0.06 && D > 0.02) {
        vec3 Vt = V - tyN * NV; vec3 ws = Vt / max(NV, 0.28);                       // world shift per unit depth
        vec2 duv = -vec2(dot(ws, T) / (lT * lT), dot(ws, B) / (lB * lB)) * D * (1.0 - 0.6 * tyRel.w);   // total uv shift at full depth
        int n = int(mix(3.0, tyRel2.z, 1.0 - NV)); float sd = 1.0 / float(n);
        float prevRay = 0.0, prevDep = 0.0, ray = 0.0; hitD = 0.0; bool hit = false;
        for (int i = 1; i <= 12; i++) {
          if (i > n) break;
          ray = float(i) * sd;
          float dep = TyDepth(TyH2(v_uv + duv * ray, gx, gy));
          if (dep <= ray) {       // ray went below the surface: refine with linear interpolation between this and the previous step
            float a0 = prevRay - prevDep, a1 = ray - dep; float t = a0 / max(a0 - a1, 1e-4);
            hitD = mix(prevRay, ray, clamp(t, 0.0, 1.0)); hit = true; break; }
          prevRay = ray; prevDep = dep;
        }
        if (!hit) hitD = 0.0;
        uvP = v_uv + duv * hitD;
        diffuseColour.rgb = textureGrad(diffuseTexture, uvP, ux, uy).rgb;           // albedo seen through the parallax shift
      }
      // per-pixel normal from the height field (central differences, step = one texel at the sampled footprint)
      float e = max(1.0, foot * tyRel2.w);
      vec2 ex = vec2(e / ts.x, 0.0), ey = vec2(0.0, e / ts.y);
      float h0 = TyH3(uvP, gx, gy), hx = TyH3(uvP + ex, gx, gy), hy = TyH3(uvP + ey, gx, gy);
      float hw = 0.75 * tyRel.x * mix(tyRel.z, tyRel.y, gnd);                       // world height of the full-scale field
      vec2 sl = vec2(hx - h0, hy - h0) * hw / vec2(e * lT / ts.x, e * lB / ts.y);   // world slope along T, B
      float sm = length(sl); if (sm > 1.4) sl *= 1.4 / sm;
      vec3 Tn = T / lT, Bn = B / lB;
      tyN = normalize(tyN - tyRS * tyBumpW * (sl.x * Tn + sl.y * Bn));
      tyValley = clamp(max(-h0, hitD * 1.5), 0.0, 1.0) * tyRS;
    }
  }
  diffuseColour.rgb *= 1.0 - tyPbr2.y * tyValley * TyDist(tyCamDist);        // cavities / cracks are darker
)GLSL";
    size_t lp = s.find("#if (LIT==1) || (OMNI==1)");
    if (lite) {   // vertex-lit surface: detail layer + fake relight of the bumped normal against the sun (baked vertex colour keeps the base lighting)
        std::string t = nblk;
        t += "  { vec3 N0 = normalize(v_normal); float sh = dot(tyN, tySunDir) - dot(N0, tySunDir); diffuseColour.rgb *= clamp(1.0 + 0.5 * sh * TyDist(tyCamDist) * tyPbr.x * tyOn, 0.5, 1.6); }\n";
        s.insert(lp, t); return true;
    }
    s.insert(lp, nblk);
    ReplaceAll(s, "normalize(v_normal),1.0)", "tyN,1.0)");                       // lightApply
    ReplaceAll(s, "dot(v_normal, normalize(toLight))", "dot(tyN, normalize(toLight))");
    // 2) GGX sun specular + Fresnel sky sheen, before shadows/fog so both still apply
    const char* sblk =
        "  diffuseColour *= light;\n"
        "  if (tyPbr.y > 0.0) {\n"
        "    vec3 V = normalize(tyCam - v_tyWorld), L = normalize(tySunDir), H = normalize(V + L);\n"
        "    float NoL = max(dot(tyN, L), 0.0), NoV = max(dot(tyN, V), 1e-3), NoH = max(dot(tyN, H), 0.0), VoH = max(dot(V, H), 0.0);\n"
        "    float rough = clamp(tyPbr.z - 0.30 * smoothstep(0.45, 0.95, tyLumA) + 0.15 * smoothstep(0.1, 0.5, tyChromaEdge) + tyPbr2.z * tyValley, 0.25, 1.0);\n"
        "    float a = rough * rough, a2 = a * a;\n"
        "    float dd = NoH * NoH * (a2 - 1.0) + 1.0; float D = a2 / (3.14159265 * dd * dd);\n"
        "    float Vis = 0.5 / max(mix(2.0 * NoL * NoV, NoL + NoV, a), 1e-4);\n"
        "    float F = 0.04 + 0.96 * pow(1.0 - VoH, 5.0);\n"
        "    float occl = clamp(dot(v_colour.rgb, vec3(0.3333)), 0.0, 1.0);        // baked vertex light/AO gates the highlight\n"
        "    float fade = TyDist(tyCamDist) * tyPbr.y * tyOn;\n"
        "    float spec = min(D * Vis * F * NoL, 4.0) * fade * occl;\n"
        "    float Fr = (0.04 + 0.96 * pow(1.0 - NoV, 5.0)) * (1.0 - tyWater);     // tyWater=1 (ini SkyFresnelSkipWater, water draws): no sky Fresnel\n"
        "    vec3 env = vec3(0.55, 0.65, 0.85);\n"
        "    diffuseColour.rgb += spec * vec3(1.0, 0.94, 0.82) + Fr * (1.0 - rough) * 0.35 * fade * occl * env;\n"
        "    diffuseColour.rgb *= 1.0 + tyPbr2.w * pow(1.0 - NoV, 3.0) * rough * fade * occl * (1.0 - 0.5 * tyValley);   // soft dust / fibre sheen at grazing angles (albedo-coloured, not plastic)\n"
        "  }\n";
    ReplaceAll(s, "  diffuseColour *= light;\n", sblk);
    if (g_pbrDebug == 1) s.insert(s.rfind("FINAL_COLOR = diffuseColour;"), "diffuseColour.rgb = tyN * 0.5 + 0.5;\n  ");   // normals view
    return true;
}

static void PbrOnLink(GLuint p) {
    GLint a = o_glGetUniformLocation(p, "tyCam"), b = o_glGetUniformLocation(p, "tySunDir"), c = o_glGetUniformLocation(p, "tyPbr"), d = o_glGetUniformLocation(p, "tyPbr2"), e = o_glGetUniformLocation(p, "tyPbr3");
    GLint f = o_glGetUniformLocation(p, "tyRel"), g = o_glGetUniformLocation(p, "tyRel2");
    if (p < kTexProgs) { g_progTyTexLoc[p] = o_glGetUniformLocation(p, "tyTex"); g_progTyTexLast[p] = -1.0f; g_progTyWaterLoc[p] = o_glGetUniformLocation(p, "tyWater"); g_progTyWaterLast[p] = -1.0f; }
    if (a >= 0 || b >= 0 || c >= 0) { g_pbrLocs[p] = {a, b, c, d, e, f, g}; static int nlog; if (nlog++ < 6) logf("[pbr] prog %u locs cam%d sun%d pbr%d pbr2%d pbr3%d", p, a, b, c, d, e); }
}
static void TessApply(GLuint p) {
    auto it = g_tessLoc.find(p); if (it == g_tessLoc.end()) return;
    unsigned& st = g_tessStamp[p]; if (st == g_frame + 1) return; st = g_frame + 1;
    typedef void(WINAPI* pu3_t)(GLuint, GLint, GLfloat, GLfloat, GLfloat); typedef void(WINAPI* pu4_t)(GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    static pu3_t pu3 = (pu3_t)o_wglGetProcAddress("glProgramUniform3f"); static pu4_t pu4 = (pu4_t)o_wglGetProcAddress("glProgramUniform4f");
    if (!pu3 || !pu4) return;
    const TessLoc& l = it->second;
    if (l.cam >= 0) pu3(p, l.cam, g_pbrCam[0], g_pbrCam[1], g_pbrCam[2]);
    float pxScale = g_pbrHaveCam && g_tsP11 > 0 && g_screenH > 0 ? 0.5f * (float)g_screenH * g_tsP11 : 0.0f;
    if (l.tp >= 0) pu4(p, l.tp, pxScale, g_tsTargetPx, g_tsTargetEdge, (float)g_tsWorldMax);
    if (l.tr >= 0) pu4(p, l.tr, g_tsReliefFade0, g_tsReliefFade1, g_tsLive * g_tsReliefDepth * g_tsGroundRelief, g_tsLive * g_tsReliefDepth * g_tsPropRelief);
    if (l.tr2 >= 0) pu4(p, l.tr2, g_tsReliefLod, g_tsEdgeFade, 0.0f, 1.0f);
}
static void PbrApply(GLuint p) {
    TessApply(p);
    auto it = g_pbrLocs.find(p); if (it == g_pbrLocs.end()) return;
    unsigned& st = g_pbrStamp[p]; if (st == g_frame + 1) return; st = g_frame + 1;
    typedef void(WINAPI* pu3_t)(GLuint, GLint, GLfloat, GLfloat, GLfloat); typedef void(WINAPI* pu4_t)(GLuint, GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    static pu3_t pu3 = (pu3_t)o_wglGetProcAddress("glProgramUniform3f"); static pu4_t pu4 = (pu4_t)o_wglGetProcAddress("glProgramUniform4f");
    if (!pu3 || !pu4) return;
    if (it->second[0] >= 0) pu3(p, it->second[0], g_pbrCam[0], g_pbrCam[1], g_pbrCam[2]);
    if (it->second[1] >= 0) pu3(p, it->second[1], g_pbrSun[0], g_pbrSun[1], g_pbrSun[2]);
    if (it->second[2] >= 0) pu4(p, it->second[2], g_pbrNormal, g_pbrSpec, g_pbrRough, g_pbrHeight);
    if (it->second[4] >= 0) pu4(p, it->second[4], g_tsDetail * g_tsLive, 0.0f, 0.0f, 0.0f);
    if (it->second[5] >= 0) pu4(p, it->second[5], g_tsReliefDepthFS * g_tsLive, g_tsGroundRelief, g_tsPropRelief, (g_tessProgs.count(p) ? 1.0f : 0.0f));
    if (it->second[6] >= 0) pu4(p, it->second[6], g_tsReliefFade0, g_tsReliefFade1, (float)g_tsFsSteps, g_tsFsMipBias);
    if (it->second[3] >= 0) pu4(p, it->second[3], g_tsLive > 0.5f ? g_pbrLowBump : 1.0f, g_pbrCavity * g_tsLive, g_pbrValleyRough * g_tsLive, g_pbrSheen * g_tsLive);
}
// camera position from the scene VP (same derivation as RunAO) + sun direction; called when the scene depth is copied (end of the 3D pass)
static int g_shotPhase = 0;      // A/B hotkey state (abshot.inl): 0 idle, 1 first frame of a pair captured
static void TessABPoll() {
    if (!g_tsAB || g_shotPhase) return;   // the A/B hotkey (abshot.inl) owns g_tsLive while a pair is captured
    static std::string last; static unsigned lastFrame; if (g_frame - lastFrame < 8) return; lastFrame = g_frame;
    char pth[MAX_PATH]; snprintf(pth, sizeof pth, "%styrender\\cam.txt", g_dir); std::string cur; ReadFileStr(pth, cur);
    if (cur != last) { if (!last.empty() || g_tsLive < 0.5f) g_tsLive = g_tsLive > 0.5f ? 0.0f : 1.0f; last = cur; g_pbrStamp.clear(); g_tessStamp.clear(); logf("[tess] AB toggle: live=%.0f", g_tsLive); }
}
static void PbrUpdate() {
    TessABPoll();
    if ((!g_pbrEnable && !g_tsEnable) || !g_haveVP) return;
    float invVP[16]; if (!Invert4(g_sceneVP, invVP)) return;
    const float* m = g_sceneVP;
    float r1[3] = {m[1], m[5], m[9]}, r3[3] = {m[3], m[7], m[11]};
    float p11 = sqrtf(r1[0]*r1[0] + r1[1]*r1[1] + r1[2]*r1[2]), l3 = sqrtf(r3[0]*r3[0] + r3[1]*r3[1] + r3[2]*r3[2]);
    if (p11 < 1e-6f || l3 < 1e-6f) return;
    float B[3] = {-r3[0]/l3, -r3[1]/l3, -r3[2]/l3};
    float zn = g_aoZMode == 0 ? -1.0f : 0.0f, nc[4], nt[4];
    for (int i = 0; i < 4; i++) { nc[i] = invVP[8 + i] * zn + invVP[12 + i]; nt[i] = invVP[4 + i] + nc[i]; }
    float N3[3] = {nc[0]/nc[3], nc[1]/nc[3], nc[2]/nc[3]}, NT[3] = {nt[0]/nt[3], nt[1]/nt[3], nt[2]/nt[3]};
    float halfH = sqrtf((NT[0]-N3[0])*(NT[0]-N3[0]) + (NT[1]-N3[1])*(NT[1]-N3[1]) + (NT[2]-N3[2])*(NT[2]-N3[2])), nearDist = halfH * p11;
    g_tsP11 = p11;
    for (int i = 0; i < 3; i++) g_pbrCam[i] = N3[i] + B[i] * nearDist;
    if (g_haveShadowDir) { float l = sqrtf(g_shadowDir[0]*g_shadowDir[0] + g_shadowDir[1]*g_shadowDir[1] + g_shadowDir[2]*g_shadowDir[2]); if (l > 1e-4f) for (int i = 0; i < 3; i++) g_pbrSun[i] = -g_shadowDir[i] / l; }
    g_pbrHaveCam = true;
    if ((g_frame % 300) == 0) logf("[cam] pos=%.1f,%.1f,%.1f forward=%.3f,%.3f,%.3f", g_pbrCam[0], g_pbrCam[1], g_pbrCam[2], -B[0], -B[1], -B[2]);   // for -DebugModeCamera test views
}
