// fx.inl — final-image passes on FB0, run after bloomResolve and before the HUD.
// Trigger: first glViewport(64x64) of a scene frame = start of the engine's adaptive-brightness downsample chain (F3 anatomy step 11),
// at which point FB0 holds the finished 3D image but no HUD yet.
// Passes: [copy FB0 -> texScene] -> [FXAA 3.11-style -> texTmp] -> [sharpen (CAS-lite) + grade -> FB0]
// Config tyrender.ini [post]: Enable=1 FXAA=1 Sharpen=0.35 Contrast=1.04 Saturation=1.03 Vibrance=0.10 Shoulder=0.25 Vignette=0.18 Gamma=1.0
// Flag file: tyrender\post_off disables the whole module (A/B).

static const char* kFXAA = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform vec2 u_rcp; uniform int u_on;
float L(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
  vec3 cM = texture(u_src, uv).rgb;
  if (u_on == 0) { o = vec4(cM, 1); return; }
  float lM = L(cM);
  float lN = L(textureOffset(u_src, uv, ivec2(0, 1)).rgb), lS = L(textureOffset(u_src, uv, ivec2(0, -1)).rgb);
  float lE = L(textureOffset(u_src, uv, ivec2(1, 0)).rgb), lW = L(textureOffset(u_src, uv, ivec2(-1, 0)).rgb);
  float lMin = min(lM, min(min(lN, lS), min(lE, lW))), lMax = max(lM, max(max(lN, lS), max(lE, lW)));
  float range = lMax - lMin;
  if (range < max(0.0312, lMax * 0.125)) { o = vec4(cM, 1); return; }
  float lNW = L(textureOffset(u_src, uv, ivec2(-1, 1)).rgb), lNE = L(textureOffset(u_src, uv, ivec2(1, 1)).rgb);
  float lSW = L(textureOffset(u_src, uv, ivec2(-1, -1)).rgb), lSE = L(textureOffset(u_src, uv, ivec2(1, -1)).rgb);
  // subpixel blend amount
  float lAvg = (2.0 * (lN + lS + lE + lW) + lNW + lNE + lSW + lSE) / 12.0;
  float sub = clamp(abs(lAvg - lM) / range, 0.0, 1.0);
  sub = smoothstep(0.0, 1.0, sub); sub = sub * sub * 0.75;
  // edge orientation
  float eH = abs(lNW + lNE - 2.0 * lN) + 2.0 * abs(lW + lE - 2.0 * lM) + abs(lSW + lSE - 2.0 * lS);
  float eV = abs(lNW + lSW - 2.0 * lW) + 2.0 * abs(lN + lS - 2.0 * lM) + abs(lNE + lSE - 2.0 * lE);
  bool horz = eH >= eV;
  float l1 = horz ? lS : lW, l2 = horz ? lN : lE;
  float g1 = abs(l1 - lM), g2 = abs(l2 - lM);
  float step = horz ? u_rcp.y : u_rcp.x;
  float lLocal; float gScaled;
  if (g1 >= g2) { step = -step; lLocal = 0.5 * (l1 + lM); gScaled = 0.25 * g1; } else { lLocal = 0.5 * (l2 + lM); gScaled = 0.25 * g2; }
  vec2 cur = uv; if (horz) cur.y += step * 0.5; else cur.x += step * 0.5;
  vec2 off = horz ? vec2(u_rcp.x, 0) : vec2(0, u_rcp.y);
  vec2 p1 = cur - off, p2 = cur + off;
  float e1 = L(texture(u_src, p1).rgb) - lLocal, e2 = L(texture(u_src, p2).rgb) - lLocal;
  bool d1 = abs(e1) >= gScaled, d2 = abs(e2) >= gScaled;
  const float Q[10] = float[10](1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 4.0, 8.0);
  for (int i = 0; i < 10 && !(d1 && d2); i++) {
    if (!d1) { p1 -= off * Q[i]; e1 = L(texture(u_src, p1).rgb) - lLocal; d1 = abs(e1) >= gScaled; }
    if (!d2) { p2 += off * Q[i]; e2 = L(texture(u_src, p2).rgb) - lLocal; d2 = abs(e2) >= gScaled; }
  }
  float dist1 = horz ? uv.x - p1.x : uv.y - p1.y, dist2 = horz ? p2.x - uv.x : p2.y - uv.y;
  bool near1 = dist1 < dist2; float dmin = min(dist1, dist2), len = dist1 + dist2;
  bool lMSmaller = lM < lLocal;
  bool ok = ((near1 ? e1 : e2) < 0.0) != lMSmaller;
  float pixOff = ok ? -dmin / len + 0.5 : 0.0;
  float fin = max(pixOff, sub);
  vec2 fuv = uv; if (horz) fuv.y += fin * step; else fuv.x += fin * step;
  o = vec4(texture(u_src, fuv).rgb, 1);
}
)";

