// ao.inl — GTAO pass injected after the engine copies scene depth (glCopyTexImage2D GL_DEPTH_COMPONENT).
// Included by tyrender.cpp. See RESEARCH\sessions\2026-10-11_session3\findings\F3_frame_anatomy_gameplay.md for why this hook point.
// Pipeline: [half-res GTAO -> RG16F (ao, viewZ)] -> [half-res 5x5 bilateral blur] -> [full-res bilateral upsample + multi-bounce, multiplied onto FB0]
//           -> re-copy colour into the engine's capture texture so bloom/brightness see the AO.
// Runtime switches (files in <game>\tyrender\, polled every 8 frames): ao_off (disable), ao_debug containing 1 = AO only, 2 = normals, 3 = view depth.
// Config tyrender.ini [ao]: Enable=1 Radius=1.5 Strength=1.0 Power=1.5 Slices=3 Steps=6 ZMode=0 (0: ndcZ=d*2-1 (GL), 1: ndcZ=d)

typedef GLuint(WINAPI* glCreateProgram_t)();
typedef void(WINAPI* glCompileShader_t)(GLuint);
typedef void(WINAPI* glGetShaderiv_t)(GLuint, GLenum, GLint*);
typedef void(WINAPI* glGetShaderInfoLog_t)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void(WINAPI* glGetProgramiv_t)(GLuint, GLenum, GLint*);
typedef void(WINAPI* glGetProgramInfoLog_t)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void(WINAPI* glUniform1i_t)(GLint, GLint);
typedef void(WINAPI* glUniform1f_t)(GLint, GLfloat);
typedef void(WINAPI* glUniform2f_t)(GLint, GLfloat, GLfloat);
typedef void(WINAPI* glUniform4f_t)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void(WINAPI* glGenFramebuffers_t)(GLsizei, GLuint*);
typedef GLenum(WINAPI* glCheckFramebufferStatus_t)(GLenum);
typedef void(WINAPI* glGenVertexArrays_t)(GLsizei, GLuint*);
typedef void(WINAPI* glBindVertexArray_t)(GLuint);
typedef void(WINAPI* glGenTextures_t)(GLsizei, GLuint*);
typedef void(WINAPI* glTexParameteri_t)(GLenum, GLenum, GLint);
typedef void(WINAPI* glGetBooleanv_t)(GLenum, GLboolean*);
typedef GLboolean(WINAPI* glIsEnabled_t)(GLenum);
typedef void(WINAPI* glDepthMask_t2)(GLboolean);

static struct {
    bool tried, ok;
    glCreateProgram_t CreateProgram; glCompileShader_t CompileShader; glGetShaderiv_t GetShaderiv; glGetShaderInfoLog_t GetShaderInfoLog;
    glGetProgramiv_t GetProgramiv; glGetProgramInfoLog_t GetProgramInfoLog; glUniform1i_t Uniform1i; glUniform1f_t Uniform1f; glUniform2f_t Uniform2f; glUniform4f_t Uniform4f;
    glGenFramebuffers_t GenFramebuffers; glCheckFramebufferStatus_t CheckFramebufferStatus; glGenVertexArrays_t GenVertexArrays; glBindVertexArray_t BindVertexArray;
    glGenTextures_t GenTextures; glTexParameteri_t TexParameteri; glGetIntegerv_t GetIntegerv; glGetBooleanv_t GetBooleanv; glIsEnabled_t IsEnabled;
    glDepthMask_t2 DepthMask;
} gx;

static void* GLProc(const char* n) {
    void* p = (void*)o_wglGetProcAddress(n);
    if (!p || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1) p = (void*)GetProcAddress(GetModuleHandleA("opengl32.dll"), n);
    return p;
}
static bool LoadGX() {
    if (gx.tried) return gx.ok;
    gx.tried = true;
#define L(f) gx.f = (decltype(gx.f))GLProc("gl" #f); if (!gx.f) { logf("[ao] missing gl" #f); return false; }
    L(CreateProgram) L(CompileShader) L(GetShaderiv) L(GetShaderInfoLog) L(GetProgramiv) L(GetProgramInfoLog) L(Uniform1i) L(Uniform1f) L(Uniform2f) L(Uniform4f)
    L(GenFramebuffers) L(CheckFramebufferStatus) L(GenVertexArrays) L(BindVertexArray) L(GenTextures) L(TexParameteri) L(GetIntegerv) L(GetBooleanv) L(IsEnabled) L(DepthMask)
#undef L
    if (!o_glCreateShader || !o_glShaderSource || !o_glAttachShader || !o_glLinkProgram || !o_glUseProgram || !o_glGetUniformLocation ||
        !o_glBindFramebuffer || !o_glFramebufferTexture2D || !o_glActiveTexture || !o_glUniformMatrix4fv) { logf("[ao] engine did not request a needed GL entry point"); return false; }
    gx.ok = true;
    return true;
}

// ---------------------------------------------------------------- scene camera tracking (VPMatrix uploads while FB0 is bound)
static float g_sceneVP[16]; static bool g_haveVP; static unsigned g_vpFrame;
// Returns true if this upload is the scene camera VP (FB0, uniform named VPMatrix, perspective). colMajor receives the matrix (unjittered).
static bool TrackVP(GLint loc, GLboolean transpose, const GLfloat* v, float* colMajor) {
    if (g_curFBO != 0 || !g_vpLocs.count(((uint64_t)g_curProg << 32) | (uint32_t)loc)) return false;
    float m[16];
    if (transpose) { for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) m[c * 4 + r] = v[r * 4 + c]; } else memcpy(m, v, sizeof m);
    // perspective test: row 3 (w row) of a perspective VP has non-zero xyz; ortho UI matrices have (0,0,0,1)
    float w2 = m[3] * m[3] + m[7] * m[7] + m[11] * m[11];
    if (w2 < 0.25f) return false;
    memcpy(g_sceneVP, m, sizeof m); g_haveVP = true; g_vpFrame = g_frame;
    memcpy(colMajor, m, sizeof m);
    return true;
}

static bool Invert4(const float* m, float* o) {  // column-major 4x4 inverse (MESA gluInvertMatrix)
    float inv[16];
    inv[0] = m[5]*m[10]*m[15]-m[5]*m[11]*m[14]-m[9]*m[6]*m[15]+m[9]*m[7]*m[14]+m[13]*m[6]*m[11]-m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15]+m[4]*m[11]*m[14]+m[8]*m[6]*m[15]-m[8]*m[7]*m[14]-m[12]*m[6]*m[11]+m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15]-m[4]*m[11]*m[13]-m[8]*m[5]*m[15]+m[8]*m[7]*m[13]+m[12]*m[5]*m[11]-m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]+m[4]*m[10]*m[13]+m[8]*m[5]*m[14]-m[8]*m[6]*m[13]-m[12]*m[5]*m[10]+m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15]+m[1]*m[11]*m[14]+m[9]*m[2]*m[15]-m[9]*m[3]*m[14]-m[13]*m[2]*m[11]+m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15]-m[0]*m[11]*m[14]-m[8]*m[2]*m[15]+m[8]*m[3]*m[14]+m[12]*m[2]*m[11]-m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15]+m[0]*m[11]*m[13]+m[8]*m[1]*m[15]-m[8]*m[3]*m[13]-m[12]*m[1]*m[11]+m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14]-m[0]*m[10]*m[13]-m[8]*m[1]*m[14]+m[8]*m[2]*m[13]+m[12]*m[1]*m[10]-m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15]-m[1]*m[7]*m[14]-m[5]*m[2]*m[15]+m[5]*m[3]*m[14]+m[13]*m[2]*m[7]-m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15]+m[0]*m[7]*m[14]+m[4]*m[2]*m[15]-m[4]*m[3]*m[14]-m[12]*m[2]*m[7]+m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15]-m[0]*m[7]*m[13]-m[4]*m[1]*m[15]+m[4]*m[3]*m[13]+m[12]*m[1]*m[7]-m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]+m[0]*m[6]*m[13]+m[4]*m[1]*m[14]-m[4]*m[2]*m[13]-m[12]*m[1]*m[6]+m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11]+m[1]*m[7]*m[10]+m[5]*m[2]*m[11]-m[5]*m[3]*m[10]-m[9]*m[2]*m[7]+m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11]-m[0]*m[7]*m[10]-m[4]*m[2]*m[11]+m[4]*m[3]*m[10]+m[8]*m[2]*m[7]-m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]+m[0]*m[7]*m[9]+m[4]*m[1]*m[11]-m[4]*m[3]*m[9]-m[8]*m[1]*m[7]+m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10]-m[0]*m[6]*m[9]-m[4]*m[1]*m[10]+m[4]*m[2]*m[9]+m[8]*m[1]*m[6]-m[8]*m[2]*m[5];
    float det = m[0]*inv[0]+m[1]*inv[4]+m[2]*inv[8]+m[3]*inv[12];
    if (det == 0) return false;
    for (int i = 0; i < 16; i++) o[i] = inv[i] / det;
    return true;
}

