// atmos.inl — volumetric atmosphere injected after AO (same hook: engine depth copy, before bloom so shafts bloom naturally).
// Per pixel: march from the camera to the scene depth (sky: MaxDist), sample the engine's cascade shadow maps (shadowTexture1/2 with
// shadowVPMatrix1/2 captured from the main pass) for sun visibility, integrate height-fog in-scattering with a Henyey-Greenstein phase,
// then  colour = colour * transmittance + inscatter.  Engine fog stays as is (this adds god rays + aerial haze).
// Config tyrender.ini [atmos]: Enable=1 Density=0.00006 HeightFalloff=0.0015 Steps=24 MaxDist=40000 SunIntensity=1.4 Anisotropy=0.6
//   SunColor=1.0,0.86,0.66  AmbientScale=0.35   Flag file: tyrender\atmos_off ; tyrender\atmos_debug (1 = inscatter only)
// World units are ~cm (camera y ~2744, near plane 15) -> densities are tiny per unit.

// captured engine uniforms (set by hooks in tyrender.cpp while FB0 is bound)
static float g_shadowVP[2][16]; static bool g_haveShadowVP[2];
static float g_shadowDir[4], g_fogColour[4]; static bool g_haveShadowDir, g_haveFog;


static const char* kAtmos = R"(
in vec2 uv; out vec4 o;
uniform sampler2DShadow u_sh1; uniform sampler2DShadow u_sh2; uniform mat4 u_shVP1; uniform mat4 u_shVP2; uniform int u_haveSh;
uniform sampler2D u_color;
uniform vec3 u_cam; uniform vec3 u_sunDir; uniform vec3 u_sunCol; uniform vec3 u_ambCol;
uniform float u_density; uniform float u_falloff; uniform int u_steps; uniform float u_maxDist; uniform float u_g; uniform float u_frameNoise; uniform int u_debug;
uniform int u_mode; uniform sampler2D u_low; uniform vec2 u_halfSize;   // u_mode: 0 full-res, 1 half-res march (scat,T), 2 full-res composite
uniform sampler3D u_vol; uniform float u_volA; uniform float u_volN; uniform float u_desat; uniform float u_lift; uniform vec3 u_haze; uniform float u_skyHaze; uniform float u_skyH;
uniform sampler2D u_skyTex; uniform float u_hazeSky, u_ambScale; uniform float u_blurStart, u_blurRange, u_blurPx; uniform vec2 u_rcp;
const vec3 LW = vec3(0.2126, 0.7152, 0.0722);
// haze colour: the CPU level-fog hue, pulled toward the colour of the actual sky near the horizon (measured on the GPU, smoothed over time)
vec3 HazeCol(vec3 cpu) {
  vec4 s = texelFetch(u_skyTex, ivec2(0), 0);
  if (s.a < 0.5 || u_hazeSky <= 0.0) return cpu;
  return mix(cpu, min(s.rgb * 0.75 / max(dot(s.rgb, LW), 1e-3), vec3(1.5)) * u_ambScale, u_hazeSky);
}
uniform mat4 u_VP; uniform float u_contact; uniform float u_contactLen; uniform float u_contactThick; uniform float u_shaftContrast;
float IGN(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
float SunVis(vec3 X) {
  if (u_haveSh == 0) return 1.0;
  vec3 c = (u_shVP1 * vec4(X, 1.0)).xyz;
  if (all(greaterThan(c, vec3(0.002))) && all(lessThan(c, vec3(0.998)))) return 1.0 - textureLod(u_sh1, c, 0.0);   // engine: lookup 1 = shadowed; explicit LOD: called in divergent branches (R5)
  c = (u_shVP2 * vec4(X, 1.0)).xyz;
  if (all(greaterThan(c, vec3(0.002))) && all(lessThan(c, vec3(0.998)))) return 1.0 - textureLod(u_sh2, c, 0.0);
  return 1.0;
}
void main() {
  vec3 col = texture(u_color, uv).rgb;
  float d = texture(u_depth, uv).r;
  vec3 P = WorldPos(uv);
  // contact shadows: short screen-space march toward the sun; only darkens pixels the shadow map considers lit
  if (u_mode != 1 && u_contact > 0.0 && d < 0.99999) {
    float vis0 = SunVis(P + u_sunDir * 3.0);
    if (vis0 > 0.5) {
      float occ = 0.0, jit = fract(IGN(gl_FragCoord.xy + 31.0) + u_frameNoise);
      float camD = distance(P, u_cam);
      for (int i = 1; i <= 12; i++) {
        vec3 Q = P + u_sunDir * (u_contactLen * (float(i) - jit) / 12.0);
        vec4 c = u_VP * vec4(Q, 1.0); vec2 q = c.xy / c.w * 0.5 + 0.5;
        if (c.w <= 0.0 || any(lessThan(q, vec2(0.0))) || any(greaterThan(q, vec2(1.0)))) break;
        float sceneD = distance(WorldPos(q), u_cam), rayD = distance(Q, u_cam);
        float dz = rayD - sceneD;                               // > 0: something is in front of the ray point
        // only a THIN depth window counts as an occluder (otherwise everything in front of the ray -> long streaks behind grass/trees)
        if (dz > 0.5 && dz < u_contactThick) { occ = (1.0 - float(i) / 13.0); break; }
      }
      col *= 1.0 - u_contact * occ * (1.0 - smoothstep(2000.0, 6000.0, camD));
    }
  }
  if (u_mode == 3) {              // froxel volume: one trilinear lookup by (screen uv, radial distance)
    float dd = length(P - u_cam);
    if (u_blurPx > 0.0 && d < 0.99999) {   // far-field softening (cheap depth-of-field look): 8 golden taps, never pulls in nearer geometry
      float b = smoothstep(u_blurStart, u_blurStart + u_blurRange, dd) * u_blurPx;
      if (b > 0.4) {
        float rot = IGN(gl_FragCoord.xy) * 6.2831853;
        vec3 acc = col; float ws = 1.0;
        for (int i = 0; i < 8; i++) {
          float a = float(i) * 2.3999632 + rot, r = sqrt((float(i) + 0.5) / 8.0) * b;
          vec2 tuv = uv + vec2(cos(a), sin(a)) * r * u_rcp;
          float td = texture(u_depth, tuv).r;
          float w = ((1.0 - td) <= (1.0 - d) * 1.15) ? 1.0 : 0.0;
          acc += texture(u_color, tuv).rgb * w; ws += w;
        }
        col = acc / ws;
      }
    }
    bool skyp = d >= 0.99999;
    if (skyp) dd = 0.12 * u_maxDist;
    dd = min(dd, u_maxDist);
    float s = log(1.0 + dd * (exp(u_volA) - 1.0) / u_maxDist) / u_volA;
    vec4 v = texture(u_vol, vec3(uv, max(s - 0.5 / u_volN, 0.5 / u_volN)));
    if (s < 1.0 / u_volN) v = mix(vec4(0.0, 0.0, 0.0, 1.0), v, s * u_volN);
    if (u_debug == 1) { o = vec4(v.rgb, 1); return; }
    if (u_debug == 2) { o = vec4(vec3(SunVis(P + vec3(0.0, 2.0, 0.0))), 1); return; }
    float omT = 1.0 - v.a;
    if (skyp) {
      // painted sky (mountains/mesas): same aerial perspective as the world, strongest at the horizon, fading with view elevation
      float el = normalize(P - u_cam).y;
      float hb = u_skyHaze * (1.0 - smoothstep(0.0, max(u_skyH, 1e-3), el));
      float sl = dot(col, vec3(0.2126, 0.7152, 0.0722));
      col = mix(col, vec3(sl), clamp(0.55 * u_desat * hb, 0.0, 1.0));
      col = mix(col, HazeCol(u_haze), clamp(u_lift * hb, 0.0, 1.0));
      o = vec4(col * mix(1.0, v.a, u_skyHaze) + v.rgb * u_skyHaze, 1.0); return;
    }
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(col, vec3(lum), clamp(u_desat * omT, 0.0, 1.0));      // aerial perspective: distant colours lose saturation ...
    col = mix(col, HazeCol(u_haze), clamp(u_lift * omT, 0.0, 1.0));          // ... and lift towards the haze colour
    o = vec4(col * v.a + v.rgb, 1.0); return;
  }
  if (u_mode == 2) {              // depth-aware 2x2 upsample of the half-res scattering (depth compared at the low-res texel centres)
    float zc = ViewPos(uv).z;
    vec2 hp = uv * u_halfSize - 0.5, f = fract(hp), base = floor(hp);
    vec4 acc = vec4(0.0); float wsum = 0.0;
    for (int dy = 0; dy <= 1; dy++) for (int dx = 0; dx <= 1; dx++) {
      vec2 tuv = (base + vec2(dx, dy) + 0.5) / u_halfSize;
      float zs = ViewPos(tuv).z;
      float wb = (dx == 1 ? f.x : 1.0 - f.x) * (dy == 1 ? f.y : 1.0 - f.y);
      float dzr = abs(zs - zc) / max(abs(zc), 0.1);
      float w = wb * exp(-dzr * dzr * 400.0) + 1e-5;
      acc += texture(u_low, tuv) * w; wsum += w;
    }
    acc /= wsum;
    o = vec4(col * acc.a + acc.rgb, 1.0); return;
  }
  vec3 ray = P - u_cam; float dist = length(ray); vec3 V = ray / max(dist, 1e-3);
  bool sky = d >= 0.99999;
  if (sky || dist > u_maxDist) dist = u_maxDist;
  if (sky) dist = min(dist, 0.12 * u_maxDist);       // keep the painted sky: only a short slab of haze/shaft glow in front of it
  float cosT = dot(V, u_sunDir);
  float g = u_g, phase = (1.0 - g * g) / (12.566 * pow(max(1.0 + g * g - 2.0 * g * cosT, 1e-4), 1.5));
  float iso = 1.0 / 12.566;
  float stepLen = dist / float(u_steps);
  float jitter = fract(IGN(gl_FragCoord.xy) + u_frameNoise);
  vec3 scat = vec3(0.0); float T = 1.0;
  for (int i = 0; i < u_steps; i++) {
    float t = (float(i) + jitter) * stepLen;
    vec3 X = u_cam + V * t;
    float dens = u_density * exp(-max(X.y - u_cam.y, -2000.0) * u_falloff);
    float sliceT = exp(-dens * stepLen);
    float vis = mix(1.0, SunVis(X), u_shaftContrast);          // < 1: softer shafts, no hard lines through the fog
    vec3 Lin = u_sunCol * (vis * phase) + u_ambCol * iso;
    scat += T * Lin * (1.0 - sliceT);          // energy-conserving slice integration (Hillaire 2015 simplified)
    T *= sliceT;
  }
  if (u_mode == 1) { o = vec4(scat, T); return; }
  if (u_debug == 1) { o = vec4(scat, 1); return; }
  if (u_debug == 2) { o = vec4(vec3(SunVis(P + vec3(0.0, 2.0, 0.0))), 1); return; }   // must match the engine's shadows on surfaces
  o = vec4(col * T + scat, 1.0);
}
)";

