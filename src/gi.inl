// gi.inl — screen-space global illumination (one diffuse bounce, colour bleeding), injected after AO in the depth-copy hook.
// Half res: per pixel N cosine-distributed rays around the reconstructed normal are marched in view space and projected to screen;
// on a hit the lit scene colour (engine capture texture) is gathered. Temporal accumulation + bilateral upsample, then added to FB0
// as  gi * albedoProxy * Strength  (albedo proxy = hue of the pixel, so shadowed areas receive bounce light too).
// Config tyrender.ini [gi]: Enable=1 Strength=1.0 Radius=400 Thickness=120 Rays=4 Steps=8 TemporalBlend=0.9
// Flag files: tyrender\gi_off, tyrender\gi_debug (1 = GI only)

static int g_giEnable = 1, g_giRays = 4, g_giSteps = 8; static bool g_giOffFile;
static float g_giStrength = 0.7f, g_giRadius = 400.0f, g_giThick = 120.0f, g_giBlend = 0.75f;
#include "gi2.inl"

static const char* kGI = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_color; uniform float u_p00; uniform float u_p11; uniform float u_radius; uniform float u_thick; uniform int u_rays; uniform int u_steps; uniform float u_frameNoise;
float IGN(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
vec2 Proj(vec3 v) { return vec2(u_p00 * v.x, u_p11 * v.y) / max(-v.z, 1e-3) * 0.5 + 0.5; }
void main() {
  float d = texture(u_depth, uv).r;
  if (d >= 0.99999) { o = vec4(0, 0, 0, -1e6); return; }
  vec3 P = ViewPos(uv), N = ViewNormal(uv, P);
  vec3 T = normalize(abs(N.y) < 0.99 ? cross(N, vec3(0, 1, 0)) : cross(N, vec3(1, 0, 0))), B = cross(N, T);
  float n1 = fract(IGN(gl_FragCoord.xy) + u_frameNoise), n2 = fract(IGN(gl_FragCoord.xy + 17.0) + u_frameNoise * 1.618);
  vec3 sum = vec3(0.0);
  for (int r = 0; r < u_rays; r++) {
    float u1 = fract(n1 + float(r) / float(u_rays)), u2 = fract(n2 + float(r) * 0.618034);
    float phi = 6.2831853 * u2, cr = sqrt(u1);                                  // cosine-weighted hemisphere
    vec3 dir = normalize(T * (cos(phi) * cr) + B * (sin(phi) * cr) + N * sqrt(max(1.0 - u1, 0.0)));
    for (int k = 1; k <= u_steps; k++) {
      float t = u_radius * pow((float(k) - 0.5 + n2 * 0.5) / float(u_steps), 2.0);
      vec3 Q = P + N * 2.0 + dir * t;
      vec2 q = Proj(Q);
      if (any(lessThan(q, vec2(0.0))) || any(greaterThan(q, vec2(1.0)))) break;
      float sz = ViewPos(q).z;
      float dz = sz - Q.z;                     // > 0: scene surface is in front of the ray point -> ray went behind it
      if (dz > 0.0 && dz < u_thick) { sum += texture(u_color, q).rgb; break; }
    }
  }
  o = vec4(sum / float(u_rays), P.z);
}
)";

static const char* kGITemporal = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_cur; uniform sampler2D u_hist; uniform mat4 u_prevVP; uniform vec2 u_halfSize; uniform float u_blend; uniform int u_histValid;
void main() {
  vec4 c = texture(u_cur, uv);
  if (c.a < -1e5 || u_histValid == 0) { o = c; return; }
  vec3 W = WorldPos(uv);
  vec4 pc = u_prevVP * vec4(W, 1.0); vec2 puv = pc.xy / pc.w * 0.5 + 0.5;
  if (pc.w <= 0.0 || any(lessThan(puv, vec2(0.0))) || any(greaterThan(puv, vec2(1.0)))) { o = c; return; }
  vec4 h = texture(u_hist, puv);
  if (abs(h.a - c.a) > 0.05 * abs(c.a) + 5.0) { o = c; return; }             // disocclusion (view depth changed a lot)
  vec3 mn = c.rgb, mx = c.rgb;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) { vec3 s = texture(u_cur, uv + vec2(x, y) / u_halfSize).rgb; mn = min(mn, s); mx = max(mx, s); }
  vec3 hc = clamp(h.rgb, mn - 0.02, mx + 0.02);
  o = vec4(mix(c.rgb, hc, u_blend), c.a);
}
)";