// ---------------------------------------------------------------- shaders
static const char* kVS = R"(#version 330
out vec2 uv;
void main() { vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }
)";

// common: world/view reconstruction from the engine's depth texture
static const char* kCommon = R"(
uniform sampler2D u_depth;
uniform mat4 u_invVP;     // NDC -> world
uniform mat4 u_view;      // world -> view (right-handed, camera looks down -Z)
uniform vec2 u_fullSize;  // full-res pixels
uniform int  u_zMode;
vec3 WorldPos(vec2 uv) {
  float d = texture(u_depth, uv).r;
  float z = (u_zMode == 0) ? d * 2.0 - 1.0 : d;
  vec4 w = u_invVP * vec4(uv * 2.0 - 1.0, z, 1.0);
  return w.xyz / w.w;
}
vec3 ViewPos(vec2 uv) { return (u_view * vec4(WorldPos(uv), 1.0)).xyz; }
// 5-tap normal: for each axis use the neighbour with the smaller depth step (rejects silhouettes)
vec3 ViewNormal(vec2 uv, vec3 P) {
  vec2 px = 1.0 / u_fullSize;
  vec3 l = ViewPos(uv - vec2(px.x, 0)), r = ViewPos(uv + vec2(px.x, 0));
  vec3 b = ViewPos(uv - vec2(0, px.y)), t = ViewPos(uv + vec2(0, px.y));
  vec3 dx = (abs(l.z - P.z) < abs(r.z - P.z)) ? P - l : r - P;
  vec3 dy = (abs(b.z - P.z) < abs(t.z - P.z)) ? P - b : t - P;
  return normalize(cross(dx, dy));
}
)";

static const char* kGTAO = R"(
in vec2 uv; out vec4 o;
uniform float u_radius; uniform float u_proj11; uniform int u_slices; uniform int u_steps; uniform int u_debug; uniform float u_frameNoise; uniform float u_thin;
const float PI = 3.14159265;
float IGN(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
void main() {
  float d = texture(u_depth, uv).r;
  if (d >= 0.99999) { o = vec4(1.0, -1e6, 0, 0); return; }        // sky
  vec3 P = ViewPos(uv);
  vec3 N = ViewNormal(uv, P);
  if (u_debug == 2) { o = vec4(N * 0.5 + 0.5, 1); return; }
  if (u_debug == 3) { o = vec4(fract(-P.z / 10.0), P.z, 0, 1); return; }
  vec3 V = normalize(-P);
  // screen radius in full-res pixels for a world radius at this distance
  float radPx = u_radius * u_proj11 * 0.5 * u_fullSize.y / max(-P.z, 0.01);
  radPx = min(radPx, 0.25 * u_fullSize.y);
  if (radPx < 1.5) { o = vec4(1.0, P.z, 0, 0); return; }
  // per-frame rotated noise so the temporal pass converges to more effective samples
  float n1 = fract(IGN(gl_FragCoord.xy) + u_frameNoise), n2 = fract(IGN(gl_FragCoord.xy + vec2(5.588238, 5.588238)) + u_frameNoise * 1.618);
  float vis = 0.0;
  for (int s = 0; s < u_slices; s++) {
    float phi = (float(s) + n1) * PI / float(u_slices);
    vec2 omega = vec2(cos(phi), sin(phi));
    vec3 dirV = vec3(omega, 0.0);
    vec3 ortho = dirV - dot(dirV, V) * V;
    vec3 axis = normalize(cross(ortho, V));
    vec3 projN = N - axis * dot(N, axis);
    float projLen = length(projN);
    float cosN = clamp(dot(projN, V) / max(projLen, 1e-4), -1.0, 1.0);
    float n = sign(dot(ortho, projN)) * acos(cosN);
    float low0 = cos(n + PI * 0.5), low1 = cos(n - PI * 0.5);
    float h0c = low0, h1c = low1;
    for (int j = 0; j < u_steps; j++) {
      float t = (float(j) + n2) / float(u_steps);
      vec2 off = omega * (t * t * radPx + 1.0) / u_fullSize;
      vec3 S0 = ViewPos(uv + off) - P, S1 = ViewPos(uv - off) - P;
      float l0 = length(S0), l1 = length(S1);
      float w0 = clamp((u_radius - l0) / (0.4 * u_radius), 0.0, 1.0);
      float w1 = clamp((u_radius - l1) / (0.4 * u_radius), 0.0, 1.0);
      // thin-occluder heuristic (Jimenez 2016): when a sample is below the current horizon, let the horizon fall back a bit,
      // so thin geometry (grass blades, ropes, leaves) does not act like a solid wall
      float s0 = mix(low0, dot(S0 / max(l0, 1e-4), V), w0), s1 = mix(low1, dot(S1 / max(l1, 1e-4), V), w1);
      h0c = (s0 > h0c) ? s0 : mix(h0c, s0, u_thin);
      h1c = (s1 > h1c) ? s1 : mix(h1c, s1, u_thin);
    }
    float h0 = -acos(clamp(h1c, -1.0, 1.0)), h1 = acos(clamp(h0c, -1.0, 1.0));
    h0 = n + clamp(h0 - n, -PI * 0.5, PI * 0.5);
    h1 = n + clamp(h1 - n, -PI * 0.5, PI * 0.5);
    float sn = sin(n);
    float a0 = (cosN + 2.0 * h0 * sn - cos(2.0 * h0 - n)) * 0.25;
    float a1 = (cosN + 2.0 * h1 * sn - cos(2.0 * h1 - n)) * 0.25;
    vis += projLen * (a0 + a1);
  }
  vis = clamp(vis / float(u_slices), 0.0, 1.0);
  o = vec4(vis, P.z, 0, 0);
}
)";

