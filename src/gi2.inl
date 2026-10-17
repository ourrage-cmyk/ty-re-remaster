// gi2.inl — Hi-Z screen-space GI (included from gi.inl; falls back to the legacy linear-march path in gi.inl on any failure).
// Pipeline (all passes GL 3.3):
//   0) depth pyramid  R32F, level 0 = 1/viewZ per full-res pixel (0 = sky), level n = max (nearest surface) of the 2x2 (odd sizes: 3) cell below
//   1) trace (half res, MRT): per pixel `Rays` cosine rays, hierarchical screen-space traversal of the pyramid (1/z is linear in screen space);
//      outputs raw radiance and a G-buffer (view normal from depth, viewZ)
//   2) temporal (half res, MRT): reprojection with bilinear depth-validated taps, colour + luminance moments + history length
//   3) a-trous wavelet filter (SVGF style, `Atrous` iterations, strides 1,2,4,..) guided by variance, depth planes and normals
//   4) composite onto FB0: depth-aware 2x2 upsample, shade-weighted desaturated albedo proxy (same look as the legacy path)
// Config [gi]: Mode=1 (0 = legacy path) Atrous=3 HiZIter=0 (0 -> Steps*4 traversal iterations) Prof=0 (log per-pass GPU ms); the old keys keep their meaning.

static int g_giMode = 1, g_giAtrous = 3, g_giHizIter = 0, g_giProf = 0;
static int g_giCharOn = 1; static float g_giCharKeep = 0.0f, g_giCharGI = 0.3f, g_giCharH = 160.0f, g_giCharR = 70.0f;   // [gi] Characters/CharacterColour/CharacterGI/CharacterHeight/CharacterRadius
static float g_giSigmaL = 4.0f, g_giBlur = 0.0f; static int g_giNoiseCycle = 0;   // [gi] SourceBlur (cone footprint px per px of ray length, 0 = off), NoiseCycle (0 = noise changes every frame, N = repeats every N frames, 1 = static)
 static int g_giSweep = 0; static const char* g_giVarName = "";
// Uniform values for a distance-LOD ramp (globals and [lod] loader are in ao.inl). Off: the ramp is parked far away with MinScale 1,
// so the keep factor is exactly 1 and the shader output is unchanged.
static void LodUniforms(GLint s, GLint e, GLint m, bool on) {
    if (on) { gx.Uniform1f(s, g_lodStart); gx.Uniform1f(e, g_lodEnd); gx.Uniform1f(m, g_lodMin); }
    else { gx.Uniform1f(s, 1e9f); gx.Uniform1f(e, 2e9f); gx.Uniform1f(m, 1.0f); }
}
// [gi] Sweep=1 (benchmark helper): cycles through variants every 150 frames and logs the per-pass GPU times of each.
static void GISweepApply() {
    static const struct { const char* n; int mode, rays, atrous, iters; } V[] = {
        {"legacy rays4", 0, 4, 3, 0}, {"legacy rays1", 0, 1, 3, 0}, {"hiz r1 a3", 1, 1, 3, 0}, {"hiz r2 a3", 1, 2, 3, 0}, {"hiz r4 a3", 1, 4, 3, 0},
        {"hiz r1 a0", 1, 1, 0, 0}, {"hiz r1 a3 it16", 1, 1, 3, 16}, {"hiz r1 a3 it64", 1, 1, 3, 64}, {"hiz r1 a5", 1, 1, 5, 0}};
    int nv = (int)(sizeof V / sizeof V[0]); int v = (int)((g_frame / 150) % nv); static int last = -1;
    if (v != last) { last = v; g_giMode = V[v].mode; g_giRays = V[v].rays; g_giAtrous = V[v].atrous; g_giHizIter = V[v].iters; g_giVarName = V[v].n; g_giProf = 1; extern void GIResetProf(); GIResetProf(); }
}


// ---- bounce-source exclusion: draws whose albedo texture is tagged emissive / transparent (water, glass, particles, glow) write alpha = 0 into the default framebuffer
// with a second, colour-masked pass; RunGI copies FB0 into a mask texture and the trace treats hits on mask pixels as unlit (the pixel can still RECEIVE bounce light).
// [gi] ExcludeSources: bit 1 = emissive tagged, bit 2 = transparent tagged, bit 4 = character draws (Ty, NPCs: never a colour source, replaces the capsule heuristic), 0 = off (default 7). Debug: DebugView=3 shows the mask.
// The bits are policy column giBits (tex.inl kMatPolicy): a class is excluded when any of its bits is enabled.
static int g_giMask = 7; static unsigned g_giMaskRan, g_giMaskFrame, g_giMaskDraws, g_giMaskFrameDraws;
typedef void(WINAPI* gm_bfs_t)(GLenum, GLenum, GLenum, GLenum);
static struct { GLint bs, bd, bsa, bda, fbo; GLboolean cm[4], dm, blend; } gmS; static gm_bfs_t gmBFS; static bool gmTried;
static bool GIMaskBegin() {
    if (!g_giMask || !g_giEnable || g_giOffFile || g_giStrength <= 0 || g_giMaskRan == g_frame || !g_curProg || !gx.GetIntegerv || !gx.GetBooleanv || !gx.IsEnabled || !g_texFlagArr) return false;
    unsigned unit = g_progDiffUnit[g_curProg < 8192 ? g_curProg : 0] & 31;
    if (!(MatPolicy(g_bound2D[unit]).giBits & g_giMask)) return false;
    if (!gmTried) { gmTried = true; gmBFS = (gm_bfs_t)GLProc("glBlendFuncSeparate"); }
    if (!gmBFS) return false;
    gx.GetIntegerv(0x8CA6, &gmS.fbo); if (gmS.fbo != 0) return false;
    gx.GetIntegerv(0x80C9, &gmS.bs); gx.GetIntegerv(0x80C8, &gmS.bd); gx.GetIntegerv(0x80CB, &gmS.bsa); gx.GetIntegerv(0x80CA, &gmS.bda);
    gx.GetBooleanv(0x0C23, gmS.cm); gx.GetBooleanv(0x0B72, &gmS.dm); gmS.blend = gx.IsEnabled(0x0BE2);
    o_glColorMask(0, 0, 0, 1); o_glDepthMask(0); o_glEnable(0x0BE2); gmBFS(0, 0, 0, 0);          // dst.a = 0 (src and dst factors zero)
    if (g_giMaskFrame != g_frame) { g_giMaskFrame = g_frame; g_giMaskFrameDraws = 0; }
    g_giMaskFrameDraws++; g_giMaskDraws++;
    return true;
}
static void GIMaskEnd() {
    gmBFS((GLenum)gmS.bs, (GLenum)gmS.bd, (GLenum)gmS.bsa, (GLenum)gmS.bda); if (!gmS.blend) o_glDisable(0x0BE2);
    o_glColorMask(gmS.cm[0], gmS.cm[1], gmS.cm[2], gmS.cm[3]); o_glDepthMask(gmS.dm);
}