static const char* kGrade = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform vec2 u_rcp;
uniform float u_sharpen, u_contrast, u_saturation, u_vibrance, u_shoulder, u_vignette, u_gamma, u_brightness, u_knee, u_roll, u_split, u_halo;
void main() {
  vec3 c = texture(u_src, uv).rgb;
  // CAS-lite: adaptive unsharp mask, weaker where local contrast is already high
  if (u_sharpen > 0.0) {
    vec3 n = textureOffset(u_src, uv, ivec2(0, 1)).rgb, s = textureOffset(u_src, uv, ivec2(0, -1)).rgb;
    vec3 e = textureOffset(u_src, uv, ivec2(1, 0)).rgb, w = textureOffset(u_src, uv, ivec2(-1, 0)).rgb;
    vec3 mn = min(c, min(min(n, s), min(e, w))), mx = max(c, max(max(n, s), max(e, w)));
    vec3 amp = sqrt(clamp(min(mn, 1.0 - mx) / max(mx, 1e-4), 0.0, 1.0));
    vec3 wgt = -amp * mix(0.125, 0.2, u_sharpen);
    c = clamp((c + (n + s + e + w) * wgt) / (1.0 + 4.0 * wgt), mix(vec3(0.0), mn, u_halo), mix(vec3(1.0), mx, u_halo));   // halo-safe: never leaves the local min/max (no dark/bright rims)
  }
  // grade (LDR in, LDR out)
  c *= u_brightness;
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  float sat = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
  c = mix(vec3(l), c, u_saturation + u_vibrance * (1.0 - sat));         // vibrance boosts dull colours more
  c = (c - 0.5) * u_contrast + 0.5;                                     // contrast around mid grey
  c = clamp(c, 0.0, 1.0);
  c = mix(c, c * c * (3.0 - 2.0 * c), u_shoulder);                     // filmic S-curve: soft toe + shoulder, endpoints fixed
  if (u_roll > 0.0) {                                                    // highlight shoulder on the brightest channel (hue/saturation preserved) + gentle highlight desaturation
    float m = max(c.r, max(c.g, c.b));
    if (m > u_knee) { float x = (m - u_knee) / (1.0 - u_knee); float f = u_knee + (1.0 - u_knee) * (x - u_roll * x * x); c *= f / m; m = f; }
    float lh = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(lh), 0.25 * smoothstep(0.82, 1.0, m) * min(u_roll * 4.0, 1.0));
  }
  if (u_split > 0.0) {                                                   // split tone: cool shadows, warm highlights (small)
    float sl = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c *= mix(vec3(1.0), vec3(0.90, 0.98, 1.14), u_split * (1.0 - smoothstep(0.0, 0.55, sl)));
    c *= mix(vec3(1.0), vec3(1.07, 1.01, 0.91), u_split * smoothstep(0.45, 1.0, sl));
  }
  c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / u_gamma));
  vec2 d = uv - 0.5; c *= 1.0 - u_vignette * dot(d, d) * 2.0;
  o = vec4(c, 1);
}
)";