// ---- v2 (XeGTAO style): half-res linear view-depth MIP chain, Hilbert/R2 noise, depth-mip sampling
static const char* kPre0 = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_depth; uniform vec4 u_invRow3; uniform vec2 u_fullSize; uniform int u_zMode;
float LinZ(ivec2 p) {
  float d = texelFetch(u_depth, p, 0).r;
  if (d >= 0.99999) return 1e5;
  vec2 n = (vec2(p) + 0.5) / u_fullSize * 2.0 - 1.0;
  float z = (u_zMode == 0) ? d * 2.0 - 1.0 : d;
  return abs(1.0 / dot(u_invRow3, vec4(n, z, 1.0)));
}
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy) * 2;
  float a = LinZ(p), b = LinZ(p + ivec2(1, 0)), c = LinZ(p + ivec2(0, 1)), d = LinZ(p + ivec2(1, 1));
  float mn = min(min(a, b), min(c, d)), mx = max(max(a, b), max(c, d)), avg = 0.25 * (a + b + c + d);
  float r = avg;
  if (mx - mn > 0.05 * mn) {   // silhouette inside the quad: keep the real sample closest to the mean (no invented depths)
    r = a; float best = abs(a - avg);
    if (abs(b - avg) < best) { r = b; best = abs(b - avg); }
    if (abs(c - avg) < best) { r = c; best = abs(c - avg); }
    if (abs(d - avg) < best) { r = d; }
  }
  o = vec4(r, 0, 0, 0);
}
)";
static const char* kPreN = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform int u_lod; uniform float u_radius;
void main() {
  ivec2 sz = textureSize(u_src, u_lod) - 1;
  ivec2 p = ivec2(gl_FragCoord.xy) * 2;
  float z0 = texelFetch(u_src, min(p, sz), u_lod).r, z1 = texelFetch(u_src, min(p + ivec2(1, 0), sz), u_lod).r;
  float z2 = texelFetch(u_src, min(p + ivec2(0, 1), sz), u_lod).r, z3 = texelFetch(u_src, min(p + ivec2(1, 1), sz), u_lod).r;
  float mx = max(max(z0, z1), max(z2, z3)), fr = 1.0 / (0.3 * u_radius);
  float w0 = clamp(1.0 - (mx - z0) * fr, 0.0, 1.0), w1 = clamp(1.0 - (mx - z1) * fr, 0.0, 1.0), w2 = clamp(1.0 - (mx - z2) * fr, 0.0, 1.0), w3 = clamp(1.0 - (mx - z3) * fr, 0.0, 1.0);
  o = vec4((z0 * w0 + z1 * w1 + z2 * w2 + z3 * w3) / (w0 + w1 + w2 + w3), 0, 0, 0);   // weighted towards the far sample (less self-occlusion on silhouettes)
}
)";
static const char* kGTAO2 = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_dmip; uniform vec2 u_halfSize; uniform float u_radius; uniform float u_p00; uniform float u_p11; uniform int u_slices; uniform int u_steps;
uniform int u_debug; uniform int u_frameIdx; uniform float u_thin; uniform float u_mipOff; uniform float u_radius2; uniform float u_wide; uniform int u_wslices;
uniform int u_nCap; uniform vec4 u_cap[16];   // character capsules, view space: [2i] = (base, radius), [2i+1] = (top, 0)
uniform float u_lodS; uniform float u_lodE; uniform float u_lodMin;   // [lod] AO ramp in view depth (parked when off: exact u_steps)
const float PI = 3.14159265;
vec3 VP(vec2 puv, float z) { vec2 n = puv * 2.0 - 1.0; return vec3(n.x * z / u_p00, n.y * z / u_p11, -z); }
// true if a view-space point is inside a character capsule: characters are never AO occluders (no dark halo behind Ty)
bool CapHit(vec3 X) {
  for (int i = 0; i < u_nCap; i++) {
    vec3 a = u_cap[2 * i].xyz, ab = u_cap[2 * i + 1].xyz - a;
    float t = clamp(dot(X - a, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
    if (length(X - (a + ab * t)) < u_cap[2 * i].w) return true;
  }
  return false;
}
uint Hilbert(uint x, uint y) {
  uint idx = 0u;
  for (uint cur = 32u; cur > 0u; cur >>= 1) {
    uint rx = ((x & cur) > 0u) ? 1u : 0u, ry = ((y & cur) > 0u) ? 1u : 0u;
    idx += cur * cur * ((3u * rx) ^ ry);
    if (ry == 0u) { if (rx == 1u) { x = 63u - x; y = 63u - y; } uint t = x; x = y; y = t; }
  }
  return idx;
}
float Occ(vec3 P, vec3 V, vec3 N, vec2 nz, float radius, float radPx, int steps, int slices) {
  vec2 px = 1.0 / u_halfSize;
  float vis = 0.0, invR = 1.0 / (0.4 * radius);
  for (int s = 0; s < slices; s++) {
    float phi = (float(s) + nz.x) * PI / float(slices);
    vec2 omega = vec2(cos(phi), sin(phi));
    vec3 dirV = vec3(omega, 0.0);
    vec3 ortho = dirV - dot(dirV, V) * V;
    vec3 axis = normalize(cross(ortho, V));
    vec3 projN = N - axis * dot(N, axis);
    float projLen = length(projN);
    float cosN = clamp(dot(projN, V) / max(projLen, 1e-4), -1.0, 1.0);
    float n = sign(dot(ortho, projN)) * acos(cosN);
    float low0 = cos(n + PI * 0.5), low1 = cos(n - PI * 0.5);
    float h0c = low0, h1c = low1;
    for (int j = 0; j < steps; j++) {
      float tt = (float(j) + nz.y) / float(steps);
      float rr = tt * tt * radPx + 1.0;
      vec2 offPx = floor(omega * rr + 0.5);
      float ml = clamp(log2(rr) - u_mipOff, 0.0, 4.0);
      vec2 o2 = offPx * px;
      vec2 u0 = uv + o2, u1 = uv - o2;
      vec3 A0 = VP(u0, textureLod(u_dmip, u0, ml).r), A1 = VP(u1, textureLod(u_dmip, u1, ml).r);
      vec3 S0 = A0 - P, S1 = A1 - P;
      float l0 = length(S0), l1 = length(S1);
      float w0 = clamp((radius - l0) * invR, 0.0, 1.0), w1 = clamp((radius - l1) * invR, 0.0, 1.0);
      // a sample inside a character capsule does not occlude (skipped, so the horizon is untouched)
      if (!CapHit(A0)) { float s0 = mix(low0, dot(S0, V) / max(l0, 1e-4), w0); h0c = (s0 > h0c) ? s0 : mix(h0c, s0, u_thin); }
      if (!CapHit(A1)) { float s1 = mix(low1, dot(S1, V) / max(l1, 1e-4), w1); h1c = (s1 > h1c) ? s1 : mix(h1c, s1, u_thin); }
    }
    float h0 = -acos(clamp(h1c, -1.0, 1.0)), h1 = acos(clamp(h0c, -1.0, 1.0));
    h0 = n + clamp(h0 - n, -PI * 0.5, PI * 0.5);
    h1 = n + clamp(h1 - n, -PI * 0.5, PI * 0.5);
    float sn = sin(n);
    float a0 = (cosN + 2.0 * h0 * sn - cos(2.0 * h0 - n)) * 0.25;
    float a1 = (cosN + 2.0 * h1 * sn - cos(2.0 * h1 - n)) * 0.25;
    vis += projLen * (a0 + a1);
  }
  return clamp(vis / float(slices), 0.0, 1.0);
}
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  float zc = texelFetch(u_dmip, p, 0).r;
  if (zc >= 9e4) { o = vec4(1.0, -1e6, 0, 0); return; }
  vec2 px = 1.0 / u_halfSize;
  vec3 P = VP(uv, zc);
  ivec2 hs = ivec2(u_halfSize) - 1;
  vec3 l = VP(uv - vec2(px.x, 0), texelFetch(u_dmip, max(p - ivec2(1, 0), 0), 0).r), r = VP(uv + vec2(px.x, 0), texelFetch(u_dmip, min(p + ivec2(1, 0), hs), 0).r);
  vec3 b = VP(uv - vec2(0, px.y), texelFetch(u_dmip, max(p - ivec2(0, 1), 0), 0).r), t = VP(uv + vec2(0, px.y), texelFetch(u_dmip, min(p + ivec2(0, 1), hs), 0).r);
  vec3 dx = (abs(l.z - P.z) < abs(r.z - P.z)) ? P - l : r - P;
  vec3 dy = (abs(b.z - P.z) < abs(t.z - P.z)) ? P - b : t - P;
  vec3 N = normalize(cross(dx, dy));
  if (u_debug == 2) { o = vec4(N * 0.5 + 0.5, 1); return; }
  if (u_debug == 3) { o = vec4(fract(zc / 10.0), -zc, 0, 1); return; }
  vec3 V = normalize(-P);
  float radPx = u_radius * u_p11 * 0.5 * u_halfSize.y / zc;      // half-res pixels
  radPx = min(radPx, 0.25 * u_halfSize.y);
  if (radPx < 0.75) { o = vec4(1.0, P.z, 0, 0); return; }
  uint hi = Hilbert(uint(p.x) & 63u, uint(p.y) & 63u) + 288u * uint(u_frameIdx & 63);
  vec2 nz = fract(0.5 + float(hi) * vec2(0.75487766624669276, 0.56984029099805327));   // R2 sequence over the Hilbert index: spatially + temporally stratified
  // [lod] distance LOD: fewer horizon steps per slice with distance. Stochastic rounding per pixel (dithered; the temporal pass averages it), off = exactly u_steps
  float lkA = 1.0 - (1.0 - u_lodMin) * smoothstep(u_lodS, u_lodE, zc);
  int stepsL = max(1, int(floor(float(u_steps) * lkA + fract(float(hi) * 0.41421356 + 0.5))));
  float vis = Occ(P, V, N, nz, u_radius, radPx, stepsL, u_slices);
  if (u_wide > 0.0) {                                                    // large-scale occlusion (corners, under overhangs): second, wide horizon search over the coarse depth mips
    float rp2 = min(u_radius2 * u_p11 * 0.5 * u_halfSize.y / zc, 0.25 * u_halfSize.y);
    if (rp2 > 2.0) vis *= mix(1.0, Occ(P, V, N, fract(nz + 0.37), u_radius2, rp2, max(stepsL - 2, 3), u_wslices), u_wide);
  }
  o = vec4(vis, P.z, 0, 0);
}
)";