typedef void(WINAPI* gq_drawbuffers_t)(GLsizei, const GLenum*);
typedef void(WINAPI* gq_u2i_t)(GLint, GLint, GLint);
typedef void(WINAPI* gq_deltex_t)(GLsizei, const GLuint*);
typedef void(WINAPI* gq_gen_t)(GLsizei, GLuint*); typedef void(WINAPI* gq_qc_t)(GLuint, GLenum); typedef void(WINAPI* gq_geti_t)(GLuint, GLenum, GLint*); typedef void(WINAPI* gq_get64_t)(GLuint, GLenum, unsigned long long*);
static struct {
    bool init, failed; int w, h, levels, hizIter; GLuint texMask, texMaskW, texMaskH, prog0, progDown, progTrace, progTemp, progAT, progComp, fbo, texHz, texRaw, texGB, texHC[2], texHM[2], texPing[2];
    int histIdx; bool histValid; unsigned lastFrame; float prevVP[16];
    gq_drawbuffers_t DrawBuffers; gq_u2i_t Uniform2i; gq_deltex_t DeleteTextures;
    // sub-pass GPU timers
    bool qinit, qfail; gq_gen_t qgen; gq_qc_t qc; gq_geti_t qgeti; gq_get64_t qget64; GLuint q[4][8]; unsigned mask[4]; int qcur; double acc[8]; int qn;
} g2;

void GIResetProf() { memset(g2.acc, 0, sizeof g2.acc); g2.qn = 0; }
static void GIMarkBegin() {
    if (!g_giProf) return;
    if (!g2.qinit && !g2.qfail) {
        g2.qgen = (gq_gen_t)GLProc("glGenQueries"); g2.qc = (gq_qc_t)GLProc("glQueryCounter"); g2.qgeti = (gq_geti_t)GLProc("glGetQueryObjectiv"); g2.qget64 = (gq_get64_t)GLProc("glGetQueryObjectui64v");
        if (!g2.qgen || !g2.qc || !g2.qgeti || !g2.qget64) g2.qfail = true; else { for (int s = 0; s < 4; s++) g2.qgen(8, g2.q[s]); g2.qinit = true; }
    }
    if (!g2.qinit) return;
    g2.qcur = (g2.qcur + 1) & 3;                                   // oldest slot: collect, then reuse
    unsigned mk = g2.mask[g2.qcur];
    if ((mk & 1) && (mk & 32)) {
        GLint ok = 0; g2.qgeti(g2.q[g2.qcur][5], 0x8867, &ok);
        if (ok) {
            unsigned long long t[8] = {};
            for (int m = 0; m < 6; m++) if (mk & (1u << m)) g2.qget64(g2.q[g2.qcur][m], 0x8866, &t[m]);
            for (int s = 0; s < 5; s++) { double d = (mk & (2u << s)) && t[s + 1] >= t[s] ? (t[s + 1] - t[s]) / 1e6 : 0.0; g2.acc[s] += d; }
            g2.acc[5] += (t[5] - t[0]) / 1e6; g2.qn++;
            if (g2.qn >= (g_giSweep ? 100 : 300)) {
                logf("[gi] prof [%s]: pyramid %.2f trace %.2f temporal %.2f atrous %.2f composite %.2f | total %.2f ms", g_giVarName, g2.acc[0] / g2.qn, g2.acc[1] / g2.qn, g2.acc[2] / g2.qn, g2.acc[3] / g2.qn, g2.acc[4] / g2.qn, g2.acc[5] / g2.qn);
                memset(g2.acc, 0, sizeof g2.acc); g2.qn = 0;
            }
        }
    }
    g2.mask[g2.qcur] = 0;
    g2.qc(g2.q[g2.qcur][0], 0x8E28); g2.mask[g2.qcur] |= 1;
}
static void GIMark(int m) { if (g_giProf && g2.qinit) { g2.qc(g2.q[g2.qcur][m], 0x8E28); g2.mask[g2.qcur] |= 1u << m; } }

// ------------------------------------------------------------------ shaders
static const char* kGIC2 = R"(
uniform vec2 u_pp;      // projection scale p00, p11
uniform vec2 u_full;    // full-res size (same as u_fullSize)
vec3 PosFromH(vec2 px, float h) { float z = 1.0 / max(h, 1e-9); vec2 ndc = (px + 0.5) / u_full * 2.0 - 1.0; return vec3(ndc * z / u_pp, -z); }
)";

static const char* kGIPyr0 = R"(
in vec2 uv; out vec4 o;
void main() {
  float d = texture(u_depth, uv).r;
  if (d >= 0.99999) { o = vec4(0.0); return; }
  o = vec4(1.0 / max(-ViewPos(uv).z, 1e-3));
}
)";