static const char* kAtmosCS = R"(#version 430
layout(local_size_x = 1, local_size_y = 1, local_size_z = 64) in;
layout(rgba16f, binding = 0) writeonly uniform image3D u_out;
uniform sampler2DShadow u_sh1; uniform sampler2DShadow u_sh2; uniform mat4 u_shVP1; uniform mat4 u_shVP2; uniform int u_haveSh;
uniform mat4 u_invVP; uniform vec3 u_cam; uniform vec3 u_sunDir; uniform vec3 u_sunCol; uniform vec3 u_ambCol;
uniform float u_density; uniform float u_falloff; uniform float u_maxDist; uniform float u_g; uniform float u_shaftContrast; uniform float u_volA; uniform vec2 u_vsize;
uniform sampler2D u_skyTex; uniform float u_hazeSky, u_ambScale;
shared vec4 sh[64]; shared vec4 res[64];
vec3 AmbDyn() {   // ambient in-scatter colour (already * 4pi): level fog hue pulled toward the measured sky colour
  vec3 cpu = u_ambCol; vec4 s = texelFetch(u_skyTex, ivec2(0), 0);
  if (s.a < 0.5 || u_hazeSky <= 0.0) return cpu;
  vec3 sn = min(s.rgb * 0.75 / max(dot(s.rgb, vec3(0.2126, 0.7152, 0.0722)), 1e-3), vec3(1.5)) * u_ambScale * 12.566;
  return mix(cpu, sn, u_hazeSky);
}
float SunVis(vec3 X) {
  if (u_haveSh == 0) return 1.0;
  vec3 c = (u_shVP1 * vec4(X, 1.0)).xyz;
  if (all(greaterThan(c, vec3(0.002))) && all(lessThan(c, vec3(0.998)))) return 1.0 - texture(u_sh1, c);
  c = (u_shVP2 * vec4(X, 1.0)).xyz;
  if (all(greaterThan(c, vec3(0.002))) && all(lessThan(c, vec3(0.998)))) return 1.0 - texture(u_sh2, c);
  return 1.0;
}
float DistOf(float s) { return u_maxDist * (exp(u_volA * s) - 1.0) / (exp(u_volA) - 1.0); }
void main() {
  uint k = gl_LocalInvocationID.z; ivec2 id = ivec2(gl_WorkGroupID.xy);
  vec2 uv = (vec2(id) + 0.5) / u_vsize;
  vec4 w = u_invVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
  vec3 dir = normalize(w.xyz / w.w - u_cam);
  float d0 = DistOf(float(k) / 64.0), d1 = DistOf(float(k + 1u) / 64.0), len = d1 - d0;
  float cosT = dot(dir, u_sunDir);
  float phase = (1.0 - u_g * u_g) / (12.566 * pow(max(1.0 + u_g * u_g - 2.0 * u_g * cosT, 1e-4), 1.5));
  vec3 Lacc = vec3(0.0); float dens = 0.0; vec3 ambDyn = AmbDyn();
  for (int i = 0; i < 2; i++) {
    float t = mix(d0, d1, 0.25 + 0.5 * float(i));
    vec3 X = u_cam + dir * t;
    dens += 0.5 * u_density * exp(-max(X.y - u_cam.y, -2000.0) * u_falloff);
    float vis = mix(1.0, SunVis(X), u_shaftContrast);
    Lacc += 0.5 * (u_sunCol * (vis * phase) + ambDyn * (1.0 / 12.566));
  }
  float ext = dens * len;
  sh[k] = vec4(Lacc * (1.0 - exp(-ext)), ext);
  barrier();
  if (k == 0u) {
    vec3 scat = vec3(0.0); float T = 1.0;
    for (int j = 0; j < 64; j++) { scat += T * sh[j].rgb; T *= exp(-sh[j].a); res[j] = vec4(scat, T); }
  }
  barrier();
  imageStore(u_out, ivec3(id, int(k)), res[k]);
}
)";