// TAA resolve: history reprojected with (unjittered) previous VP using the closest depth in 3x3, YCoCg neighbourhood clamp,
// current sample un-jittered by offsetting its UV. Ghosting on animated characters is bounded by the clamp (no per-object motion vectors).
static const char* kTAA = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_cur; uniform sampler2D u_hist; uniform sampler2D u_depth;
uniform mat4 u_invVP; uniform mat4 u_prevVP; uniform vec2 u_rcp; uniform vec2 u_jitterUV; uniform float u_blend; uniform int u_valid; uniform float u_reject;
vec3 YC(vec3 c) { return vec3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }
vec3 RGB(vec3 y) { return vec3(y.x + y.y - y.z, y.x + y.z, y.x - y.y - y.z); }
void main() {
  vec3 cur = texture(u_cur, uv + u_jitterUV).rgb;   // rendered(u) = true(u - j)  ->  true(u) = rendered(u + j)
  vec3 mn = vec3(1e9), mx = vec3(-1e9), m1 = vec3(0), m2 = vec3(0);
  float dMin = 1.0; vec2 dUV = uv;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    vec2 o2 = vec2(x, y) * u_rcp;
    vec3 s = YC(texture(u_cur, uv + o2).rgb);
    mn = min(mn, s); mx = max(mx, s); m1 += s; m2 += s * s;
    float d = texture(u_depth, uv + o2).r; if (d < dMin) { dMin = d; dUV = uv + o2; }
  }
  // variance clip box (tighter than min/max), intersected with min/max
  vec3 mu = m1 / 9.0, sig = sqrt(max(m2 / 9.0 - mu * mu, 0.0));
  mn = max(mn, mu - 1.0 * sig); mx = min(mx, mu + 1.0 * sig);
  if (u_valid == 0) { o = vec4(cur, 1); return; }
  vec4 w = u_invVP * vec4(dUV * 2.0 - 1.0, dMin * 2.0 - 1.0, 1.0); w /= w.w;
  vec4 pc = u_prevVP * vec4(w.xyz, 1.0);
  vec2 puv = uv + (pc.xy / pc.w * 0.5 + 0.5 - dUV);
  if (pc.w <= 0.0 || any(lessThan(puv, vec2(0.0))) || any(greaterThan(puv, vec2(1.0)))) { o = vec4(cur, 1); return; }
  vec3 hRaw = YC(texture(u_hist, puv).rgb);
  vec3 h = clamp(hRaw, mn, mx);
  float motion = length((puv - uv) / u_rcp);
  float blend = mix(u_blend, 0.5, clamp(motion / 12.0, 0.0, 1.0));   // trust history less under motion (ghosting)
  // history rejection (R5 grass ghosting): wind-animated grass and characters have no motion vectors, so their reprojected history is the
  // OLD geometry. When the raw history lies far outside the current neighbourhood box it is a disocclusion, not jitter noise -> blend less.
  float hd = abs(hRaw.x - h.x) / max(mx.x - mn.x, 0.02);
  blend = mix(blend, min(blend, 0.35), u_reject * smoothstep(0.08, 0.5, hd));
  o = vec4(mix(cur, RGB(h), blend), 1);
}
)";

static struct { bool init, failed; int w, h; GLuint progFXAA, progGrade, progTAA, texScene, texTmp, texHist[2], fbo; int histIdx; bool histValid; unsigned lastFrame; float prevVP[16]; } fx;
static int g_fxEnable = 1, g_fxFXAA = 1, g_fxAA = 2; static float g_taaBlend = 0.85f, g_taaReject = 1.0f;
static float g_fxSharpen = 0.25f, g_fxContrast = 1.0f, g_fxSat = 1.0f, g_fxVib = 0.04f, g_fxShoulder = 0.12f, g_fxVignette = 0.0f, g_fxGamma = 1.0f, g_fxBrightness = 1.0f, g_fxKnee = 0.72f, g_fxRoll = 0.15f, g_fxSplit = 0.12f; static int g_fxTonemap = 1; static float g_fxHalo = 1.0f;