static const char* kGIPyrDown = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform int u_lod; uniform ivec2 u_srcDim; uniform ivec2 u_dstDim;
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy), s = p * 2;
  int nx = (p.x == u_dstDim.x - 1 && (u_srcDim.x & 1) == 1) ? 3 : 2, ny = (p.y == u_dstDim.y - 1 && (u_srcDim.y & 1) == 1) ? 3 : 2;
  float m = 0.0;
  for (int y = 0; y < ny; y++) for (int x = 0; x < nx; x++) m = max(m, texelFetch(u_src, min(s + ivec2(x, y), u_srcDim - 1), u_lod).r);
  o = vec4(m);
}
)";

#define GI_CHAR_GLSL R"(
uniform vec3 u_ch[8]; uniform int u_nch; uniform vec3 u_chUp; uniform float u_chH; uniform float u_chR; uniform float u_chKeep; uniform float u_chGI;
bool InChar(vec3 P) {
  for (int i = 0; i < 8; i++) { if (i >= u_nch) break;
    vec3 d = P - u_ch[i]; float h = dot(d, u_chUp); if (h < 14.0) continue;
    vec3 q = d - u_chUp * clamp(h, 0.0, u_chH); if (dot(q, q) < u_chR * u_chR) return true; }
  return false;
}
)"
static const char* kGITrace2 = GI_CHAR_GLSL R"(
in vec2 uv; layout(location = 0) out vec4 o0; layout(location = 1) out vec4 o1;
uniform sampler2D u_hz; uniform sampler2D u_color; uniform sampler2D u_mask; uniform int u_useMask;
uniform float u_radius; uniform float u_thick; uniform int u_rays; uniform int u_iters; uniform int u_levels; uniform float u_frame; uniform float u_blur;
uniform float u_lodS; uniform float u_lodE; uniform float u_lodMin;   // [lod] ramp in view depth (world units); lodMin = share of rays kept at the far end
float IGN(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
float H0(ivec2 p) { return texelFetch(u_hz, clamp(p, ivec2(0), ivec2(u_full) - 1), 0).r; }
vec3 Trace(vec3 Ps, vec3 dir) {
  float tmax = u_radius;
  if (dir.z > 1e-4) tmax = min(tmax, (-1.0 - Ps.z) / dir.z);
  if (tmax < 1.0) return vec3(0.0);
  vec3 Pe = Ps + dir * tmax;
  vec3 s0 = vec3((vec2(u_pp.x * Ps.x, u_pp.y * Ps.y) / (-Ps.z) * 0.5 + 0.5) * u_full, 1.0 / (-Ps.z));
  vec3 s1 = vec3((vec2(u_pp.x * Pe.x, u_pp.y * Pe.y) / (-Pe.z) * 0.5 + 0.5) * u_full, 1.0 / (-Pe.z));
  if (any(lessThan(s0.xy, vec2(0.0))) || any(greaterThanEqual(s0.xy, u_full))) return vec3(0.0);
  vec3 d = s1 - s0;
  float tc = 1.0;                                                   // clip the ray to the screen
  if (d.x > 0.0) tc = min(tc, (u_full.x - 0.01 - s0.x) / d.x); else if (d.x < 0.0) tc = min(tc, (0.01 - s0.x) / d.x);
  if (d.y > 0.0) tc = min(tc, (u_full.y - 0.01 - s0.y) / d.y); else if (d.y < 0.0) tc = min(tc, (0.01 - s0.y) / d.y);
  d *= max(tc, 0.0);
  float len = length(d.xy);
  if (len < 1.0) return vec3(0.0);
  vec2 invD = 1.0 / vec2(d.x >= 0.0 ? max(d.x, 1e-6) : min(d.x, -1e-6), d.y >= 0.0 ? max(d.y, 1e-6) : min(d.y, -1e-6));
  vec2 stepUp = vec2(d.x >= 0.0 ? 1.0 : 0.0, d.y >= 0.0 ? 1.0 : 0.0);
  float eps = 0.02 / len, t = eps;
  int level = 1, maxL = min(u_levels - 1, 9);
  bool hit = false; vec3 hp = s0;
  for (int i = 0; i < u_iters; i++) {
    if (t >= 1.0) break;
    vec3 pos = s0 + d * t;
    ivec2 dims = max(ivec2(u_full) >> level, ivec2(1));
    ivec2 cell = clamp(ivec2(pos.xy) >> level, ivec2(0), dims - 1);
    float s = texelFetch(u_hz, cell, level).r;
    vec2 lo = vec2(cell << level), hi = vec2((cell + 1) << level);
    hi = vec2(cell.x == dims.x - 1 ? u_full.x : hi.x, cell.y == dims.y - 1 ? u_full.y : hi.y);
    vec2 tb2 = (mix(lo, hi, stepUp) - s0.xy) * invD;
    float tb = min(tb2.x, tb2.y);
    float sc = s * 0.998;
    if (pos.z <= sc) {                                              // ray is behind the nearest surface of this cell
      if (level == 0) {
        if (1.0 / pos.z - 1.0 / s < u_thick) { hit = true; hp = pos; break; }
        t = tb + eps;                                               // behind a thin occluder: keep going
      } else level--;
    } else {
      float tp = (d.z < 0.0) ? (sc - s0.z) / d.z : 1e9;             // where the ray dips below the cell's nearest depth
      if (tp < tb) {
        if (level == 0) { hit = true; hp = s0 + d * tp; break; }
        t = tp; level--;
      } else { t = tb + eps; level = min(level + 1, maxL); }
    }
  }
  if (!hit) return vec3(0.0);
  if (u_useMask != 0 && texture(u_mask, hp.xy / u_full).a < 0.25) return vec3(0.0);   // water / glass / glow: no bounce source
  vec3 hc;
  float fp = clamp(u_blur * length(hp.xy - s0.xy), 0.0, 40.0);                // cone footprint: far hits integrate a wider patch of the source (tames animated high-contrast sources such as water)
  if (fp > 1.5) {
    vec2 o1 = vec2(fp, fp * 0.5), o2 = vec2(-fp * 0.5, fp);
    hc = texture(u_color, (hp.xy + o1) / u_full).rgb + texture(u_color, (hp.xy - o1) / u_full).rgb + texture(u_color, (hp.xy + o2) / u_full).rgb + texture(u_color, (hp.xy - o2) / u_full).rgb;
    hc = (hc + 2.0 * texture(u_color, hp.xy / u_full).rgb) / 6.0;
  } else hc = texture(u_color, hp.xy / u_full).rgb;
  if (u_nch > 0 && InChar(PosFromH(hp.xy, hp.z))) hc = mix(vec3(dot(hc, vec3(0.2126, 0.7152, 0.0722))), hc, u_chKeep);   // a character is a light source with no colour: its red shorts must not tint the ground
  return hc;
}
void main() {
  ivec2 ip = ivec2(gl_FragCoord.xy) * 2;
  float h = H0(ip);
  if (h <= 0.0) { o0 = vec4(0.0); o1 = vec4(0.0, 0.0, 0.0, -1.0); return; }
  vec3 P = PosFromH(vec2(ip), h);
  float hl = H0(ip - ivec2(1, 0)), hr = H0(ip + ivec2(1, 0)), hb = H0(ip - ivec2(0, 1)), ht = H0(ip + ivec2(0, 1));
  vec3 l = hl > 0.0 ? PosFromH(vec2(ip) - vec2(1, 0), hl) : P, r = hr > 0.0 ? PosFromH(vec2(ip) + vec2(1, 0), hr) : P;
  vec3 b = hb > 0.0 ? PosFromH(vec2(ip) - vec2(0, 1), hb) : P, t = ht > 0.0 ? PosFromH(vec2(ip) + vec2(0, 1), ht) : P;
  vec3 dx = (abs(l.z - P.z) < abs(r.z - P.z)) ? P - l : r - P;
  vec3 dy = (abs(b.z - P.z) < abs(t.z - P.z)) ? P - b : t - P;
  vec3 N = cross(dx, dy); N = (dot(N, N) > 1e-12) ? normalize(N) : vec3(0.0, 0.0, 1.0);
  vec3 T = normalize(abs(N.y) < 0.99 ? cross(N, vec3(0, 1, 0)) : cross(N, vec3(1, 0, 0))), B = cross(N, T);
  float n1 = fract(IGN(gl_FragCoord.xy) + u_frame * 0.7548776662), n2 = fract(IGN(gl_FragCoord.xy + 17.0) + u_frame * 0.5698402910);
  vec3 sum = vec3(0.0);
  vec3 Ps = P + N * 2.0;
  // [lod] distance LOD: lk = share of rays kept at this depth (1 = all, the default). Each ray is kept with probability lk, decided per pixel and
  // per frame, so far pixels get fewer traces without a hard step. The divisor is the expected count, so the mean stays unbiased.
  float lk = 1.0 - (1.0 - u_lodMin) * smoothstep(u_lodS, u_lodE, -P.z);
  float nd = fract(IGN(gl_FragCoord.xy + 91.0) + u_frame * 0.3819660);
  for (int rr = 0; rr < u_rays; rr++) {
    if (fract(nd + float(rr) * 0.618034) >= lk) continue;
    float u1 = fract(n1 + float(rr) / float(u_rays)), u2 = fract(n2 + float(rr) * 0.618034);
    float phi = 6.2831853 * u2, cr = sqrt(u1);
    vec3 dir = normalize(T * (cos(phi) * cr) + B * (sin(phi) * cr) + N * sqrt(max(1.0 - u1, 0.0)));
    sum += Trace(Ps, dir);
  }
  o0 = vec4(sum / max(float(u_rays) * lk, 1e-3), 0.0);
  o1 = vec4(N, -P.z);
}
)";