static const char* kSkyEst = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_color; uniform sampler2D u_prev; uniform vec3 u_cam; uniform float u_blend;
void main() {
  vec3 acc = vec3(0.0); float ws = 0.0;
  for (int iy = 0; iy < 10; iy++) for (int ix = 0; ix < 16; ix++) {
    vec2 q = (vec2(ix, iy) + 0.5) / vec2(16.0, 10.0);
    if (texture(u_depth, q).r < 0.99999) continue;
    float el = normalize(WorldPos(q) - u_cam).y;                 // sky near the horizon dominates (that is the colour the haze blends into)
    float w = exp(-abs(el) * 5.0) + 0.02;
    acc += texture(u_color, q).rgb * w; ws += w;
  }
  vec4 prev = texelFetch(u_prev, ivec2(0), 0);
  if (ws < 0.05) { o = prev; return; }                           // no sky in view: keep the last estimate
  vec3 t = acc / ws;
  o = vec4(prev.a > 0.5 ? mix(prev.rgb, t, u_blend) : t, 1.0);
}
)";
static struct { GLuint prog, tex[2]; int idx; bool failed, init; } skyE;
static int g_atHazeFromSky = 1; static float g_atHazeSky = 0.0f, g_atBlurPx = 0.0f, g_atBlurStart = 4000.0f, g_atBlurRange = 12000.0f; static int g_atContactStable = 1;