// temporal accumulation (half res): reproject history with last frame's VP, reject on camera-distance mismatch, clamp to 3x3 neighbourhood
static const char* kTemporal = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_cur; uniform sampler2D u_hist; uniform mat4 u_prevVP; uniform vec3 u_cam; uniform vec3 u_prevCam; uniform vec2 u_halfSize; uniform float u_blend; uniform int u_histValid; uniform int u_tmode;
void main() {
  vec4 c = texture(u_cur, uv);
  if (c.g < -1e5) { o = vec4(1.0, c.g, -1.0, 0); return; }
  vec3 W = WorldPos(uv);
  float dist = distance(W, u_cam);
  float res = c.r; float outLen = 0.0;
  if (u_tmode == 1) {   // v2: bilinear depth-validated history, per-pixel history length (1/n blend up to u_blend), variance clamp
    float hsum = 0.0, wsum = 0.0, lsum = 0.0;
    if (u_histValid != 0) {
      vec4 pc = u_prevVP * vec4(W, 1.0);
      if (pc.w > 0.0) {
        vec2 puv = pc.xy / pc.w * 0.5 + 0.5, hp = puv * u_halfSize - 0.5, f = fract(hp); ivec2 b = ivec2(floor(hp));
        float expect = distance(W, u_prevCam);
        for (int i = 0; i < 4; i++) {
          ivec2 o2 = ivec2(i & 1, i >> 1), q = b + o2;
          if (any(lessThan(q, ivec2(0))) || any(greaterThanEqual(q, ivec2(u_halfSize)))) continue;
          vec4 h = texelFetch(u_hist, q, 0);
          if (h.b <= 0.0 || abs(h.b - expect) > 0.03 * expect + 2.0) continue;
          float w = (o2.x == 1 ? f.x : 1.0 - f.x) * (o2.y == 1 ? f.y : 1.0 - f.y);
          hsum += h.r * w; lsum += h.a * w; wsum += w;
        }
      }
    }
    if (wsum > 0.05) {
      hsum /= wsum; lsum /= wsum;
      float m1 = 0.0, m2 = 0.0;
      for (int y = -2; y <= 2; y++) for (int x = -2; x <= 2; x++) { float s = texture(u_cur, uv + vec2(x, y) / u_halfSize).r; m1 += s; m2 += s * s; }
      m1 /= 25.0; float sd = sqrt(max(m2 / 25.0 - m1 * m1, 0.0));
      // 3x3 neighbourhood clamp before the blend (on top of the 5x5 variance box): stale history cannot pull the pixel outside its current neighbours
      float n3lo = 1e9, n3hi = -1e9;
      for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) { float s = texture(u_cur, uv + vec2(x, y) / u_halfSize).r; n3lo = min(n3lo, s); n3hi = max(n3hi, s); }
      float hc = clamp(clamp(hsum, n3lo - 0.03, n3hi + 0.03), m1 - 2.0 * sd - 0.03, m1 + 2.0 * sd + 0.03);
      float len = lsum * (hc == hsum ? 1.0 : 0.5);
      float alpha = max(1.0 / (len + 1.0), 1.0 - u_blend);
      res = mix(hc, c.r, alpha); outLen = min(len + 1.0, 32.0);
    } else outLen = 1.0;
    o = vec4(res, c.g, dist, outLen); return;
  }
  if (u_histValid != 0) {
    vec4 pc = u_prevVP * vec4(W, 1.0);
    vec2 puv = pc.xy / pc.w * 0.5 + 0.5;
    if (pc.w > 0.0 && all(greaterThan(puv, vec2(0.0))) && all(lessThan(puv, vec2(1.0)))) {
      vec4 h = texture(u_hist, puv);
      float expect = distance(W, u_prevCam);
      if (h.b > 0.0 && abs(h.b - expect) < 0.03 * expect + 2.0) {
        float mn = 1.0, mx = 0.0;
        for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) { float s = texture(u_cur, uv + vec2(x, y) / u_halfSize).r; mn = min(mn, s); mx = max(mx, s); }
        res = mix(c.r, clamp(h.r, mn - 0.01, mx + 0.01), u_blend);
      }
    }
  }
  o = vec4(res, c.g, dist, 0);
}
)";

static const char* kBlur = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform vec2 u_halfSize; uniform int u_blurR; uniform float u_blurK;
void main() {
  vec4 c = texture(u_src, uv);
  if (c.g < -1e5) { o = c; return; }
  float sum = 0.0, wsum = 0.0;
  for (int y = -u_blurR; y <= u_blurR; y++) for (int x = -u_blurR; x <= u_blurR; x++) {
    vec4 s = texture(u_src, uv + vec2(x, y) / u_halfSize);
    float dz = abs(s.g - c.g) / max(abs(c.g), 0.1);
    float w = exp(-dz * dz * 400.0) * exp(-float(x * x + y * y) * u_blurK);
    sum += s.r * w; wsum += w;
  }
  o = vec4(sum / max(wsum, 1e-4), c.g, 0, 0);
}
)";

static const char* kComposite = R"(
in vec2 uv; out vec4 o;
uniform sampler2D u_ao; uniform sampler2D u_color; uniform vec2 u_halfSize; uniform float u_strength; uniform float u_power; uniform int u_debug; uniform float u_mbScale;
uniform vec3 u_cam; uniform vec4 u_fogC; uniform float u_atmD; uniform float u_atmF; uniform float u_fogFade; uniform float u_fogPow; uniform float u_fadeS; uniform float u_fadeE;
void main() {
  if (u_debug == 2) { o = texture(u_ao, uv); return; }          // normals were written straight to the AO target
  if (u_debug == 3) { o = vec4(vec3(texture(u_ao, uv).r), 1); return; }
  float d = texture(u_depth, uv).r;
  float ao = 1.0;
  if (d < 0.99999) {
    float z = ViewPos(uv).z;
    // bilateral upsample from the 4 nearest half-res texels
    vec2 hp = uv * u_halfSize - 0.5, f = fract(hp), base = (floor(hp) + 0.5) / u_halfSize;
    float sum = 0.0, wsum = 0.0;
    for (int i = 0; i < 4; i++) {
      vec2 o2 = vec2(i & 1, i >> 1);
      vec4 s = texture(u_ao, base + o2 / u_halfSize);
      float bw = mix(1.0 - f.x, f.x, o2.x) * mix(1.0 - f.y, f.y, o2.y);
      float dz = abs(s.g - z) / max(abs(z), 0.1);
      float w = bw * exp(-dz * dz * 400.0) + 1e-4;
      sum += s.r * w; wsum += w;
    }
    ao = pow(clamp(sum / wsum, 0.0, 1.0), u_power);
    if (u_fadeE > 0.0) ao = mix(ao, 1.0, smoothstep(u_fadeS, u_fadeE, -z));   // distance fade: AO goes to 1 (no darkening) between FadeStart and FadeEnd view distance
    if (u_fogFade > 0.0) {   // AO acts only on the unfogged part of the pixel: engine fog fraction and the atmosphere's transmittance (same formulas as the fog passes)
      vec3 Wp = WorldPos(uv); float dist = distance(Wp, u_cam);
      float fe = u_fogC.x + clamp((-z - u_fogC.z) * u_fogC.w, 0.0, 1.0) * u_fogC.y;
      float kd = normalize(Wp - u_cam).y * u_atmF, tau = u_atmD * ((abs(kd) < 1e-6) ? dist : (1.0 - exp(-kd * dist)) / kd);
      float k = clamp(1.0 - fe, 0.0, 1.0) * exp(-clamp(tau, 0.0, 20.0));
      ao = mix(1.0, ao, mix(1.0, pow(k, u_fogPow), u_fogFade));   // FogFadePower > 1: AO fades out faster than the fog thickens (dark blobs on fogged trees)
    }
  }
  // multi-bounce (Jimenez 2016) with the lit colour as albedo proxy, so bright surfaces don't go grey
  vec3 rho = clamp(texture(u_color, uv).rgb * u_mbScale, 0.0, 1.0);   // lit colour overestimates albedo -> scale down
  vec3 a = 2.0404 * rho - 0.3324, b = -4.7951 * rho + 0.6417, c = 2.7552 * rho + 0.6903;
  vec3 g = max(vec3(ao), ((ao * a + b) * ao + c) * ao);
  vec3 m = mix(vec3(1.0), g, u_strength);
  if (u_debug == 1) { o = vec4(vec3(ao), 1); return; }
  o = vec4(m, 1.0);
}
)";