static void LoadFXConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_fxEnable = GetPrivateProfileIntA("post", "Enable", 1, ini); g_fxFXAA = GetPrivateProfileIntA("post", "FXAA", 1, ini);
    g_fxAA = GetPrivateProfileIntA("post", "AA", 2, ini);          // 0 none, 1 FXAA, 2 TAA, 3 TAA + FXAA
    GetPrivateProfileStringA("post", "TAABlend", "0.85", b, sizeof b, ini); g_taaBlend = (float)atof(b);
    GetPrivateProfileStringA("post", "TAAReject", "1.0", b, sizeof b, ini); g_taaReject = (float)atof(b);   // 0 = old behaviour; 1 = history far outside the neighbourhood (moving grass) blends at most 0.35
#define F(key, def, var) GetPrivateProfileStringA("post", key, def, b, sizeof b, ini); var = (float)atof(b);
    F("Sharpen", "0.25", g_fxSharpen) F("SharpenHaloClamp", "1.0", g_fxHalo) F("Contrast", "1.0", g_fxContrast) F("Saturation", "1.0", g_fxSat) F("Vibrance", "0.04", g_fxVib)
    F("Shoulder", "0.12", g_fxShoulder) F("Vignette", "0.0", g_fxVignette) F("Gamma", "1.0", g_fxGamma) F("Brightness", "1.0", g_fxBrightness) F("TonemapKnee", "0.72", g_fxKnee) F("TonemapRoll", "0.15", g_fxRoll) F("SplitTone", "0.12", g_fxSplit)
    g_fxTonemap = GetPrivateProfileIntA("post", "Tonemap", 1, ini);
#undef F
}

// called once per presented frame: choose next frame's sub-pixel jitter (Halton 2,3; 8-sample cycle)
static float Halton(int i, int b) { float f = 1, r = 0; while (i > 0) { f /= b; r += f * (i % b); i /= b; } return r; }
static void UpdateJitter() {
    g_jitterOn = g_fxEnable && !g_fxOffFile && !fx.failed && (g_fxAA == 2 || g_fxAA == 3) && g_screenW > 0;
    if (!g_jitterOn) { g_jitterX = g_jitterY = 0; return; }
    int i = (g_frame % 8) + 1;
    g_jitterX = (Halton(i, 2) - 0.5f) * 2.0f / g_screenW;   // NDC units (one pixel = 2/W)
    g_jitterY = (Halton(i, 3) - 0.5f) * 2.0f / g_screenH;
}

static GLuint MakeTex8(int w, int h) {
    GLuint t; gx.GenTextures(1, &t); o_glBindTexture(0x0DE1, t);
    o_glTexImage2D(0x0DE1, 0, 0x8058 /*RGBA8*/, w, h, 0, 0x1908, 0x1401, nullptr);
    gx.TexParameteri(0x0DE1, 0x2801, 0x2601); gx.TexParameteri(0x0DE1, 0x2800, 0x2601);   // LINEAR (FXAA needs bilinear)
    gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);
    return t;
}