typedef void(WINAPI* glDispatchCompute_t)(GLuint, GLuint, GLuint);
typedef void(WINAPI* glBindImageTexture_t)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
typedef void(WINAPI* glMemoryBarrier_t)(GLbitfield);
typedef void(WINAPI* glTexImage3D_t)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void(WINAPI* glDeleteTextures_t)(GLsizei, const GLuint*);
typedef GLenum(WINAPI* glGetError_t)();
static struct { bool tried, ok, failed; GLuint prog, tex; int fw, fh; glDispatchCompute_t Dispatch; glBindImageTexture_t BindImage; glMemoryBarrier_t Barrier; glTexImage3D_t TexImage3D; glDeleteTextures_t DelTex; glGetError_t GetErr; } vol;
static int g_atFroxel = 1, g_atFroxelPx = 12;
static float g_atSkyH = 0.25f, g_atHazeHue = 0.25f, g_atIntensity = 2.5f, g_atDesat = 0.7f, g_atLift = 0.35f, g_atSkyHaze = 0.5f;

static bool InitVolume() {
    if (vol.tried) return vol.ok && !vol.failed;
    vol.tried = true;
    if (g_dbgForce & 1) { logf("[fallback] FORCED froxel failure -> ray-march path"); vol.failed = true; return false; }
    const char* ver = (const char*)o_glGetString(0x1F02);
    int maj = 0, mnr = 0; if (ver) sscanf(ver, "%d.%d", &maj, &mnr);
    if (maj < 4 || (maj == 4 && mnr < 3)) { logf("[atmos] froxel: GL %s lacks compute, using ray-march path", ver ? ver : "?"); vol.failed = true; return false; }
    vol.Dispatch = (glDispatchCompute_t)GLProc("glDispatchCompute"); vol.BindImage = (glBindImageTexture_t)GLProc("glBindImageTexture");
    vol.Barrier = (glMemoryBarrier_t)GLProc("glMemoryBarrier"); vol.TexImage3D = (glTexImage3D_t)GLProc("glTexImage3D"); vol.DelTex = (glDeleteTextures_t)GLProc("glDeleteTextures");
    vol.GetErr = (glGetError_t)GLProc("glGetError");
    if (!vol.Dispatch || !vol.BindImage || !vol.Barrier || !vol.TexImage3D || !vol.DelTex || !vol.GetErr) { logf("[atmos] froxel: missing entry points"); vol.failed = true; return false; }
    GLuint s = o_glCreateShader(0x91B9); o_glShaderSource(s, 1, &kAtmosCS, nullptr); gx.CompileShader(s);
    GLint ok = 0; gx.GetShaderiv(s, 0x8B81, &ok);
    if (!ok) { char buf[4096]; gx.GetShaderInfoLog(s, sizeof buf, nullptr, buf); logf("[atmos] froxel compute compile FAILED:\n%s", buf); vol.failed = true; return false; }
    vol.prog = gx.CreateProgram(); o_glAttachShader(vol.prog, s); o_glLinkProgram(vol.prog);
    gx.GetProgramiv(vol.prog, 0x8B82, &ok);
    if (!ok) { char buf[4096]; gx.GetProgramInfoLog(vol.prog, sizeof buf, nullptr, buf); logf("[atmos] froxel compute link FAILED:\n%s", buf); vol.failed = true; return false; }
    vol.ok = true; logf("[atmos] froxel compute path ready"); return true;
}
static bool EnsureVolTex(int w, int h) {
    int fw = (w + g_atFroxelPx - 1) / g_atFroxelPx, fh = (h + g_atFroxelPx - 1) / g_atFroxelPx;
    if (vol.tex && vol.fw == fw && vol.fh == fh) return true;
    if (vol.tex) vol.DelTex(1, &vol.tex);
    gx.GenTextures(1, &vol.tex); o_glBindTexture(0x806F, vol.tex);
    while (vol.GetErr()) {}
    vol.TexImage3D(0x806F, 0, 0x881A, fw, fh, 64, 0, 0x1908, 0x1406, nullptr);
    if (vol.GetErr()) { logf("[atmos] froxel 3D texture allocation failed (%dx%dx64)", fw, fh); vol.failed = true; return false; }
    gx.TexParameteri(0x806F, 0x2801, 0x2601); gx.TexParameteri(0x806F, 0x2800, 0x2601);
    gx.TexParameteri(0x806F, 0x2802, 0x812F); gx.TexParameteri(0x806F, 0x2803, 0x812F); gx.TexParameteri(0x806F, 0x8072, 0x812F);
    vol.fw = fw; vol.fh = fh; logf("[atmos] froxel volume %dx%dx64", fw, fh); return true;
}