// [debug] ForceFallback bit mask (test aid for the fallback paths): 1 froxel atmosphere, 2 Hi-Z GI, 4 AO v2, 8 tessellation, 16 light shader rewrite
static int g_dbgForce = 0;
static GLuint CompileProg(const char* name, const char* vs, const char* fs1, const char* fs2, const char* fs3) {
    if ((g_dbgForce & 4) && (!strcmp(name, "pre0") || !strcmp(name, "preN") || !strcmp(name, "gtao2"))) { logf("[fallback] FORCED compile failure of AO v2 program %s", name); return 0; }
    if ((g_dbgForce & 2) && !strncmp(name, "gi_", 3)) { logf("[fallback] FORCED compile failure of GI program %s", name); return 0; }
    GLuint p = gx.CreateProgram();
    const char* fsParts[3] = {fs1, fs2, fs3}; int nfs = fs3 ? 3 : fs2 ? 2 : 1;
    GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    for (int k = 0; k < 2; k++) {
        GLuint s = o_glCreateShader(types[k]);
        if (k == 0) o_glShaderSource(s, 1, &vs, nullptr); else o_glShaderSource(s, nfs, fsParts, nullptr);
        gx.CompileShader(s);
        GLint ok = 0; gx.GetShaderiv(s, 0x8B81 /*COMPILE_STATUS*/, &ok);
        if (!ok) { char buf[4096]; gx.GetShaderInfoLog(s, sizeof buf, nullptr, buf); logf("[ao] %s %s compile FAILED:\n%s", name, k ? "fs" : "vs", buf); SessFailure("ao compile", buf); return 0; }
        o_glAttachShader(p, s);
    }
    o_glLinkProgram(p);
    GLint ok = 0; gx.GetProgramiv(p, 0x8B82 /*LINK_STATUS*/, &ok);
    if (!ok) { char buf[4096]; gx.GetProgramInfoLog(p, sizeof buf, nullptr, buf); logf("[ao] %s link FAILED:\n%s", name, buf); SessFailure("ao link", buf); return 0; }
    return p;
}

static void AtmosFogParams(float* dens0, float* falloff);   // defined in gpuprof.inl (reads atmos.inl globals)
// ---------------------------------------------------------------- resources
static struct {
    bool init, failed; int w, h;
    GLuint progAO, progBlur, progComp, progTemporal, vao, fbo, texA, texB, texH[2];
    GLuint progPre0, progPreN, progAO2, texD; bool v2ok;
    int histIdx; bool histValid; unsigned lastFrame; float prevVP[16], prevCam[3];
} ao;
static float g_aoRadius2 = 400.0f, g_aoWide = 1.0f; static float g_aoRadius = 60.0f, g_aoStrength = 0.85f, g_aoPower = 1.0f, g_aoBlend = 0.92f, g_aoMB = 0.6f, g_aoThin = 0.5f;
static int g_aoSlices = 3, g_aoSteps = 6, g_aoZMode = 0, g_aoEnable = 1, g_aoTemporal = 1;
static int g_aoWSlices = 0, g_aoNoiseFrames = 64, g_aoBlurR = 2; static float g_aoBlurK = 0.15f; static int g_aoTMode = 1; static float g_aoFogFade = 1.0f, g_aoFogPow = 2.0f; static bool g_aoOffFile; static int g_aoDebugIni; static float g_aoMipOff = 3.15f; static int g_aoLegacy = 0;
static float g_aoFadeS = 0.0f, g_aoFadeE = 0.0f;   // [ao] FadeStart / FadeEnd: view distance where AO fades out (FadeEnd 0 = off)
static float g_aoCharR = 40.0f, g_aoCharH = 120.0f; // [ao] CharRadius / CharHeight: character capsule for the AO occluder mask (CharRadius 0 = off)

// [lod] distance LOD (issue #10). Master Enable (default 0) plus one key per effect (GI, AO, Fog; default 0). An effect is reduced with distance
// only when Enable=1 and its own key is 1. Start/End: view depth in world units; MinScale: share of the work kept at End (smooth ramp, no hard step).
// Used by the GI trace (gi2.inl), the AO horizon search (kGTAO2) and the froxel fog (kAtmosCS). Loaded from LoadAOConfig, which runs at startup and on reload.
static int g_lodEnable = 0, g_lodGI = 0, g_lodAO = 0, g_lodFog = 0;
static float g_lodStart = 1500.0f, g_lodEnd = 6000.0f, g_lodMin = 0.35f;
static void LoadLodConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); char b[32];
    g_lodEnable = GetPrivateProfileIntA("lod", "Enable", 0, ini);
    g_lodGI = GetPrivateProfileIntA("lod", "GI", 0, ini); g_lodAO = GetPrivateProfileIntA("lod", "AO", 0, ini); g_lodFog = GetPrivateProfileIntA("lod", "Fog", 0, ini);
    GetPrivateProfileStringA("lod", "Start", "1500", b, sizeof b, ini); g_lodStart = (float)atof(b);
    GetPrivateProfileStringA("lod", "End", "6000", b, sizeof b, ini); g_lodEnd = (float)atof(b); if (g_lodEnd < g_lodStart + 1.0f) g_lodEnd = g_lodStart + 1.0f;
    GetPrivateProfileStringA("lod", "MinScale", "0.35", b, sizeof b, ini); g_lodMin = (float)atof(b); if (g_lodMin < 0.0f) g_lodMin = 0.0f; if (g_lodMin > 1.0f) g_lodMin = 1.0f;
}