static void RunFX(int w, int h) {
    if (!g_fxEnable || g_fxOffFile || fx.failed) return;
    if (!LoadGX()) { fx.failed = true; return; }
    if (!fx.init) {
        fx.progFXAA = CompileProg("fxaa", kVS, kFXAA, nullptr, nullptr);
        fx.progGrade = CompileProg("grade", kVS, kGrade, nullptr, nullptr);
        fx.progTAA = CompileProg("taa", kVS, kTAA, nullptr, nullptr);
        if (!fx.progFXAA || !fx.progGrade || !fx.progTAA) { fx.failed = true; return; }
        gx.GenFramebuffers(1, &fx.fbo); fx.init = true;
        if (!ao.vao) gx.GenVertexArrays(1, &ao.vao);
    }
    typedef void(WINAPI* glCopyTexSubImage2D_t)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
    static glCopyTexSubImage2D_t copySub = (glCopyTexSubImage2D_t)GLProc("glCopyTexSubImage2D");
    if (fx.w != w || fx.h != h) {
        fx.texScene = MakeTex8(w, h); fx.texTmp = MakeTex8(w, h);
        for (int i = 0; i < 2; i++) { gx.GenTextures(1, &fx.texHist[i]); o_glBindTexture(0x0DE1, fx.texHist[i]);
            o_glTexImage2D(0x0DE1, 0, 0x881A /*RGBA16F*/, w, h, 0, 0x1908, 0x1406, nullptr);
            gx.TexParameteri(0x0DE1, 0x2801, 0x2601); gx.TexParameteri(0x0DE1, 0x2800, 0x2601); gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F); }
        fx.histValid = false; fx.w = w; fx.h = h; logf("[fx] initialised %dx%d", w, h);
    }

    GLint prevFBO, prevVP[4], prevUnit, prevTex0, prevVAO, prevBlendSrc, prevBlendDst; GLboolean prevDepthMask, prevColorMask[4];
    gx.GetIntegerv(0x8CA6, &prevFBO); gx.GetIntegerv(0x0BA2, prevVP); gx.GetIntegerv(0x84E0, &prevUnit);
    o_glActiveTexture(0x84C0); gx.GetIntegerv(0x8069, &prevTex0);
    gx.GetIntegerv(0x85B5, &prevVAO); gx.GetIntegerv(0x0BE1, &prevBlendSrc); gx.GetIntegerv(0x0BE0, &prevBlendDst);
    gx.GetBooleanv(0x0B72, &prevDepthMask); gx.GetBooleanv(0x0C23, prevColorMask);
    GLboolean eBlend = gx.IsEnabled(0x0BE2), eDepth = gx.IsEnabled(0x0B71), eCull = gx.IsEnabled(0x0B44), eScissor = gx.IsEnabled(0x0C11), eStencil = gx.IsEnabled(0x0B90);
    GLuint prevProg = g_curProg;
    o_glDisable(0x0BE2); o_glDisable(0x0B71); o_glDisable(0x0B44); o_glDisable(0x0C11); o_glDisable(0x0B90);
    o_glDepthMask(0); o_glColorMask(1, 1, 1, 1); gx.BindVertexArray(ao.vao);

    // copy finished FB0 (MSAA resolves implicitly)
    o_glBindFramebuffer(0x8D40, 0);
    o_glBindTexture(0x0DE1, fx.texScene); copySub(0x0DE1, 0, 0, 0, 0, 0, w, h);
    GLuint gradeSrc = fx.texScene;
    o_glBindFramebuffer(0x8D40, fx.fbo); o_glViewport(0, 0, w, h);
    // TAA -> history[cur]
    bool taa = (g_fxAA == 2 || g_fxAA == 3) && g_sceneDepthTex && g_vpFrame == g_frame;
    if (taa) {
        float invVP[16]; Invert4(g_sceneVP, invVP);
        int cur = fx.histIdx ^ 1;
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, fx.texHist[cur], 0);
        GLuint p = fx.progTAA; o_glUseProgram(p);
        gx.Uniform1i(o_glGetUniformLocation(p, "u_cur"), 0); gx.Uniform1i(o_glGetUniformLocation(p, "u_hist"), 1); gx.Uniform1i(o_glGetUniformLocation(p, "u_depth"), 2);
        o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_invVP"), 1, 0, invVP); o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_prevVP"), 1, 0, fx.prevVP);
        gx.Uniform2f(o_glGetUniformLocation(p, "u_rcp"), 1.0f / w, 1.0f / h);
        gx.Uniform2f(o_glGetUniformLocation(p, "u_jitterUV"), g_jitterOn ? g_jitterX * 0.5f : 0.0f, g_jitterOn ? g_jitterY * 0.5f : 0.0f);
        gx.Uniform1f(o_glGetUniformLocation(p, "u_blend"), g_taaBlend); gx.Uniform1f(o_glGetUniformLocation(p, "u_reject"), g_taaReject);
        gx.Uniform1i(o_glGetUniformLocation(p, "u_valid"), (fx.histValid && fx.lastFrame + 1 == g_frame) ? 1 : 0);
        o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, fx.texHist[fx.histIdx]);
        o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, g_sceneDepthTex);
        o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, fx.texScene);
        o_glDrawArrays(4, 0, 3);
        o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, 0); o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, 0); o_glActiveTexture(0x84C0);
        fx.histIdx = cur; fx.histValid = true; fx.lastFrame = g_frame; memcpy(fx.prevVP, g_sceneVP, sizeof fx.prevVP);
        gradeSrc = fx.texHist[cur];
    }
    // FXAA -> texTmp
    if (g_fxAA == 1 || g_fxAA == 3 || (g_fxAA != 0 && !taa && g_fxFXAA)) {
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, fx.texTmp, 0);
        o_glUseProgram(fx.progFXAA);
        gx.Uniform1i(o_glGetUniformLocation(fx.progFXAA, "u_src"), 0); gx.Uniform2f(o_glGetUniformLocation(fx.progFXAA, "u_rcp"), 1.0f / w, 1.0f / h);
        gx.Uniform1i(o_glGetUniformLocation(fx.progFXAA, "u_on"), 1);
        o_glBindTexture(0x0DE1, gradeSrc);
        o_glDrawArrays(4, 0, 3);
        gradeSrc = fx.texTmp;
    }
    // sharpen + grade -> FB0
    o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
    o_glUseProgram(fx.progGrade);
    auto U = [](const char* n, float v) { gx.Uniform1f(o_glGetUniformLocation(fx.progGrade, n), v); };
    gx.Uniform1i(o_glGetUniformLocation(fx.progGrade, "u_src"), 0); gx.Uniform2f(o_glGetUniformLocation(fx.progGrade, "u_rcp"), 1.0f / w, 1.0f / h);
    U("u_sharpen", g_fxSharpen); U("u_halo", g_fxHalo); U("u_contrast", g_fxContrast); U("u_saturation", g_fxSat); U("u_vibrance", g_fxVib);
    U("u_shoulder", g_fxShoulder); U("u_vignette", g_fxVignette); U("u_gamma", g_fxGamma); U("u_brightness", g_fxBrightness);
    U("u_knee", g_fxKnee); U("u_roll", g_fxTonemap ? g_fxRoll : 0.0f); U("u_split", g_fxTonemap ? g_fxSplit : 0.0f);
    o_glBindTexture(0x0DE1, gradeSrc);
    o_glDrawArrays(4, 0, 3);

    gx.BindVertexArray(prevVAO); o_glBindTexture(0x0DE1, prevTex0); o_glActiveTexture(prevUnit);
    o_glBindFramebuffer(0x8D40, prevFBO); o_glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]); o_glUseProgram(prevProg);
    o_glBlendFunc(prevBlendSrc, prevBlendDst);
    if (eBlend) o_glEnable(0x0BE2); if (eDepth) o_glEnable(0x0B71); if (eCull) o_glEnable(0x0B44); if (eScissor) o_glEnable(0x0C11); if (eStencil) o_glEnable(0x0B90);
    o_glDepthMask(prevDepthMask); o_glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
    static bool once; if (!once) { once = true; logf("[fx] first run ok at frame %u", g_frame); }
}