static const char* kGITemporal2 = R"(
in vec2 uv; layout(location = 0) out vec4 oC; layout(location = 1) out vec4 oM;
uniform sampler2D u_raw; uniform sampler2D u_gb; uniform sampler2D u_hc; uniform sampler2D u_hm;
uniform mat4 u_prevVP; uniform vec2 u_half; uniform float u_blend; uniform int u_histValid;
void main() {
  ivec2 ip = ivec2(gl_FragCoord.xy);
  vec4 raw = texelFetch(u_raw, ip, 0), gb = texelFetch(u_gb, ip, 0);
  if (gb.w < 0.0) { oC = vec4(0.0, 0.0, 0.0, -1.0); oM = vec4(0.0); return; }
  float lum = dot(raw.rgb, vec3(0.2126, 0.7152, 0.0722));
  vec3 hc = vec3(0.0); vec4 hm = vec4(0.0); float wsum = 0.0;
  if (u_histValid != 0) {
    vec3 W = WorldPos((vec2(ip) * 2.0 + 0.5) / u_fullSize);
    vec4 pc = u_prevVP * vec4(W, 1.0);
    if (pc.w > 0.0) {
      vec2 puv = pc.xy / pc.w * 0.5 + 0.5, hp = puv * u_half - 0.5, f = fract(hp); ivec2 b = ivec2(floor(hp));
      for (int i = 0; i < 4; i++) {
        ivec2 o2 = ivec2(i & 1, i >> 1), c = b + o2;
        if (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, ivec2(u_half)))) continue;
        vec4 h = texelFetch(u_hc, c, 0);
        if (h.a < 0.0 || abs(h.a - pc.w) > 0.05 * pc.w + 3.0) continue;         // disocclusion / sky / depth mismatch
        float w = (o2.x == 1 ? f.x : 1.0 - f.x) * (o2.y == 1 ? f.y : 1.0 - f.y);
        hc += h.rgb * w; hm += texelFetch(u_hm, c, 0) * w; wsum += w;
      }
    }
  }
  float alpha = 1.0, len = 0.0, m1 = lum, m2 = lum * lum; vec3 col = raw.rgb;
  if (wsum > 0.05) {
    hc /= wsum; hm /= wsum; len = hm.z;
    alpha = max(1.0 / (len + 1.0), 1.0 - u_blend);
    col = mix(hc, raw.rgb, alpha); m1 = mix(hm.x, lum, alpha); m2 = mix(hm.y, lum * lum, alpha);
  }
  oC = vec4(col, gb.w); oM = vec4(m1, m2, min(len + 1.0, 255.0), 0.0);
}
)";