static const char* kGIComposite = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_gi; uniform sampler2D u_color; uniform vec2 u_halfSize; uniform float u_strength; uniform int u_debug;
void main() {
  float d = texture(u_depth, uv).r;
  if (d >= 0.99999) { o = vec4(0); return; }
  float z = ViewPos(uv).z;
  vec2 hp = uv * u_halfSize - 0.5, f = fract(hp), base = (floor(hp) + 0.5) / u_halfSize;
  vec3 sum = vec3(0.0); float wsum = 0.0;
  for (int dy = -1; dy <= 2; dy++) for (int dx = -1; dx <= 2; dx++) {            // 4x4 bilateral: smooth, edge-aware
    vec4 s = texture(u_gi, base + vec2(dx, dy) / u_halfSize);
    float dist2 = dot(vec2(dx, dy) - f, vec2(dx, dy) - f);
    float dz = abs(s.a - z) / max(abs(z), 0.1);
    float w = exp(-dist2 * 0.6) * exp(-dz * dz * 300.0) + 1e-5;
    sum += s.rgb * w; wsum += w;
  }
  vec3 gi = sum / wsum;
  vec3 c = texture(u_color, uv).rgb;
  float l = max(dot(c, vec3(0.2126, 0.7152, 0.0722)), 0.03);
  vec3 albedo = clamp(mix(vec3(l), c, 0.6) / l * 0.35, 0.0, 1.0);   // desaturated hue at modest albedo
  // indirect light matters in shade; on brightly lit surfaces (sunny grass) it must not add visible glow
  float shade = pow(clamp(1.0 - l * 1.4, 0.0, 1.0), 2.0);
  if (u_debug == 1) { o = vec4(gi, 1); return; }
  o = vec4(min(gi * albedo * u_strength * shade, vec3(0.25)), 1.0);
}
)";

static struct { bool init, failed; int w, h; GLuint prog, progT, progC, texRaw, texH[2]; int histIdx; bool histValid; unsigned lastFrame; float prevVP[16]; } gi;
static int g_giDebugIni;
static void LoadGIConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_giEnable = GetPrivateProfileIntA("gi", "Enable", 1, ini); g_giRays = GetPrivateProfileIntA("gi", "Rays", 4, ini); g_giSteps = GetPrivateProfileIntA("gi", "Steps", 8, ini);
#define F(key, def, var) GetPrivateProfileStringA("gi", key, def, b, sizeof b, ini); var = (float)atof(b);
    F("Strength", "0.7", g_giStrength) F("Radius", "400", g_giRadius) F("Thickness", "120", g_giThick) F("TemporalBlend", "0.85", g_giBlend) F("SigmaL", "4", g_giSigmaL) F("SourceBlur", "0.15", g_giBlur) g_giNoiseCycle = GetPrivateProfileIntA("gi", "NoiseCycle", 8, ini); g_giMask = GetPrivateProfileIntA("gi", "ExcludeSources", 7, ini);
    g_giCharOn = GetPrivateProfileIntA("gi", "Characters", 1, ini); F("CharacterColour", "0.0", g_giCharKeep) F("CharacterGI", "0.3", g_giCharGI) F("CharacterHeight", "160", g_giCharH) F("CharacterRadius", "70", g_giCharR)
    g_giChecker = GetPrivateProfileIntA("gi", "Checker", 0, ini); g_giMode = GetPrivateProfileIntA("gi", "Mode", 1, ini); g_giAtrous = GetPrivateProfileIntA("gi", "Atrous", 3, ini); g_giHizIter = GetPrivateProfileIntA("gi", "HiZIter", 0, ini); g_giProf = GetPrivateProfileIntA("gi", "Prof", 0, ini); g_giSweep = GetPrivateProfileIntA("gi", "Sweep", 0, ini); g_giDebugIni = GetPrivateProfileIntA("gi", "DebugView", 0, ini);