static struct { bool init, failed; GLuint prog, tex, low; int w, h, lw, lh; } atm;
static int g_atHalf = 1; static int g_atEnable = 1, g_atSteps = 24, g_atDebug; static bool g_atOffFile; static float g_atContact = 0.35f, g_atContactLen = 25.0f, g_atContactThick = 6.0f, g_atShaftContrast = 0.5f, g_atFogMul = 1.0f;
static float g_atDensity = 0.000015f, g_atFalloff = 0.0015f, g_atMaxDist = 40000.0f, g_atSunInt = 1.4f, g_atG = 0.6f, g_atAmb = 1.0f, g_atSunCol[3] = {1.0f, 0.86f, 0.66f};

static void LoadAtmosConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[64];
    g_atHalf = GetPrivateProfileIntA("atmos", "HalfRes", 1, ini); g_atEnable = GetPrivateProfileIntA("atmos", "Enable", 1, ini); g_atSteps = GetPrivateProfileIntA("atmos", "Steps", 24, ini);
#define F(key, def, var) GetPrivateProfileStringA("atmos", key, def, b, sizeof b, ini); var = (float)atof(b);
    F("Density", "0.000015", g_atDensity) F("HeightFalloff", "0.0015", g_atFalloff) F("MaxDist", "40000", g_atMaxDist)
    F("SunIntensity", "1.4", g_atSunInt) F("Anisotropy", "0.6", g_atG) F("AmbientScale", "1.0", g_atAmb)
    F("ContactShadows", "0.35", g_atContact) F("ContactLength", "25", g_atContactLen) F("ContactThickness", "6", g_atContactThick) F("FogAmount", "1.0", g_atFogMul)
    F("ShaftContrast", "0.5", g_atShaftContrast) F("Intensity", "2.5", g_atIntensity) F("Desaturation", "0.7", g_atDesat) F("HazeLift", "0.35", g_atLift) F("SkyHaze", "0.5", g_atSkyHaze) F("SkyHazeHeight", "0.25", g_atSkyH) F("HazeHue", "0.25", g_atHazeHue) F("HazeSky", "0.0", g_atHazeSky) F("DepthBlur", "0", g_atBlurPx) F("BlurStart", "4000", g_atBlurStart) F("BlurRange", "12000", g_atBlurRange)
    g_atContactStable = GetPrivateProfileIntA("atmos", "ContactStable", 1, ini);
    g_atFroxel = GetPrivateProfileIntA("atmos", "Froxel", 1, ini); g_atFroxelPx = GetPrivateProfileIntA("atmos", "FroxelPixels", 12, ini); if (g_atFroxelPx < 4) g_atFroxelPx = 4; if (g_atFroxelPx > 64) g_atFroxelPx = 64;
#undef F
    GetPrivateProfileStringA("atmos", "SunColor", "1.0,0.86,0.66", b, sizeof b, ini);
    sscanf(b, "%f,%f,%f", &g_atSunCol[0], &g_atSunCol[1], &g_atSunCol[2]);
}
static void PollAtmosFlags() {
    char p[MAX_PATH]; std::string s;
    snprintf(p, sizeof p, "%styrender\\atmos_off", g_dir); g_atOffFile = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    snprintf(p, sizeof p, "%styrender\\atmos_debug", g_dir); g_atDebug = ReadFileStr(p, s) ? atoi(s.c_str()) : 0;
}