static const char* kGIAtrous = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform sampler2D u_hm; uniform sampler2D u_gb; uniform int u_step; uniform int u_first; uniform vec2 u_half; uniform float u_sigL;
float Lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float VarAt(ivec2 p) {
  p = clamp(p, ivec2(0), ivec2(u_half) - 1);
  if (u_first == 0) return texelFetch(u_src, p, 0).a;
  vec4 m = texelFetch(u_hm, p, 0);
  float v = max(m.y - m.x * m.x, 0.0);
  if (m.z < 4.0) {                                                    // short history: add a spatial 3x3 luminance variance
    float s1 = 0.0, s2 = 0.0;
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) { float l = Lum(texelFetch(u_src, clamp(p + ivec2(x, y), ivec2(0), ivec2(u_half) - 1), 0).rgb); s1 += l; s2 += l * l; }
    v = max(v, max(s2 / 9.0 - (s1 / 9.0) * (s1 / 9.0), 0.0));
  }
  return v;
}
vec3 PosH(ivec2 q, float z) { vec2 ndc = (vec2(q) * 2.0 + 0.5) / u_full * 2.0 - 1.0; return vec3(ndc * z / u_pp, -z); }
void main() {
  ivec2 ip = ivec2(gl_FragCoord.xy);
  vec4 cs = texelFetch(u_src, ip, 0); vec4 gc = texelFetch(u_gb, ip, 0);
  if (gc.w < 0.0) { o = vec4(0.0); return; }
  vec3 cc = cs.rgb; float lc = Lum(cc);
  float vc = 0.0;                                                     // 3x3 gaussian-prefiltered centre variance
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) vc += VarAt(ip + ivec2(x, y)) * ((x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0) / 16.0);
  float sigL = u_sigL * sqrt(max(vc, 0.0)) + 1e-3;
  vec3 Pc = PosH(ip, gc.w);
  float kern[3] = float[3](0.375, 0.25, 0.0625);
  vec3 sum = cc; float wsum = 1.0, vsum = VarAt(ip), w2 = 1.0;
  sum = cc * 1.0; vsum = VarAt(ip) * 1.0;
  for (int y = -2; y <= 2; y++) for (int x = -2; x <= 2; x++) {
    if (x == 0 && y == 0) continue;
    ivec2 q = ip + ivec2(x, y) * u_step;
    if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, ivec2(u_half)))) continue;
    vec4 gq = texelFetch(u_gb, q, 0);
    if (gq.w < 0.0) continue;
    float wn = pow(max(dot(gc.xyz, gq.xyz), 0.0), 48.0);
    float pd = abs(dot(gc.xyz, PosH(q, gq.w) - Pc));
    float wz = exp(-pd / (0.012 * gc.w + 0.5));
    vec4 sq = texelFetch(u_src, q, 0);
    float wl = exp(-abs(Lum(sq.rgb) - lc) / sigL);
    float w = kern[abs(x)] * kern[abs(y)] / 0.140625 * wn * wz * wl;       // centre kernel weight normalised to 1 by the 0.375^2 factor
    sum += sq.rgb * w; wsum += w; vsum += VarAt(q) * w * w; w2 += 0.0;
  }
  float iw = 1.0 / wsum;
  o = vec4(sum * iw, vsum * iw * iw);
}
)";

static const char* kGIComposite2 = GI_CHAR_GLSL R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_gi; uniform sampler2D u_gb; uniform sampler2D u_hz; uniform sampler2D u_color; uniform vec2 u_half; uniform float u_strength; uniform int u_debug;
void main() {
  ivec2 fp = ivec2(gl_FragCoord.xy);
  float h = texelFetch(u_hz, fp, 0).r;
  if (h <= 0.0) { o = vec4(0.0); return; }
  float z = 1.0 / h;
  vec2 hp = vec2(fp) * 0.5, f = fract(hp); ivec2 b = ivec2(floor(hp));
  vec3 sum = vec3(0.0); float wsum = 0.0;
  for (int i = 0; i < 4; i++) {
    ivec2 o2 = ivec2(i & 1, i >> 1), c = clamp(b + o2, ivec2(0), ivec2(u_half) - 1);
    float zq = texelFetch(u_gb, c, 0).w;
    if (zq < 0.0) continue;
    float bw = (o2.x == 1 ? f.x : 1.0 - f.x) * (o2.y == 1 ? f.y : 1.0 - f.y);
    float dz = abs(zq - z) / z;
    float w = bw * exp(-dz * dz * 300.0) + 1e-5;
    sum += texelFetch(u_gi, c, 0).rgb * w; wsum += w;
  }
  vec3 gi = wsum > 0.0 ? sum / wsum : vec3(0.0);
  vec3 c = texture(u_color, uv).rgb;
  bool onChar = u_nch > 0 && InChar(ViewPos(uv));
  if (u_debug == 2) { o = vec4(onChar ? 1.0 : 0.0, 0.0, 0.0, 1.0); return; }
  if (onChar) gi *= u_chGI;
  float l = max(dot(c, vec3(0.2126, 0.7152, 0.0722)), 0.03);
  vec3 albedo = clamp(mix(vec3(l), c, 0.6) / l * 0.35, 0.0, 1.0);
  float shade = pow(clamp(1.0 - l * 1.4, 0.0, 1.0), 2.0);
  if (u_debug == 1) { o = vec4(gi, 1); return; }
  o = vec4(min(gi * albedo * u_strength * shade, vec3(0.25)), 1.0);
}
)";