#undef F
}
static void PollGIFlags() {
    char p[MAX_PATH]; std::string s;
    snprintf(p, sizeof p, "%styrender\\gi_off", g_dir); g_giOffFile = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    snprintf(p, sizeof p, "%styrender\\gi_debug", g_dir); g_giDebug = ReadFileStr(p, s) ? atoi(s.c_str()) : g_giDebugIni;
}

static void RunGI(GLuint depthTex, int w, int h) {
    if (!g_giEnable || g_giOffFile || g_abOff || (gi.failed && g2.failed) || !g_haveVP || g_vpFrame != g_frame || !g_colorCopyTex || g_giStrength <= 0) return;
    if (!LoadGX()) { gi.failed = true; return; }
    if (g_giSweep) GISweepApply();
    float invVP[16]; if (!Invert4(g_sceneVP, invVP)) return;
    const float* m = g_sceneVP;
    float r0[3] = {m[0], m[4], m[8]}, r1[3] = {m[1], m[5], m[9]}, r3[3] = {m[3], m[7], m[11]};
    float p00 = sqrtf(r0[0]*r0[0] + r0[1]*r0[1] + r0[2]*r0[2]), p11 = sqrtf(r1[0]*r1[0] + r1[1]*r1[1] + r1[2]*r1[2]), l3 = sqrtf(r3[0]*r3[0] + r3[1]*r3[1] + r3[2]*r3[2]);
    float R[3] = {r0[0]/p00, r0[1]/p00, r0[2]/p00}, U[3] = {r1[0]/p11, r1[1]/p11, r1[2]/p11}, B[3] = {-r3[0]/l3, -r3[1]/l3, -r3[2]/l3};
    float nc[4], nt[4]; for (int i = 0; i < 4; i++) { nc[i] = -invVP[8 + i] + invVP[12 + i]; nt[i] = invVP[4 + i] + nc[i]; }
    float N3[3] = {nc[0]/nc[3], nc[1]/nc[3], nc[2]/nc[3]}, NT[3] = {nt[0]/nt[3], nt[1]/nt[3], nt[2]/nt[3]};
    float nearDist = sqrtf((NT[0]-N3[0])*(NT[0]-N3[0]) + (NT[1]-N3[1])*(NT[1]-N3[1]) + (NT[2]-N3[2])*(NT[2]-N3[2])) * p11;
    float cam[3] = {N3[0] + B[0]*nearDist, N3[1] + B[1]*nearDist, N3[2] + B[2]*nearDist};
    float view[16] = { R[0], U[0], B[0], 0,  R[1], U[1], B[1], 0,  R[2], U[2], B[2], 0,
                       -(R[0]*cam[0] + R[1]*cam[1] + R[2]*cam[2]), -(U[0]*cam[0] + U[1]*cam[1] + U[2]*cam[2]), -(B[0]*cam[0] + B[1]*cam[1] + B[2]*cam[2]), 1 };

    GLint prevFBO, prevVP[4], prevUnit, prevTex[5], prevVAO, prevBlendSrc, prevBlendDst; GLboolean prevDepthMask, prevColorMask[4];
    gx.GetIntegerv(0x8CA6, &prevFBO); gx.GetIntegerv(0x0BA2, prevVP); gx.GetIntegerv(0x84E0, &prevUnit);
    for (int i = 0; i < 5; i++) { o_glActiveTexture(0x84C0 + i); gx.GetIntegerv(0x8069, &prevTex[i]); }
    gx.GetIntegerv(0x85B5, &prevVAO); gx.GetIntegerv(0x0BE1, &prevBlendSrc); gx.GetIntegerv(0x0BE0, &prevBlendDst);
    gx.GetBooleanv(0x0B72, &prevDepthMask); gx.GetBooleanv(0x0C23, prevColorMask);
    GLboolean eBlend = gx.IsEnabled(0x0BE2), eDepth = gx.IsEnabled(0x0B71), eCull = gx.IsEnabled(0x0B44), eScissor = gx.IsEnabled(0x0C11), eStencil = gx.IsEnabled(0x0B90);
    GLuint prevProg = g_curProg;
    o_glDisable(0x0BE2); o_glDisable(0x0B71); o_glDisable(0x0B44); o_glDisable(0x0C11); o_glDisable(0x0B90);
    o_glDepthMask(0); o_glColorMask(1, 1, 1, 1); gx.BindVertexArray(ao.vao);
    bool done = GIHiz(depthTex, w, h, invVP, view, p00, p11);
    if (!done && g_giMode != 0 && g2.failed) { gi.failed = true; static int once; if (!once++) logf("[fallback] Hi-Z GI unavailable -> GI OFF (no legacy fallback)"); }   // fallback policy: failure -> OFF
    if (!done) {
    if (!gi.init && !gi.failed) {
        gi.prog = CompileProg("ssgi", kVS, "#version 330\n", kCommon, kGI);
        gi.progT = CompileProg("ssgi_temporal", kVS, "#version 330\n", kCommon, kGITemporal);
        gi.progC = CompileProg("ssgi_composite", kVS, "#version 330\n", kCommon, kGIComposite);
        if (!gi.prog || !gi.progT || !gi.progC) gi.failed = true;
        else { if (!ao.vao) gx.GenVertexArrays(1, &ao.vao); if (!ao.fbo) gx.GenFramebuffers(1, &ao.fbo); gi.init = true; }
    }
    if (!gi.failed) {
    GIMarkBegin(); GIMark(1);
    int hw = w / 2, hh = h / 2;
    if (gi.w != w || gi.h != h) {
        gi.texRaw = MakeTex(hw, hh, 0x881A, 0x1908);
        for (int i = 0; i < 2; i++) gi.texH[i] = MakeTex(hw, hh, 0x881A, 0x1908);
        for (GLuint t : {gi.texH[0], gi.texH[1]}) { o_glBindTexture(0x0DE1, t); gx.TexParameteri(0x0DE1, 0x2801, 0x2601); gx.TexParameteri(0x0DE1, 0x2800, 0x2601); }
        gi.w = w; gi.h = h; gi.histValid = false; logf("[gi] initialised %dx%d", hw, hh);
    }
    auto Common = [&](GLuint p) {
          gx.Uniform1i(o_glGetUniformLocation(p, "u_depth"), 0); o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_invVP"), 1, 0, invVP);
          o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_view"), 1, 0, view); gx.Uniform2f(o_glGetUniformLocation(p, "u_fullSize"), (float)w, (float)h);
          gx.Uniform1i(o_glGetUniformLocation(p, "u_zMode"), g_aoZMode);
      };
      o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
      o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, g_colorCopyTex);
      // 1) trace -> texRaw
      o_glBindFramebuffer(0x8D40, ao.fbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, gi.texRaw, 0); o_glViewport(0, 0, hw, hh);
      o_glUseProgram(gi.prog); Common(gi.prog);
      gx.Uniform1i(o_glGetUniformLocation(gi.prog, "u_color"), 1);
      gx.Uniform1f(o_glGetUniformLocation(gi.prog, "u_p00"), p00); gx.Uniform1f(o_glGetUniformLocation(gi.prog, "u_p11"), p11);
      gx.Uniform1f(o_glGetUniformLocation(gi.prog, "u_radius"), g_giRadius); gx.Uniform1f(o_glGetUniformLocation(gi.prog, "u_thick"), g_giThick);
      gx.Uniform1i(o_glGetUniformLocation(gi.prog, "u_rays"), g_giRays); gx.Uniform1i(o_glGetUniformLocation(gi.prog, "u_steps"), g_giSteps);
      gx.Uniform1f(o_glGetUniformLocation(gi.prog, "u_frameNoise"), fmodf(g_frame * 0.6180339f, 1.0f));
      o_glDrawArrays(4, 0, 3); GIMark(2);
      // 2) temporal -> texH[cur]
      int cur = gi.histIdx ^ 1;
      o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, gi.texH[cur], 0);
      o_glUseProgram(gi.progT); Common(gi.progT);
      gx.Uniform1i(o_glGetUniformLocation(gi.progT, "u_cur"), 1); gx.Uniform1i(o_glGetUniformLocation(gi.progT, "u_hist"), 2);
      gx.Uniform2f(o_glGetUniformLocation(gi.progT, "u_halfSize"), (float)hw, (float)hh); gx.Uniform1f(o_glGetUniformLocation(gi.progT, "u_blend"), g_giBlend);
      gx.Uniform1i(o_glGetUniformLocation(gi.progT, "u_histValid"), (gi.histValid && gi.lastFrame + 1 == g_frame) ? 1 : 0);
      o_glUniformMatrix4fv(o_glGetUniformLocation(gi.progT, "u_prevVP"), 1, 0, gi.prevVP);
      o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, gi.texRaw);
      o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, gi.texH[gi.histIdx]);
      o_glDrawArrays(4, 0, 3);
      gi.histIdx = cur; gi.histValid = true; gi.lastFrame = g_frame; memcpy(gi.prevVP, g_sceneVP, sizeof gi.prevVP);
      GIMark(3); GIMark(4);
      // 3) composite (additive) onto FB0
      o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
      o_glUseProgram(gi.progC); Common(gi.progC);
      gx.Uniform1i(o_glGetUniformLocation(gi.progC, "u_gi"), 2); gx.Uniform1i(o_glGetUniformLocation(gi.progC, "u_color"), 1);
      gx.Uniform2f(o_glGetUniformLocation(gi.progC, "u_halfSize"), (float)hw, (float)hh); gx.Uniform1f(o_glGetUniformLocation(gi.progC, "u_strength"), g_giStrength);
      gx.Uniform1i(o_glGetUniformLocation(gi.progC, "u_debug"), g_giDebug);
      o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, g_colorCopyTex);
      o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, gi.texH[cur]);
      if (!g_giDebug) { o_glEnable(0x0BE2); o_glBlendFunc(1, 1); }
      o_glDrawArrays(4, 0, 3);
      o_glDisable(0x0BE2); GIMark(5);
    }
    }
    g_sceneModified = true;

    gx.BindVertexArray(prevVAO);
    for (int i = 0; i < 5; i++) { o_glActiveTexture(0x84C0 + i); o_glBindTexture(0x0DE1, prevTex[i]); }
    o_glActiveTexture(prevUnit);
    o_glBindFramebuffer(0x8D40, prevFBO); o_glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]); o_glUseProgram(prevProg);
    o_glBlendFunc(prevBlendSrc, prevBlendDst);
    if (eBlend) o_glEnable(0x0BE2); if (eDepth) o_glEnable(0x0B71); if (eCull) o_glEnable(0x0B44); if (eScissor) o_glEnable(0x0C11); if (eStencil) o_glEnable(0x0B90);
    o_glDepthMask(prevDepthMask); o_glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
    static bool once; if (!once) { once = true; logf("[gi] first run ok"); }
}