static void RunAtmos(GLuint depthTex, int w, int h) {
    if ((!g_atEnable && g_atContact <= 0) || g_atOffFile || atm.failed || !g_haveVP || g_vpFrame != g_frame || !g_colorCopyTex) return;
    if (!LoadGX()) { atm.failed = true; return; }
    if (!atm.init) {
        atm.prog = CompileProg("atmos", kVS, "#version 330\n", kCommon, kAtmos);
        if (!atm.prog) { atm.failed = true; return; }
        if (!ao.vao) gx.GenVertexArrays(1, &ao.vao);
        if (!ao.fbo) gx.GenFramebuffers(1, &ao.fbo);
        atm.init = true;
    }
    if (g_atEnable && g_atFroxel && g_atContact <= 0 && !InitVolume()) { static int once; if (!once++) logf("[fallback] froxel unavailable -> atmosphere OFF (no heavy ray-march fallback)"); atm.failed = true; return; }   // fallback policy: failure -> OFF, never a slower path
    typedef void(WINAPI* glCopyTexSubImage2D_t)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
    static glCopyTexSubImage2D_t copySub = (glCopyTexSubImage2D_t)GLProc("glCopyTexSubImage2D");
    typedef void(WINAPI* glUniform3f_t)(GLint, GLfloat, GLfloat, GLfloat);
    static glUniform3f_t u3 = (glUniform3f_t)GLProc("glUniform3f");
    if (atm.w != w || atm.h != h) {   // private copy of the current FB0 colour (AO already applied) as the pass input
        gx.GenTextures(1, &atm.tex); o_glBindTexture(0x0DE1, atm.tex);
        o_glTexImage2D(0x0DE1, 0, 0x8058, w, h, 0, 0x1908, 0x1401, nullptr);
        gx.TexParameteri(0x0DE1, 0x2801, 0x2600); gx.TexParameteri(0x0DE1, 0x2800, 0x2600);
        atm.w = w; atm.h = h;
    }
    float invVP[16]; if (!Invert4(g_sceneVP, invVP)) return;
    // camera + view matrix (same derivation as RunAO)
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
    float sun[3] = {0.3f, 0.8f, 0.3f};
    if (g_haveShadowDir) { float l = sqrtf(g_shadowDir[0]*g_shadowDir[0] + g_shadowDir[1]*g_shadowDir[1] + g_shadowDir[2]*g_shadowDir[2]); if (l > 1e-4f) for (int i = 0; i < 3; i++) sun[i] = -g_shadowDir[i] / l; }
    // haze colour = level fog hue normalised to a bright luminance (0.75): fog must lift distant areas, not darken the image
    float amb[3] = {0.6f, 0.7f, 0.85f}; if (g_haveFog) for (int i = 0; i < 3; i++) amb[i] = g_fogColour[i];
    { float l = 0.2126f * amb[0] + 0.7152f * amb[1] + 0.0722f * amb[2]; if (l > 1e-3f) for (int i = 0; i < 3; i++) amb[i] = g_atHazeHue * fminf(amb[i] * 0.75f / l, 1.5f) + (1.0f - g_atHazeHue) * 0.75f; }   // mostly neutral haze (HazeHue = share of the level fog hue)
    bool haveSh = g_haveShadowVP[0] && g_haveShadowVP[1] && g_shadowTex[0] && g_shadowTex[1];

    // save state
    GLint prevFBO, prevVP[4], prevUnit, prevTex[4], prevVAO, prevBlendSrc, prevBlendDst; GLboolean prevDepthMask, prevColorMask[4];
    gx.GetIntegerv(0x8CA6, &prevFBO); gx.GetIntegerv(0x0BA2, prevVP); gx.GetIntegerv(0x84E0, &prevUnit);
    for (int i = 0; i < 4; i++) { o_glActiveTexture(0x84C0 + i); gx.GetIntegerv(0x8069, &prevTex[i]); }
    gx.GetIntegerv(0x85B5, &prevVAO); gx.GetIntegerv(0x0BE1, &prevBlendSrc); gx.GetIntegerv(0x0BE0, &prevBlendDst);
    gx.GetBooleanv(0x0B72, &prevDepthMask); gx.GetBooleanv(0x0C23, prevColorMask);
    GLboolean eBlend = gx.IsEnabled(0x0BE2), eDepth = gx.IsEnabled(0x0B71), eCull = gx.IsEnabled(0x0B44), eScissor = gx.IsEnabled(0x0C11), eStencil = gx.IsEnabled(0x0B90);
    GLuint prevProg = g_curProg;
    o_glDisable(0x0BE2); o_glDisable(0x0B71); o_glDisable(0x0B44); o_glDisable(0x0C11); o_glDisable(0x0B90);
    o_glDepthMask(0); o_glColorMask(1, 1, 1, 1); gx.BindVertexArray(ao.vao);

    o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
    o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, atm.tex); copySub(0x0DE1, 0, 0, 0, 0, 0, w, h);
    // sky colour estimate (1x1, GPU only, smoothed over frames) -> haze colour follows the real sky
    bool useSky = false;
    if (g_atHazeFromSky && g_atHazeSky > 0.0f && !skyE.failed) {
        if (!skyE.init) {
            skyE.init = true; skyE.prog = CompileProg("skyest", kVS, "#version 330\n", kCommon, kSkyEst);
            if (!skyE.prog) skyE.failed = true;
            else { static const float z4[4] = {0, 0, 0, 0}; for (int i = 0; i < 2; i++) { gx.GenTextures(1, &skyE.tex[i]); o_glBindTexture(0x0DE1, skyE.tex[i]);
                       o_glTexImage2D(0x0DE1, 0, 0x881A, 1, 1, 0, 0x1908, 0x1406, z4); gx.TexParameteri(0x0DE1, 0x2801, 0x2600); gx.TexParameteri(0x0DE1, 0x2800, 0x2600); } }
        }
        if (!skyE.failed) {
            GLuint sp = skyE.prog; o_glUseProgram(sp);
            gx.Uniform1i(o_glGetUniformLocation(sp, "u_depth"), 0); gx.Uniform1i(o_glGetUniformLocation(sp, "u_color"), 1); gx.Uniform1i(o_glGetUniformLocation(sp, "u_prev"), 6);
            o_glUniformMatrix4fv(o_glGetUniformLocation(sp, "u_invVP"), 1, 0, invVP); gx.Uniform1i(o_glGetUniformLocation(sp, "u_zMode"), g_aoZMode);
            u3(o_glGetUniformLocation(sp, "u_cam"), cam[0], cam[1], cam[2]); gx.Uniform1f(o_glGetUniformLocation(sp, "u_blend"), 0.08f);
            o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
            o_glActiveTexture(0x84C6); o_glBindTexture(0x0DE1, skyE.tex[skyE.idx]);
            o_glBindFramebuffer(0x8D40, ao.fbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, skyE.tex[skyE.idx ^ 1], 0); o_glViewport(0, 0, 1, 1);
            o_glDrawArrays(4, 0, 3);
            o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
            skyE.idx ^= 1; o_glBindTexture(0x0DE1, skyE.tex[skyE.idx]); useSky = true;
        }
    }
    GLuint p = atm.prog; o_glUseProgram(p);
    auto I = [&](const char* n, int v) { gx.Uniform1i(o_glGetUniformLocation(p, n), v); };
    auto F = [&](const char* n, float v) { gx.Uniform1f(o_glGetUniformLocation(p, n), v); };
    I("u_depth", 0); I("u_color", 1); I("u_sh1", 2); I("u_sh2", 3); I("u_haveSh", haveSh ? 1 : 0);
    o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_invVP"), 1, 0, invVP); o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_view"), 1, 0, view);
    gx.Uniform2f(o_glGetUniformLocation(p, "u_fullSize"), (float)w, (float)h); I("u_zMode", g_aoZMode);
    if (haveSh) { o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_shVP1"), 1, 0, g_shadowVP[0]); o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_shVP2"), 1, 0, g_shadowVP[1]); }
    u3(o_glGetUniformLocation(p, "u_cam"), cam[0], cam[1], cam[2]); u3(o_glGetUniformLocation(p, "u_sunDir"), sun[0], sun[1], sun[2]);
    u3(o_glGetUniformLocation(p, "u_sunCol"), g_atSunCol[0] * g_atSunInt, g_atSunCol[1] * g_atSunInt, g_atSunCol[2] * g_atSunInt);
    u3(o_glGetUniformLocation(p, "u_ambCol"), amb[0] * g_atAmb * 12.566f, amb[1] * g_atAmb * 12.566f, amb[2] * g_atAmb * 12.566f);   // * 4pi cancels the iso phase
    F("u_density", g_atEnable ? g_atDensity * g_atIntensity * g_atFogMul * g_fogAmount / fmaxf(g_fogDistance, 0.05f) : 0.0f);
    o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_VP"), 1, 0, g_sceneVP); F("u_contact", g_atContact); F("u_contactLen", g_atContactLen); F("u_contactThick", g_atContactThick); F("u_shaftContrast", g_atShaftContrast); F("u_falloff", g_atFalloff); I("u_steps", g_atEnable ? g_atSteps : 0); F("u_maxDist", g_atMaxDist); F("u_g", g_atG);
    F("u_frameNoise", g_atContactStable ? 0.0f : fmodf(g_frame * 0.6180339f, 1.0f));
    I("u_skyTex", 6); F("u_hazeSky", useSky ? g_atHazeSky : 0.0f); F("u_ambScale", g_atAmb); F("u_blurStart", g_atBlurStart); F("u_blurRange", fmaxf(g_atBlurRange, 1.0f)); F("u_blurPx", g_atBlurPx * h / 1440.0f); gx.Uniform2f(o_glGetUniformLocation(p, "u_rcp"), 1.0f / w, 1.0f / h); I("u_debug", g_atDebug);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
    o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, haveSh ? g_shadowTex[0] : 0);
    o_glActiveTexture(0x84C3); o_glBindTexture(0x0DE1, haveSh ? g_shadowTex[1] : 0);
    bool useVol = g_atEnable && g_atFroxel && InitVolume() && EnsureVolTex(w, h);
    if (useVol) {
        GLuint cs = vol.prog; o_glUseProgram(cs);
        auto CI = [&](const char* n, int v) { gx.Uniform1i(o_glGetUniformLocation(cs, n), v); };
        auto CF = [&](const char* n, float v) { gx.Uniform1f(o_glGetUniformLocation(cs, n), v); };
        const float volA = 5.0f;
        CI("u_sh1", 2); CI("u_sh2", 3); CI("u_haveSh", haveSh ? 1 : 0);
        if (haveSh) { o_glUniformMatrix4fv(o_glGetUniformLocation(cs, "u_shVP1"), 1, 0, g_shadowVP[0]); o_glUniformMatrix4fv(o_glGetUniformLocation(cs, "u_shVP2"), 1, 0, g_shadowVP[1]); }
        o_glUniformMatrix4fv(o_glGetUniformLocation(cs, "u_invVP"), 1, 0, invVP);
        u3(o_glGetUniformLocation(cs, "u_cam"), cam[0], cam[1], cam[2]); u3(o_glGetUniformLocation(cs, "u_sunDir"), sun[0], sun[1], sun[2]);
        u3(o_glGetUniformLocation(cs, "u_sunCol"), g_atSunCol[0] * g_atSunInt, g_atSunCol[1] * g_atSunInt, g_atSunCol[2] * g_atSunInt);
        u3(o_glGetUniformLocation(cs, "u_ambCol"), amb[0] * g_atAmb * 12.566f, amb[1] * g_atAmb * 12.566f, amb[2] * g_atAmb * 12.566f);
        CF("u_density", g_atDensity * g_atIntensity * g_atFogMul * g_fogAmount / fmaxf(g_fogDistance, 0.05f)); CF("u_falloff", g_atFalloff); CF("u_maxDist", g_atMaxDist);
        CI("u_skyTex", 6); CF("u_hazeSky", useSky ? g_atHazeSky : 0.0f); CF("u_ambScale", g_atAmb); CF("u_g", g_atG); CF("u_shaftContrast", g_atShaftContrast); CF("u_volA", volA); gx.Uniform2f(o_glGetUniformLocation(cs, "u_vsize"), (float)vol.fw, (float)vol.fh);
        vol.BindImage(0, vol.tex, 0, 1, 0, 0x88B9 /*WRITE_ONLY*/, 0x881A);
        vol.Dispatch(vol.fw, vol.fh, 1);
        vol.Barrier(0x20 | 0x8);
        vol.BindImage(0, 0, 0, 0, 0, 0x88B9, 0x881A);
        o_glActiveTexture(0x84C5); o_glBindTexture(0x806F, vol.tex);
        o_glActiveTexture(0x84C0);
        o_glUseProgram(p);
        I("u_vol", 5); F("u_volA", volA); F("u_volN", 64.0f); F("u_desat", g_atDesat); F("u_lift", g_atLift); F("u_skyHaze", g_atSkyHaze); F("u_skyH", g_atSkyH);
        u3(o_glGetUniformLocation(p, "u_haze"), amb[0] * g_atAmb, amb[1] * g_atAmb, amb[2] * g_atAmb);
        I("u_mode", 3);
    } else
    if (g_atHalf && !g_atDebug && g_atEnable) {
        int hw = w / 2, hh = h / 2;
        if (!atm.low || atm.lw != hw || atm.lh != hh) {
            atm.low = MakeTex(hw, hh, 0x881A, 0x1908); o_glBindTexture(0x0DE1, atm.low); gx.TexParameteri(0x0DE1, 0x2801, 0x2601); gx.TexParameteri(0x0DE1, 0x2800, 0x2601);
            atm.lw = hw; atm.lh = hh; logf("[atmos] half-res target %dx%d", hw, hh);
        }
        I("u_mode", 1); gx.Uniform2f(o_glGetUniformLocation(p, "u_halfSize"), (float)hw, (float)hh);
        o_glBindFramebuffer(0x8D40, ao.fbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, atm.low, 0); o_glViewport(0, 0, hw, hh);
        o_glDrawArrays(4, 0, 3);
        o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
        o_glActiveTexture(0x84C4); o_glBindTexture(0x0DE1, atm.low); I("u_low", 4); I("u_mode", 2);
        o_glActiveTexture(0x84C0);
    } else { I("u_mode", 0); }
    o_glDrawArrays(4, 0, 3);
    if (g_atHalf) { o_glActiveTexture(0x84C4); o_glBindTexture(0x0DE1, 0); o_glActiveTexture(0x84C0); }
    if (useVol) { o_glActiveTexture(0x84C5); o_glBindTexture(0x806F, 0); o_glActiveTexture(0x84C0); }
    if (useSky) { o_glActiveTexture(0x84C6); o_glBindTexture(0x0DE1, 0); o_glActiveTexture(0x84C0); }
    g_sceneModified = true;

    gx.BindVertexArray(prevVAO);
    for (int i = 0; i < 4; i++) { o_glActiveTexture(0x84C0 + i); o_glBindTexture(0x0DE1, prevTex[i]); }
    o_glActiveTexture(prevUnit);
    o_glBindFramebuffer(0x8D40, prevFBO); o_glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]); o_glUseProgram(prevProg);
    o_glBlendFunc(prevBlendSrc, prevBlendDst);
    if (eBlend) o_glEnable(0x0BE2); if (eDepth) o_glEnable(0x0B71); if (eCull) o_glEnable(0x0B44); if (eScissor) o_glEnable(0x0C11); if (eStencil) o_glEnable(0x0B90);
    o_glDepthMask(prevDepthMask); o_glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
    static bool once; if (!once) { once = true; logf("[atmos] first run: shadows=%d sun=(%.2f %.2f %.2f) fog=(%.2f %.2f %.2f) shTex=%u,%u", haveSh, sun[0], sun[1], sun[2], amb[0], amb[1], amb[2], g_shadowTex[0], g_shadowTex[1]); }
}

static void OnNamedUniformMat(int id, const GLfloat* v, GLboolean t) {
    if (id != 1 && id != 2) return;
    float* m = g_shadowVP[id - 1];
    if (t) { for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) m[c * 4 + r] = v[r * 4 + c]; } else memcpy(m, v, 64);
    g_haveShadowVP[id - 1] = true;
}
static void OnNamedUniform4(int id, const GLfloat* v) {
    if (id == 3) { memcpy(g_shadowDir, v, 16); g_haveShadowDir = true; }
    else if (id == 4) { memcpy(g_fogColour, v, 16); g_haveFog = true; }
}