// ------------------------------------------------------------------ driver
static GLuint MakeTexF(int w, int h, GLint ifmt) { return MakeTex(w, h, ifmt, 0x1908); }

static bool GIHizInit(int w, int h) {
    if (g2.failed) return false;
    if (!g2.fbo) {
        g2.DrawBuffers = (gq_drawbuffers_t)GLProc("glDrawBuffers"); g2.DeleteTextures = (gq_deltex_t)GLProc("glDeleteTextures"); g2.Uniform2i = (gq_u2i_t)GLProc("glUniform2i");
        if (!g2.DrawBuffers || !g2.Uniform2i) { logf("[gi] Hi-Z disabled: glDrawBuffers missing"); g2.failed = true; return false; }
        const char* h330 = "#version 330\n";
        auto build = [&](const char* nm, const char* body, bool c2) -> GLuint {
            static std::string cat[8]; static int ci; std::string& s = cat[ci++ & 7]; s = std::string(c2 ? kGIC2 : "") + body;   // CompileProg takes 3 fragment parts
            return CompileProg(nm, kVS, h330, kCommon, s.c_str());
        };
        g2.prog0 = build("gi_pyr0", kGIPyr0, false); g2.progDown = build("gi_pyrdown", kGIPyrDown, false);
        g2.progTrace = build("gi_trace", kGITrace2, true); g2.progTemp = build("gi_temporal2", kGITemporal2, false);
        g2.progAT = build("gi_atrous", kGIAtrous, true); g2.progComp = build("gi_composite2", kGIComposite2, false);
        if (!g2.prog0 || !g2.progDown || !g2.progTrace || !g2.progTemp || !g2.progAT || !g2.progComp) { logf("[gi] Hi-Z shaders failed, using legacy path"); g2.failed = true; return false; }
        if (!ao.vao) gx.GenVertexArrays(1, &ao.vao);
        gx.GenFramebuffers(1, &g2.fbo);
    }
    if (g2.w != w || g2.h != h) {
        if (g2.DeleteTextures && g2.texHz) {
            GLuint old[] = {g2.texHz, g2.texRaw, g2.texGB, g2.texHC[0], g2.texHC[1], g2.texHM[0], g2.texHM[1], g2.texPing[0], g2.texPing[1]}; g2.DeleteTextures(9, old);
        }
        int hw = w / 2, hh = h / 2;
        g2.levels = 1; while ((w >> g2.levels) >= 1 || (h >> g2.levels) >= 1) { g2.levels++; if (g2.levels > 14) break; }
        gx.GenTextures(1, &g2.texHz); o_glBindTexture(0x0DE1, g2.texHz);
        for (int l = 0; l < g2.levels; l++) o_glTexImage2D(0x0DE1, l, 0x822E /*R32F*/, w >> l > 0 ? w >> l : 1, h >> l > 0 ? h >> l : 1, 0, 0x1903, 0x1406, nullptr);
        gx.TexParameteri(0x0DE1, 0x2801, 0x2600); gx.TexParameteri(0x0DE1, 0x2800, 0x2600); gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);
        g2.texRaw = MakeTexF(hw, hh, 0x881A); g2.texGB = MakeTexF(hw, hh, 0x881A);
        for (int i = 0; i < 2; i++) { g2.texHC[i] = MakeTexF(hw, hh, 0x881A); g2.texHM[i] = MakeTexF(hw, hh, 0x8814); g2.texPing[i] = MakeTexF(hw, hh, 0x881A); }
        // verify the pyramid levels are renderable
        GLint prevFBO; gx.GetIntegerv(0x8CA6, &prevFBO);
        o_glBindFramebuffer(0x8D40, g2.fbo);
        bool ok = true;
        for (int l = 0; l < g2.levels && ok; l++) { o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g2.texHz, l); ok = gx.CheckFramebufferStatus(0x8D40) == 0x8CD5; }
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g2.texRaw, 0); GLenum two[2] = {0x8CE0, 0x8CE1};
        o_glFramebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, g2.texGB, 0); g2.DrawBuffers(2, two);
        if (ok) ok = gx.CheckFramebufferStatus(0x8D40) == 0x8CD5;
        o_glFramebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, 0, 0); g2.DrawBuffers(1, two);
        o_glBindFramebuffer(0x8D40, prevFBO);
        if (!ok) { logf("[gi] Hi-Z framebuffers incomplete, using legacy path"); g2.failed = true; return false; }
        g2.w = w; g2.h = h; g2.histValid = false; logf("[gi] Hi-Z initialised %dx%d, %d pyramid levels", w, h, g2.levels);
    }
    return true;
}