static void LoadAOConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    char b[32];
    LoadLodConfig();
    g_dbgForce = GetPrivateProfileIntA("debug", "ForceFallback", 0, ini);
    g_aoEnable = GetPrivateProfileIntA("ao", "Enable", 1, ini);
    GetPrivateProfileStringA("ao", "WideRadius", "400", b, sizeof b, ini); g_aoRadius2 = (float)atof(b);
    GetPrivateProfileStringA("ao", "WideStrength", "1.0", b, sizeof b, ini); g_aoWide = (float)atof(b);
    GetPrivateProfileStringA("ao", "Radius", "60", b, sizeof b, ini); g_aoRadius = (float)atof(b);
    GetPrivateProfileStringA("ao", "Thin", "0.5", b, sizeof b, ini); g_aoThin = (float)atof(b);
    GetPrivateProfileStringA("ao", "TemporalBlend", "0.92", b, sizeof b, ini); g_aoBlend = (float)atof(b);
    GetPrivateProfileStringA("ao", "MultiBounce", "0.6", b, sizeof b, ini); g_aoMB = (float)atof(b);
    g_aoTemporal = GetPrivateProfileIntA("ao", "Temporal", 1, ini);
    GetPrivateProfileStringA("ao", "Strength", "0.85", b, sizeof b, ini); g_aoStrength = (float)atof(b);
    GetPrivateProfileStringA("ao", "Power", "1.0", b, sizeof b, ini); g_aoPower = (float)atof(b);
    g_aoSlices = GetPrivateProfileIntA("ao", "Slices", 3, ini); g_aoSteps = GetPrivateProfileIntA("ao", "Steps", 6, ini);
    g_aoZMode = GetPrivateProfileIntA("ao", "ZMode", 0, ini);
    g_aoExp = GetPrivateProfileIntA("ao", "Exp", 0, ini);
    GetPrivateProfileStringA("ao", "DepthMipOffset", "3.15", b, sizeof b, ini); g_aoMipOff = (float)atof(b);
    g_aoLegacy = GetPrivateProfileIntA("ao", "Legacy", 0, ini);
    GetPrivateProfileStringA("ao", "FadeStart", "0", b, sizeof b, ini); g_aoFadeS = (float)atof(b);
    GetPrivateProfileStringA("ao", "FadeEnd", "0", b, sizeof b, ini); g_aoFadeE = (float)atof(b);
    GetPrivateProfileStringA("ao", "CharRadius", "40", b, sizeof b, ini); g_aoCharR = (float)atof(b);
    GetPrivateProfileStringA("ao", "CharHeight", "120", b, sizeof b, ini); g_aoCharH = (float)atof(b);
    g_aoWSlices = GetPrivateProfileIntA("ao", "WideSlices", 0, ini);   // 0 = same as Slices
    g_aoNoiseFrames = GetPrivateProfileIntA("ao", "NoiseFrames", 64, ini); g_aoBlurR = GetPrivateProfileIntA("ao", "BlurRadius", 2, ini); if (g_aoBlurR < 1) g_aoBlurR = 1; if (g_aoBlurR > 5) g_aoBlurR = 5; GetPrivateProfileStringA("ao", "BlurFalloff", "0.15", b, sizeof b, ini); g_aoBlurK = (float)atof(b);
    g_aoTMode = GetPrivateProfileIntA("ao", "TemporalMode", 1, ini);   // 0 = v0.3.1 nearest-history accumulation, 1 = bilinear + history length + variance clamp
    GetPrivateProfileStringA("ao", "FogFade", "1.0", b, sizeof b, ini); g_aoFogFade = (float)atof(b); GetPrivateProfileStringA("ao", "FogFadePower", "2.0", b, sizeof b, ini); g_aoFogPow = fmaxf((float)atof(b), 0.1f);   // 0 = AO ignores fog (old behaviour), 1 = AO fades with fog transmittance
    g_aoDebugIni = GetPrivateProfileIntA("ao", "DebugView", 0, ini);   // test aid: 1 = AO-only view (live via ini reload)
    logf("[ao] config: debugview=%d tmode=%d blend=%.2f fogfade=%.2f wide=%.2f", g_aoDebugIni, g_aoTMode, g_aoBlend, g_aoFogFade, g_aoWide);
}
static void PollAOFlags() {
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%styrender\\ao_off", g_dir); g_aoOffFile = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    snprintf(p, sizeof p, "%styrender\\ao_debug", g_dir); std::string s; { int nd = ReadFileStr(p, s) ? atoi(s.c_str()) : g_aoDebugIni; if (nd != g_aoDebug) logf("[ao] debug view %d -> %d", g_aoDebug, nd); g_aoDebug = nd; }
    snprintf(p, sizeof p, "%styrender\\ao_reload", g_dir);
    if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) { DeleteFileA(p); LoadAOConfig(); logf("[ao] config reloaded: radius=%.2f strength=%.2f power=%.2f slices=%d steps=%d zmode=%d", g_aoRadius, g_aoStrength, g_aoPower, g_aoSlices, g_aoSteps, g_aoZMode); }
}

static GLuint MakeTex(int w, int h, GLint ifmt = 0x822F /*RG16F*/, GLenum fmt = 0x8227 /*RG*/) {
    GLuint t; gx.GenTextures(1, &t); o_glBindTexture(0x0DE1, t);
    o_glTexImage2D(0x0DE1, 0, ifmt, w, h, 0, fmt, 0x1406 /*FLOAT*/, nullptr);
    gx.TexParameteri(0x0DE1, 0x2801, 0x2600); gx.TexParameteri(0x0DE1, 0x2800, 0x2600);     // NEAREST (we filter manually)
    gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);     // CLAMP_TO_EDGE
    return t;
}
static bool InitAO(int w, int h) {
    if (ao.failed) return false;
    if (ao.init && ao.w == w && ao.h == h) return true;
    if (!LoadGX()) { ao.failed = true; return false; }
    if (!ao.init) {
        ao.progAO = CompileProg("gtao", kVS, "#version 330\n", kCommon, kGTAO);
        ao.progBlur = CompileProg("blur", kVS, kBlur, nullptr, nullptr);
        ao.progComp = CompileProg("composite", kVS, "#version 330\n", kCommon, kComposite);
        ao.progTemporal = CompileProg("temporal", kVS, "#version 330\n", kCommon, kTemporal);
        if (!ao.progAO || !ao.progBlur || !ao.progComp || !ao.progTemporal) { ao.failed = true; return false; }
        ao.progPre0 = CompileProg("pre0", kVS, kPre0, nullptr, nullptr); ao.progPreN = CompileProg("preN", kVS, kPreN, nullptr, nullptr);
        ao.progAO2 = CompileProg("gtao2", kVS, kGTAO2, nullptr, nullptr);
        ao.v2ok = ao.progPre0 && ao.progPreN && ao.progAO2;
        if (!ao.v2ok && !g_aoLegacy) { logf("[fallback] AO v2 unavailable -> AO OFF (no legacy fallback)"); SessEvent("AO turned off: v2 shader unavailable"); ao.failed = true; return false; }   // fallback policy: failure -> OFF
        if (!ao.v2ok) logf("[ao] v2 (depth-mip GTAO) unavailable, using legacy GTAO (Legacy=1)");
        gx.GenVertexArrays(1, &ao.vao); gx.GenFramebuffers(1, &ao.fbo);
    }
    ao.w = w; ao.h = h;
    ao.texA = MakeTex(w / 2, h / 2); ao.texB = MakeTex(w / 2, h / 2);   // (old textures leak on resize; rare)
    for (int i = 0; i < 2; i++) ao.texH[i] = MakeTex(w / 2, h / 2, 0x881A /*RGBA16F*/, 0x1908 /*RGBA*/);
    if (ao.v2ok) {   // linear view-depth, 5 mip levels (R32F), NEAREST_MIPMAP_NEAREST
        ao.texD = MakeTex(w / 2, h / 2, 0x822E /*R32F*/, 0x1903 /*RED*/);
        for (int l = 1; l <= 4; l++) o_glTexImage2D(0x0DE1, l, 0x822E, std::max(1, (w / 2) >> l), std::max(1, (h / 2) >> l), 0, 0x1903, 0x1406, nullptr);
        gx.TexParameteri(0x0DE1, 0x2801, 0x2700); gx.TexParameteri(0x0DE1, 0x813C, 0); gx.TexParameteri(0x0DE1, 0x813D, 4);
    }
    ao.histValid = false;
    ao.init = true;
    logf("[ao] initialised %dx%d (half %dx%d)", w, h, w / 2, h / 2);
    return true;
}