// Runs the Hi-Z pipeline. GL state save/restore is done by the caller (RunGI). Returns false if it could not run (caller then uses the legacy path).
static bool GIHiz(GLuint depthTex, int w, int h, const float* invVP, const float* view, float p00, float p11) {
    if (!g_giMode || g2.failed || !GIHizInit(w, h)) return false;
    int hw = w / 2, hh = h / 2;
    auto UL = [](GLuint p, const char* n) { return o_glGetUniformLocation(p, n); };
    auto Common = [&](GLuint p) {
        gx.Uniform1i(UL(p, "u_depth"), 0); o_glUniformMatrix4fv(UL(p, "u_invVP"), 1, 0, invVP); o_glUniformMatrix4fv(UL(p, "u_view"), 1, 0, view);
        gx.Uniform2f(UL(p, "u_fullSize"), (float)w, (float)h); gx.Uniform2f(UL(p, "u_full"), (float)w, (float)h); gx.Uniform1i(UL(p, "u_zMode"), g_aoZMode);
        gx.Uniform2f(UL(p, "u_pp"), p00, p11);
    };
    GIMarkBegin();
    auto SetChars = [&](GLuint p) {
        typedef void(WINAPI* u3fv_t)(GLint, GLsizei, const GLfloat*); static u3fv_t u3fv = (u3fv_t)GLProc("glUniform3fv"); typedef void(WINAPI* u3f_t)(GLint, GLfloat, GLfloat, GLfloat); static u3f_t u3f = (u3f_t)GLProc("glUniform3f");
        int n = (g_giCharOn && u3fv && u3f && g_charsFrame == g_frame) ? g_nChars : 0; float cv[24] = {};
        for (int i = 0; i < n; i++) for (int r = 0; r < 3; r++) cv[i * 3 + r] = view[r] * g_chars[i][0] + view[4 + r] * g_chars[i][1] + view[8 + r] * g_chars[i][2] + view[12 + r];
        gx.Uniform1i(UL(p, "u_nch"), n);
        if (n) { u3fv(UL(p, "u_ch"), 8, cv); u3f(UL(p, "u_chUp"), view[4], view[5], view[6]); gx.Uniform1f(UL(p, "u_chH"), g_giCharH); gx.Uniform1f(UL(p, "u_chR"), g_giCharR); gx.Uniform1f(UL(p, "u_chKeep"), g_giCharKeep); gx.Uniform1f(UL(p, "u_chGI"), g_giCharGI); }
    };
    bool useMask = false;
    if (g_giMask && g_giMaskFrame == g_frame && g_giMaskFrameDraws > 0) {      // FB0 alpha = 0 where excluded sources were drawn -> own texture
        // Half-res mask: the trace only samples it at half-res steps, and it is looked up by normalised UV, so no shader change.
        // glCopyTexImage2D cannot downscale (it would grab a quarter of FB0), so FB0 is blitted into the half-res texture with GL_NEAREST.
        if (!g2.texMask) gx.GenTextures(1, &g2.texMask);
        o_glActiveTexture(0x84C3); o_glBindTexture(0x0DE1, g2.texMask);
        if (g2.texMaskW != (GLuint)hw || g2.texMaskH != (GLuint)hh) { o_glTexImage2D(0x0DE1, 0, 0x8058, hw, hh, 0, 0x1908, 0x1401, nullptr); g2.texMaskW = hw; g2.texMaskH = hh; }
        gx.TexParameteri(0x0DE1, 0x2801, 0x2600); gx.TexParameteri(0x0DE1, 0x2800, 0x2600); gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);
        bool scissor = gx.IsEnabled(0x0C11);
        if (scissor) o_glDisable(0x0C11);                                   // scissor clips blits
        o_glBindFramebuffer(0x8CA8, 0); o_glBindFramebuffer(0x8CA9, g2.fbo);  // read FB0, draw into the GI fbo
        GLenum blitBuf[1] = {0x8CE0}; o_glFramebufferTexture2D(0x8CA9, 0x8CE0, 0x0DE1, g2.texMask, 0); g2.DrawBuffers(1, blitBuf);
        o_glBlitFramebuffer(0, 0, w, h, 0, 0, hw, hh, 0x4000 /*COLOR*/, 0x2600 /*NEAREST*/);
        if (scissor) o_glEnable(0x0C11);
        useMask = true;
    }
    g_giMaskRan = g_frame;
    { static unsigned lastLog; if (g_frame - lastLog > 600) { lastLog = g_frame; logf("[gi] source mask: %u marked draws this frame, %u total, copy %d", g_giMaskFrame == g_frame ? g_giMaskFrameDraws : 0, g_giMaskDraws, (int)useMask); } }
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
    o_glBindFramebuffer(0x8D40, g2.fbo);
    GLenum bufs0[1] = {0x8CE0}, bufs01[2] = {0x8CE0, 0x8CE1};
    g2.DrawBuffers(1, bufs0);
    // 0) pyramid
    o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g2.texHz, 0); o_glViewport(0, 0, w, h);
    o_glUseProgram(g2.prog0); Common(g2.prog0); o_glDrawArrays(4, 0, 3);
    o_glUseProgram(g2.progDown); gx.Uniform1i(UL(g2.progDown, "u_src"), 1);
    o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, g2.texHz);
    for (int l = 1; l < g2.levels; l++) {
        int sw = w >> (l - 1) > 0 ? w >> (l - 1) : 1, sh = h >> (l - 1) > 0 ? h >> (l - 1) : 1, dw = w >> l > 0 ? w >> l : 1, dh = h >> l > 0 ? h >> l : 1;
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g2.texHz, l); o_glViewport(0, 0, dw, dh);
        gx.Uniform1i(UL(g2.progDown, "u_lod"), l - 1);
        g2.Uniform2i(UL(g2.progDown, "u_srcDim"), sw, sh); g2.Uniform2i(UL(g2.progDown, "u_dstDim"), dw, dh);
        o_glDrawArrays(4, 0, 3);
    }
    GIMark(1);
    // 1) trace (MRT: raw radiance + gbuffer)
    o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g2.texRaw, 0); o_glFramebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, g2.texGB, 0); g2.DrawBuffers(2, bufs01);
    o_glViewport(0, 0, hw, hh);
    o_glUseProgram(g2.progTrace); Common(g2.progTrace);
    SetChars(g2.progTrace);
    gx.Uniform1i(UL(g2.progTrace, "u_hz"), 1); gx.Uniform1i(UL(g2.progTrace, "u_color"), 2); gx.Uniform1i(UL(g2.progTrace, "u_mask"), 3); gx.Uniform1i(UL(g2.progTrace, "u_useMask"), useMask ? 1 : 0);
    gx.Uniform1f(UL(g2.progTrace, "u_radius"), g_giRadius); gx.Uniform1f(UL(g2.progTrace, "u_thick"), g_giThick);
    gx.Uniform1i(UL(g2.progTrace, "u_rays"), g_giRays < 1 ? 1 : g_giRays); gx.Uniform1i(UL(g2.progTrace, "u_iters"), g_giHizIter > 0 ? g_giHizIter : (g_giSteps < 2 ? 8 : g_giSteps * 4));
    gx.Uniform1i(UL(g2.progTrace, "u_levels"), g2.levels); gx.Uniform1f(UL(g2.progTrace, "u_frame"), (float)(g_giNoiseCycle > 0 ? (g_frame % g_giNoiseCycle) : (g_frame & 4095))); gx.Uniform1f(UL(g2.progTrace, "u_blur"), g_giBlur);
    LodUniforms(UL(g2.progTrace, "u_lodS"), UL(g2.progTrace, "u_lodE"), UL(g2.progTrace, "u_lodMin"), g_lodEnable && g_lodGI);
    o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, g2.texHz);
    o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, g_colorCopyTex);
    o_glDrawArrays(4, 0, 3);
    GIMark(2);
    // 2) temporal (MRT: colour+z, moments)
    int cur = g2.histIdx ^ 1, prev = g2.histIdx;
    o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g2.texHC[cur], 0); o_glFramebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, g2.texHM[cur], 0);
    o_glUseProgram(g2.progTemp); Common(g2.progTemp);
    gx.Uniform1i(UL(g2.progTemp, "u_raw"), 1); gx.Uniform1i(UL(g2.progTemp, "u_gb"), 2); gx.Uniform1i(UL(g2.progTemp, "u_hc"), 3); gx.Uniform1i(UL(g2.progTemp, "u_hm"), 4);
    gx.Uniform2f(UL(g2.progTemp, "u_half"), (float)hw, (float)hh); gx.Uniform1f(UL(g2.progTemp, "u_blend"), g_giBlend);
    gx.Uniform1i(UL(g2.progTemp, "u_histValid"), (g2.histValid && g2.lastFrame + 1 == g_frame) ? 1 : 0);
    o_glUniformMatrix4fv(UL(g2.progTemp, "u_prevVP"), 1, 0, g2.prevVP);
    o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, g2.texRaw);
    o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, g2.texGB);
    o_glActiveTexture(0x84C3); o_glBindTexture(0x0DE1, g2.texHC[prev]);
    o_glActiveTexture(0x84C4); o_glBindTexture(0x0DE1, g2.texHM[prev]);
    o_glDrawArrays(4, 0, 3);
    g2.histIdx = cur; g2.histValid = true; g2.lastFrame = g_frame; memcpy(g2.prevVP, g_sceneVP, sizeof g2.prevVP);
    GIMark(3);
    // 3) a-trous
    o_glFramebufferTexture2D(0x8D40, 0x8CE1, 0x0DE1, 0, 0); g2.DrawBuffers(1, bufs0);
    GLuint finalTex = g2.texHC[cur];
    int nat = g_giAtrous < 0 ? 0 : g_giAtrous > 5 ? 5 : g_giAtrous;
    if (nat > 0) {
        o_glUseProgram(g2.progAT); Common(g2.progAT);
        gx.Uniform1i(UL(g2.progAT, "u_src"), 1); gx.Uniform1i(UL(g2.progAT, "u_hm"), 2); gx.Uniform1i(UL(g2.progAT, "u_gb"), 3);
        gx.Uniform2f(UL(g2.progAT, "u_half"), (float)hw, (float)hh); gx.Uniform1f(UL(g2.progAT, "u_sigL"), g_giSigmaL);
        o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, g2.texHM[cur]);
        o_glActiveTexture(0x84C3); o_glBindTexture(0x0DE1, g2.texGB);
        GLuint src = g2.texHC[cur];
        for (int i = 0; i < nat; i++) {
            GLuint dst = g2.texPing[i & 1];
            o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, dst, 0);
            gx.Uniform1i(UL(g2.progAT, "u_step"), 1 << i); gx.Uniform1i(UL(g2.progAT, "u_first"), i == 0 ? 1 : 0);
            o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, src);
            o_glDrawArrays(4, 0, 3);
            src = dst;
        }
        finalTex = src;
    }
    GIMark(4);
    // 4) composite (additive) onto FB0
    o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
    o_glUseProgram(g2.progComp); Common(g2.progComp);
    SetChars(g2.progComp);
    gx.Uniform1i(UL(g2.progComp, "u_gi"), 1); gx.Uniform1i(UL(g2.progComp, "u_gb"), 2); gx.Uniform1i(UL(g2.progComp, "u_hz"), 3); gx.Uniform1i(UL(g2.progComp, "u_color"), 4);
    gx.Uniform2f(UL(g2.progComp, "u_half"), (float)hw, (float)hh); gx.Uniform1f(UL(g2.progComp, "u_strength"), g_giStrength); gx.Uniform1i(UL(g2.progComp, "u_debug"), g_giDebug);
    o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, finalTex);
    o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, g2.texGB);
    o_glActiveTexture(0x84C3); o_glBindTexture(0x0DE1, g2.texHz);
    o_glActiveTexture(0x84C4); o_glBindTexture(0x0DE1, g_colorCopyTex);
    if (!g_giDebug) { o_glEnable(0x0BE2); o_glBlendFunc(1, 1); }
    o_glDrawArrays(4, 0, 3);
    o_glDisable(0x0BE2);
    GIMark(5);
    static bool once; if (!once) { once = true; logf("[gi] Hi-Z path first run ok (rays=%d iters=%d atrous=%d)", g_giRays, g_giHizIter > 0 ? g_giHizIter : g_giSteps * 4, nat); }
    return true;
}