// ---------------------------------------------------------------- the pass
static inline void AODraw() { if (!(g_aoExp & 8)) o_glDrawArrays(4, 0, 3); }
static void RunAO(GLuint depthTex, int w, int h) {
    if (!g_aoEnable || g_aoOffFile || g_abOff || !g_haveVP || g_vpFrame != g_frame || !g_colorCopyTex) return;
    if (!InitAO(w, h)) return;
    float invVP[16]; if (!Invert4(g_sceneVP, invVP)) return;
    // camera basis from VP rows (VP = P*V, symmetric frustum): row0 ~ right*P00, row1 ~ up*P11, row3 = -back
    const float* m = g_sceneVP;
    float r0[3] = {m[0], m[4], m[8]}, r1[3] = {m[1], m[5], m[9]}, r3[3] = {m[3], m[7], m[11]};
    float p00 = sqrtf(r0[0]*r0[0] + r0[1]*r0[1] + r0[2]*r0[2]), p11 = sqrtf(r1[0]*r1[0] + r1[1]*r1[1] + r1[2]*r1[2]), l3 = sqrtf(r3[0]*r3[0] + r3[1]*r3[1] + r3[2]*r3[2]);
    float R[3] = {r0[0]/p00, r0[1]/p00, r0[2]/p00}, U[3] = {r1[0]/p11, r1[1]/p11, r1[2]/p11}, B[3] = {-r3[0]/l3, -r3[1]/l3, -r3[2]/l3};
    // camera position: the near-plane centre N = invVP*(0,0,zNear,1) lies nearDist in front of the camera; the point at NDC (0,1,zNear)
    // is nearDist/P11 above it  =>  nearDist = halfHeight * P11, cam = N + back * nearDist.
    float zn = g_aoZMode == 0 ? -1.0f : 0.0f;
    float nc[4], nt[4];
    for (int i = 0; i < 4; i++) { nc[i] = invVP[8 + i] * zn + invVP[12 + i]; nt[i] = invVP[4 + i] + nc[i]; }
    float N3[3] = {nc[0]/nc[3], nc[1]/nc[3], nc[2]/nc[3]}, NT[3] = {nt[0]/nt[3], nt[1]/nt[3], nt[2]/nt[3]};
    float halfH = sqrtf((NT[0]-N3[0])*(NT[0]-N3[0]) + (NT[1]-N3[1])*(NT[1]-N3[1]) + (NT[2]-N3[2])*(NT[2]-N3[2]));
    float nearDist = halfH * p11;
    float cam[3] = {N3[0] + B[0] * nearDist, N3[1] + B[1] * nearDist, N3[2] + B[2] * nearDist};
    float view[16] = { R[0], U[0], B[0], 0,  R[1], U[1], B[1], 0,  R[2], U[2], B[2], 0,
                       -(R[0]*cam[0] + R[1]*cam[1] + R[2]*cam[2]), -(U[0]*cam[0] + U[1]*cam[1] + U[2]*cam[2]), -(B[0]*cam[0] + B[1]*cam[1] + B[2]*cam[2]), 1 };

    // ---- save state
    GLint prevFBO, prevVP[4], prevUnit, prevTex[3], prevVAO, prevBlendSrc, prevBlendDst; GLboolean prevDepthMask, prevColorMask[4];
    gx.GetIntegerv(0x8CA6 /*FRAMEBUFFER_BINDING*/, &prevFBO); gx.GetIntegerv(0x0BA2, prevVP); gx.GetIntegerv(0x84E0 /*ACTIVE_TEXTURE*/, &prevUnit);
    for (int i = 0; i < 3; i++) { o_glActiveTexture(0x84C0 + i); gx.GetIntegerv(0x8069 /*TEXTURE_BINDING_2D*/, &prevTex[i]); }
    gx.GetIntegerv(0x85B5 /*VERTEX_ARRAY_BINDING*/, &prevVAO); gx.GetIntegerv(0x0BE1 /*BLEND_SRC*/, &prevBlendSrc); gx.GetIntegerv(0x0BE0 /*BLEND_DST*/, &prevBlendDst);
    gx.GetBooleanv(0x0B72 /*DEPTH_WRITEMASK*/, &prevDepthMask); gx.GetBooleanv(0x0C23 /*COLOR_WRITEMASK*/, prevColorMask);
    GLboolean eBlend = gx.IsEnabled(0x0BE2), eDepth = gx.IsEnabled(0x0B71), eCull = gx.IsEnabled(0x0B44), eScissor = gx.IsEnabled(0x0C11), eStencil = gx.IsEnabled(0x0B90);
    GLuint prevProg = g_curProg;

    o_glDisable(0x0BE2); o_glDisable(0x0B71); o_glDisable(0x0B44); o_glDisable(0x0C11); o_glDisable(0x0B90);
    o_glDepthMask(0); o_glColorMask(1, 1, 1, 1);
    gx.BindVertexArray(ao.vao);
    int hw = w / 2, hh = h / 2;
    auto U1i = [](GLuint p, const char* n, int v) { gx.Uniform1i(o_glGetUniformLocation(p, n), v); };
    auto U1f = [](GLuint p, const char* n, float v) { gx.Uniform1f(o_glGetUniformLocation(p, n), v); };
    auto U2f = [](GLuint p, const char* n, float a, float b) { gx.Uniform2f(o_glGetUniformLocation(p, n), a, b); };
    auto SetCommon = [&](GLuint p) {
        U1i(p, "u_depth", 0); o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_invVP"), 1, 0, invVP);
        o_glUniformMatrix4fv(o_glGetUniformLocation(p, "u_view"), 1, 0, view); U2f(p, "u_fullSize", (float)w, (float)h); U1i(p, "u_zMode", g_aoZMode);
    };
    // 1) GTAO -> texA (half res)
    o_glBindFramebuffer(0x8D40, ao.fbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, ao.texA, 0); o_glViewport(0, 0, hw, hh);
    if (ao.v2ok && !g_aoLegacy) {
        typedef void(WINAPI* glUniform4fv_t)(GLint, GLsizei, const GLfloat*);
        static glUniform4fv_t u4v = (glUniform4fv_t)GLProc("glUniform4fv");
        // (a) full-res depth -> half-res linear view depth, mip 0
        float row3[4] = {invVP[3], invVP[7], invVP[11], invVP[15]};
        o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, ao.texD, 0);
        o_glUseProgram(ao.progPre0); U1i(ao.progPre0, "u_depth", 0); U2f(ao.progPre0, "u_fullSize", (float)w, (float)h); U1i(ao.progPre0, "u_zMode", g_aoZMode);
        u4v(o_glGetUniformLocation(ao.progPre0, "u_invRow3"), 1, row3);
        GProf(10); AODraw();
        // (b) mips 1..4 (weighted towards far samples); sample only level k-1 while rendering level k
        o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, ao.texD);
        o_glUseProgram(ao.progPreN); U1i(ao.progPreN, "u_src", 1); U1f(ao.progPreN, "u_radius", g_aoRadius);
        for (int l = 1; l <= 4; l++) {
            gx.TexParameteri(0x0DE1, 0x813C, l - 1); gx.TexParameteri(0x0DE1, 0x813D, l - 1);
            o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, ao.texD, l); o_glViewport(0, 0, std::max(1, hw >> l), std::max(1, hh >> l));
            U1i(ao.progPreN, "u_lod", 0);   // BASE_LEVEL=l-1 makes the fetch lod relative to the base
            AODraw();
        }
        gx.TexParameteri(0x0DE1, 0x813C, 0); gx.TexParameteri(0x0DE1, 0x813D, 4);
        GProf(11); GProf(16);
        // (c) GTAO on the depth chain -> texA
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, ao.texA, 0); o_glViewport(0, 0, hw, hh);
        // character capsules in view space (same transform as gi2.inl); used only when CharRadius > 0 and characters were drawn this frame
        float capd[64] = {}; int nCap = (g_aoCharR > 0.0f && g_charsFrame == g_frame) ? std::min(g_nChars, 8) : 0;
        for (int i = 0; i < nCap; i++) {
            const float* c = g_chars[i];
            for (int r = 0; r < 3; r++) {
                float base = view[r] * c[0] + view[4 + r] * c[1] + view[8 + r] * c[2] + view[12 + r];
                capd[8 * i + r] = base;
                capd[8 * i + 4 + r] = base + view[4 + r] * g_aoCharH;   // character up = world +Y through the view rotation
            }
            capd[8 * i + 3] = g_aoCharR;
        }
        o_glUseProgram(ao.progAO2);
        U1i(ao.progAO2, "u_nCap", nCap); u4v(o_glGetUniformLocation(ao.progAO2, "u_cap"), 16, capd);
        U1i(ao.progAO2, "u_dmip", 1); U2f(ao.progAO2, "u_halfSize", (float)hw, (float)hh);
        U1f(ao.progAO2, "u_radius", g_aoRadius); U1f(ao.progAO2, "u_p00", p00); U1f(ao.progAO2, "u_p11", p11);
        U1i(ao.progAO2, "u_slices", g_aoSlices); U1i(ao.progAO2, "u_steps", g_aoSteps); U1i(ao.progAO2, "u_debug", g_aoDebug);
        { bool lodA = g_lodEnable && g_lodAO; U1f(ao.progAO2, "u_lodS", lodA ? g_lodStart : 1e9f); U1f(ao.progAO2, "u_lodE", lodA ? g_lodEnd : 2e9f); U1f(ao.progAO2, "u_lodMin", lodA ? g_lodMin : 1.0f); }
        // noise steps every 8 frames (not every frame), so the temporal pass sees a stable pattern (less shimmer and grain)
        U1i(ao.progAO2, "u_frameIdx", g_aoTemporal ? (int)((g_frame / 8u) % (unsigned)(g_aoNoiseFrames < 1 ? 1 : g_aoNoiseFrames)) : 0); U1f(ao.progAO2, "u_thin", g_aoThin); U1f(ao.progAO2, "u_mipOff", g_aoMipOff); U1f(ao.progAO2, "u_radius2", g_aoRadius2); U1f(ao.progAO2, "u_wide", g_aoWide); U1i(ao.progAO2, "u_wslices", g_aoWSlices > 0 ? g_aoWSlices : g_aoSlices);
        AODraw(); GProf(17);
        o_glActiveTexture(0x84C0);
    } else {
    o_glUseProgram(ao.progAO); SetCommon(ao.progAO);
    U1f(ao.progAO, "u_radius", g_aoRadius); U1f(ao.progAO, "u_proj11", p11); U1i(ao.progAO, "u_slices", g_aoSlices); U1i(ao.progAO, "u_steps", g_aoSteps); U1i(ao.progAO, "u_debug", g_aoDebug);
    U1f(ao.progAO, "u_thin", g_aoThin);
    // slow noise cycle: the rotation steps every 8 frames instead of every frame, so the temporal pass sees a stable pattern (less shimmer and grain)
    U1f(ao.progAO, "u_frameNoise", g_aoTemporal ? fmodf(floorf(g_frame / 8.0f) * 0.6180339f, 1.0f) : 0.0f);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
    GProf(10); AODraw(); GProf(11);
    }
    GProf(12);
    bool dbgRaw = (g_aoDebug == 2 || g_aoDebug == 3);
    // 2) temporal: texA + history[prev] -> history[cur]
    GLuint blurSrc = ao.texA;
    if (g_aoTemporal && !dbgRaw) {
        int cur = ao.histIdx ^ 1;
        bool valid = ao.histValid && ao.lastFrame + 1 == g_frame;
        o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, ao.texH[cur], 0);
        o_glUseProgram(ao.progTemporal); SetCommon(ao.progTemporal);
        U1i(ao.progTemporal, "u_cur", 1); U1i(ao.progTemporal, "u_hist", 2); U2f(ao.progTemporal, "u_halfSize", (float)hw, (float)hh);
        U1i(ao.progTemporal, "u_tmode", g_aoTMode);
        U1f(ao.progTemporal, "u_blend", g_aoBlend); U1i(ao.progTemporal, "u_histValid", valid ? 1 : 0);
        o_glUniformMatrix4fv(o_glGetUniformLocation(ao.progTemporal, "u_prevVP"), 1, 0, ao.prevVP);
        typedef void(WINAPI* glUniform3f_t)(GLint, GLfloat, GLfloat, GLfloat);
        static glUniform3f_t u3 = (glUniform3f_t)GLProc("glUniform3f");
        u3(o_glGetUniformLocation(ao.progTemporal, "u_cam"), cam[0], cam[1], cam[2]);
        u3(o_glGetUniformLocation(ao.progTemporal, "u_prevCam"), ao.prevCam[0], ao.prevCam[1], ao.prevCam[2]);
        o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, ao.texA);
        o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, ao.texH[ao.histIdx]);
        AODraw();
        o_glActiveTexture(0x84C0);
        ao.histIdx = cur; ao.histValid = true; blurSrc = ao.texH[cur];
    }
    memcpy(ao.prevVP, g_sceneVP, sizeof ao.prevVP); memcpy(ao.prevCam, cam, sizeof ao.prevCam); ao.lastFrame = g_frame;
    // 3) blur -> texB
    o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, ao.texB, 0);
    o_glUseProgram(ao.progBlur); U1i(ao.progBlur, "u_src", 0); U1i(ao.progBlur, "u_blurR", g_aoBlurR); U1f(ao.progBlur, "u_blurK", g_aoBlurK); U2f(ao.progBlur, "u_halfSize", (float)hw, (float)hh);
    o_glBindTexture(0x0DE1, dbgRaw ? 0 : blurSrc);
    if (!dbgRaw) AODraw();
    GLuint aoResult = (g_aoDebug == 2 || g_aoDebug == 3) ? ao.texA : ao.texB;
    GProf(13); GProf(14);
    // 3) composite onto FB0 (multiply), or overwrite in debug views
    o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
    o_glUseProgram(ao.progComp); SetCommon(ao.progComp);
    U1i(ao.progComp, "u_ao", 1); U1i(ao.progComp, "u_color", 2); U2f(ao.progComp, "u_halfSize", (float)hw, (float)hh);
    { typedef void(WINAPI* glUniform3f_t)(GLint, GLfloat, GLfloat, GLfloat); static glUniform3f_t u3c = (glUniform3f_t)GLProc("glUniform3f");
      u3c(o_glGetUniformLocation(ao.progComp, "u_cam"), cam[0], cam[1], cam[2]);
      gx.Uniform4f(o_glGetUniformLocation(ao.progComp, "u_fogC"), g_fogC[0], g_fogC[1], g_fogC[2], g_fogC[3]);
      float ad, af; AtmosFogParams(&ad, &af); U1f(ao.progComp, "u_atmD", ad); U1f(ao.progComp, "u_atmF", af);
      U1f(ao.progComp, "u_fogPow", g_aoFogPow); U1f(ao.progComp, "u_fogFade", g_haveFogC ? g_aoFogFade : (ad > 0 ? g_aoFogFade : 0.0f)); }
    U1f(ao.progComp, "u_fadeS", g_aoFadeS); U1f(ao.progComp, "u_fadeE", g_aoFadeE);
    U1f(ao.progComp, "u_strength", g_aoStrength); U1f(ao.progComp, "u_power", g_aoPower); U1i(ao.progComp, "u_debug", g_aoDebug); U1f(ao.progComp, "u_mbScale", g_aoMB);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, depthTex);
    o_glActiveTexture(0x84C1); o_glBindTexture(0x0DE1, aoResult);
    o_glActiveTexture(0x84C2); o_glBindTexture(0x0DE1, g_colorCopyTex);
    if (!g_aoDebug) { o_glEnable(0x0BE2); o_glBlendFunc(0 /*ZERO*/, 0x0300 /*SRC_COLOR*/); }
    if (!(g_aoExp & 2)) AODraw();
    o_glDisable(0x0BE2);
    GProf(15);
    g_sceneModified = true;  // hook re-copies FB0 colour into the engine's capture texture after all scene passes

    // ---- restore state
    gx.BindVertexArray(prevVAO);
    for (int i = 0; i < 3; i++) { o_glActiveTexture(0x84C0 + i); o_glBindTexture(0x0DE1, prevTex[i]); }
    o_glActiveTexture(prevUnit);
    o_glBindFramebuffer(0x8D40, prevFBO); o_glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    o_glUseProgram(prevProg);
    o_glBlendFunc(prevBlendSrc, prevBlendDst);
    if (eBlend) o_glEnable(0x0BE2); if (eDepth) o_glEnable(0x0B71); if (eCull) o_glEnable(0x0B44); if (eScissor) o_glEnable(0x0C11); if (eStencil) o_glEnable(0x0B90);
    o_glDepthMask(prevDepthMask); o_glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
    { static int sc; if (sc++ == 300) { GLint sb = -1; gx.GetIntegerv(0x0D57, &sb); logf("[stencil] STENCIL_BITS=%d engine glEnable(STENCIL_TEST) calls so far=%u", sb, g_stencilEnables); } }
    static bool once; if (!once) { once = true; logf("[ao] first run ok: cam=(%.2f %.2f %.2f) near=%.3f p00=%.3f p11=%.3f depthTex=%u colorTex=%u", cam[0], cam[1], cam[2], nearDist, p00, p11, depthTex, g_colorCopyTex); }
}
