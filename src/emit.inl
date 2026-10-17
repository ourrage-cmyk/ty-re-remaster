// emit.inl - phase 2 emitter light injection (docs/MATERIAL_CLASSIFICATION_PLAN.md section 4; first user #9, the portal lights the ground).
// Emitter draws (policy column emissive: emitters, transparent emitters, emissive characters) are drawn once more, right after the normal draw,
// into a half-res emission buffer. Before each splat the scene depth is copied from FB0 and max-downsampled into the emission buffer's depth, so the
// splat is depth-tested against the scene (an emitter behind a wall does not light the other side). The splat outputs the albedo (the fragment colour
// captured before lighting, through the tyEmit uniform), not the lit colour. Once per frame, after GI and before atmos, the buffer is blurred and
// added to FB0 scaled by [emit] Strength.
// ini [emit]: Enable=0 (default: none of this runs), Strength=1.0 (composite gain), Radius=12 (blur step in half-res texels, all emitters share it),
//             MaxEmitters=16 (cap on splatted emitter draws per frame; the first N in draw order, NOT ranked by screen area or distance yet).
// Known limits: an emitter whose shader could not be rewritten (no tyEmit uniform) is not splatted. Lit emitters output their albedo captured before
// the light multiply, which is only the albedo when the stock light line is present.
#include <cstdio>

static int g_emitEnable = 0, g_emitMax = 16; static float g_emitStrength = 1.0f, g_emitRadius = 12.0f;
static GLuint g_emitFbo, g_emitVao, g_emitTex[2], g_emitProgBlur, g_emitProgComp, g_emitProgDepth; static int g_emitW, g_emitH; static bool g_emitInitFail;
static GLuint g_emitDepthFull, g_emitDepthHalf; static int g_emitFullW, g_emitFullH;   // full-res copy of the scene depth; half-res depth of the emission buffer
static unsigned g_emitFrame, g_emitCount, g_emitComposed; static bool g_emitCleared, g_emitActive;
static GLint g_progTyEmitLoc[8192];   // per program: tyEmit uniform location (-1 = shader not rewritten, no emission from it)
typedef void(WINAPI* emDrawBufs_t)(GLsizei, const GLenum*); typedef void(WINAPI* emClearBufv_t)(GLenum, GLint, const GLfloat*); typedef void(WINAPI* emDelTex_t)(GLsizei, const GLuint*);
typedef void(WINAPI* emDepthFunc_t)(GLenum); typedef void(WINAPI* emUniform1f_t)(GLint, GLfloat);
static emDrawBufs_t emDrawBufs; static emClearBufv_t emClearBufv; static emDelTex_t emDelTex; static emDepthFunc_t emDepthFunc; static emUniform1f_t emUniform1f;
static struct { GLint fbo, readFbo, vp[4], vao, prog, bs, bd, depthFunc, unit, tex0; GLboolean blend, scissor, depthTest, cm[4], dm; } emS;

static void LoadEmitConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    g_emitEnable = GetPrivateProfileIntA("emit", "Enable", 0, ini); g_emitMax = GetPrivateProfileIntA("emit", "MaxEmitters", 16, ini);
    char b[32]; GetPrivateProfileStringA("emit", "Strength", "1.0", b, sizeof b, ini); g_emitStrength = (float)atof(b);
    GetPrivateProfileStringA("emit", "Radius", "12", b, sizeof b, ini); g_emitRadius = (float)atof(b);
    if (g_emitMax < 0) g_emitMax = 0;
    for (int i = 0; i < 8192; i++) g_progTyEmitLoc[i] = -1;
    if (g_emitEnable) logf("[emit] enabled: strength %.2f radius %.1f max %d", g_emitStrength, g_emitRadius, g_emitMax);
}

// Shader rewrite (only when Enable=1): the stock output line emits the albedo (captured before the light multiply) when tyEmit = 1.
// Runs after the decal rewrite, which also ends in the stock output line. TestCompile in glShaderSource falls back to the stock shader on failure.
static bool EmitRewriteFS(std::string& s) {
    if (!g_emitEnable) return false;
    const char* kOut = "FINAL_COLOR = diffuseColour;";
    if (s.find(kOut) == std::string::npos || s.find("void main") == std::string::npos || s.find("uniform float tyEmit;") != std::string::npos) return false;
    const char* kLit = "diffuseColour *= light;";
    size_t lit = s.find(kLit);
    if (lit != std::string::npos && s.find(kLit, lit + 1) == std::string::npos) s.insert(lit, "vec4 tyEmAlb = diffuseColour;\n  ");   // albedo before the light multiply
    else lit = std::string::npos;
    ReplaceAll(s, kOut, lit != std::string::npos ? "if (tyEmit > 0.5) FINAL_COLOR = vec4(tyEmAlb.rgb, 1.0); else FINAL_COLOR = diffuseColour;"
                                                 : "vec4 tyEmAlb = diffuseColour;\n  if (tyEmit > 0.5) FINAL_COLOR = vec4(tyEmAlb.rgb, 1.0); else FINAL_COLOR = diffuseColour;");
    s.insert(s.rfind("void main"), "uniform float tyEmit;\n");
    return true;
}
static void EmitOnLink(GLuint p) {
    if (p < 8192) g_progTyEmitLoc[p] = o_glGetUniformLocation(p, "tyEmit");
}

static const char* kEmitBlur = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_src; uniform vec2 u_dir;
void main() {
  vec4 c = texture(u_src, uv) * 0.2270270;
  c += (texture(u_src, uv + u_dir) + texture(u_src, uv - u_dir)) * 0.1945946;
  c += (texture(u_src, uv + 2.0 * u_dir) + texture(u_src, uv - 2.0 * u_dir)) * 0.1216216;
  c += (texture(u_src, uv + 3.0 * u_dir) + texture(u_src, uv - 3.0 * u_dir)) * 0.0540540;
  c += (texture(u_src, uv + 4.0 * u_dir) + texture(u_src, uv - 4.0 * u_dir)) * 0.0162162;
  o = c;
}
)";
static const char* kEmitComp = R"(#version 330
in vec2 uv; out vec4 o;
uniform sampler2D u_em; uniform float u_strength;
void main() { o = vec4(texture(u_em, uv).rgb * u_strength, 1.0); }
)";
// half-res depth: the farthest of each 2x2 block of the full-res scene depth (conservative: an emitter pixel next to an occluder is not cut away)
static const char* kEmitDepth = R"(#version 330
uniform sampler2D u_d;
void main() {
  ivec2 sz = textureSize(u_d, 0) - 1;
  ivec2 p = ivec2(gl_FragCoord.xy) * 2;
  float d = max(max(texelFetch(u_d, min(p, sz), 0).r, texelFetch(u_d, min(p + ivec2(1, 0), sz), 0).r),
                max(texelFetch(u_d, min(p + ivec2(0, 1), sz), 0).r, texelFetch(u_d, min(p + ivec2(1, 1), sz), 0).r));
  gl_FragDepth = d;
}
)";

static GLuint MakeDepthTex(int w, int h) {
    GLuint t; gx.GenTextures(1, &t); o_glBindTexture(0x0DE1, t);
    o_glTexImage2D(0x0DE1, 0, 0x81A6 /*DEPTH_COMPONENT24*/, w, h, 0, 0x1902 /*DEPTH_COMPONENT*/, 0x1402 /*UNSIGNED_INT*/, nullptr);
    gx.TexParameteri(0x0DE1, 0x2801, 0x2600); gx.TexParameteri(0x0DE1, 0x2800, 0x2600); gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);
    return t;
}

static bool EmitInit(int w, int h) {
    if (g_emitInitFail || w < 2 || h < 2) return false;
    int hw = w / 2, hh = h / 2;
    if (!g_emitProgBlur) {
        emDrawBufs = (emDrawBufs_t)GLProc("glDrawBuffers"); emClearBufv = (emClearBufv_t)GLProc("glClearBufferfv"); emDelTex = (emDelTex_t)GLProc("glDeleteTextures");
        emDepthFunc = (emDepthFunc_t)GLProc("glDepthFunc"); emUniform1f = (emUniform1f_t)GLProc("glUniform1f");
        g_emitProgBlur = CompileProg("emit_blur", kVS, kEmitBlur, nullptr, nullptr);
        g_emitProgComp = CompileProg("emit_comp", kVS, kEmitComp, nullptr, nullptr);
        g_emitProgDepth = CompileProg("emit_depth", kVS, kEmitDepth, nullptr, nullptr);
        if (!g_emitProgBlur || !g_emitProgComp || !g_emitProgDepth || !emDrawBufs || !emClearBufv || !emDepthFunc || !emUniform1f) { logf("[emit] shaders or GL entry points missing, emitter light off"); g_emitInitFail = true; return false; }
        gx.GenVertexArrays(1, &g_emitVao); gx.GenFramebuffers(1, &g_emitFbo);
    }
    if (g_emitW != hw || g_emitH != hh || g_emitFullW != w || g_emitFullH != h) {
        GLint prevUnit, prevTex0; gx.GetIntegerv(0x84E0, &prevUnit); o_glActiveTexture(0x84C0); gx.GetIntegerv(0x8069, &prevTex0);
        if (emDelTex && g_emitTex[0]) { emDelTex(2, g_emitTex); GLuint dd[2] = {g_emitDepthFull, g_emitDepthHalf}; emDelTex(2, dd); }
        for (int i = 0; i < 2; i++) {
            g_emitTex[i] = MakeTex(hw, hh, 0x881A /*RGBA16F*/, 0x1908 /*RGBA*/);
            o_glBindTexture(0x0DE1, g_emitTex[i]);
            gx.TexParameteri(0x0DE1, 0x2801, 0x2601); gx.TexParameteri(0x0DE1, 0x2800, 0x2601); gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);
        }
        g_emitDepthFull = MakeDepthTex(w, h); g_emitDepthHalf = MakeDepthTex(hw, hh);
        o_glBindTexture(0x0DE1, (GLuint)prevTex0); o_glActiveTexture((GLenum)prevUnit);
        GLint prevFBO; gx.GetIntegerv(0x8CA6, &prevFBO);
        o_glBindFramebuffer(0x8D40, g_emitFbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g_emitTex[0], 0);
        o_glFramebufferTexture2D(0x8D40, 0x8D00 /*DEPTH_ATTACHMENT*/, 0x0DE1, g_emitDepthHalf, 0);
        bool ok = gx.CheckFramebufferStatus(0x8D40) == 0x8CD5;
        o_glBindFramebuffer(0x8D40, prevFBO);
        if (!ok) { logf("[emit] emission framebuffer incomplete, emitter light off"); g_emitInitFail = true; return false; }
        g_emitW = hw; g_emitH = hh; g_emitFullW = w; g_emitFullH = h; logf("[emit] emission buffer %dx%d (depth tested)", hw, hh);
    }
    return true;
}

// Called before an emitter draw has been issued (the caller then re-issues the draw with the original gl call, and calls EmitEnd).
// Returns false when the draw is not an emitter, the cap is reached, the shader has no tyEmit uniform, or the draw is not into the default framebuffer.
// Sets up: scene depth copy -> half-res depth (max of 2x2) on the emission buffer -> splat state (depth LEQUAL, additive, albedo output).
static bool EmitBegin() {
    // Init (textures, shaders) happens in RunEmit at end of frame, never inside a draw: texture creation would rebind the draw's textures.
    if (!g_emitEnable || g_emitActive || !g_emitW || g_curFBO != 0 || !g_curProg || g_curProg >= 8192 || !g_texEnable) return false;
    GLuint tex = g_bound2D[g_progDiffUnit[g_curProg] & 31];
    if (!MatPolicy(tex).emissive) return false;
    GLint tyLoc = g_progTyEmitLoc[g_curProg]; if (tyLoc < 0) return false;
    if (g_emitFrame != g_frame) { g_emitFrame = g_frame; g_emitCount = 0; g_emitCleared = false; }
    if (g_emitCount >= (unsigned)g_emitMax) return false;
    g_emitCount++; g_emitActive = true;
    gx.GetIntegerv(0x8CA6, &emS.fbo); gx.GetIntegerv(0x8CAA /*READ_FRAMEBUFFER_BINDING*/, &emS.readFbo); gx.GetIntegerv(0x0BA2, emS.vp); gx.GetIntegerv(0x85B5, &emS.vao); gx.GetIntegerv(0x8B8D, &emS.prog);
    gx.GetIntegerv(0x80C9, &emS.bs); gx.GetIntegerv(0x80C8, &emS.bd); gx.GetIntegerv(0x0B74 /*DEPTH_FUNC*/, &emS.depthFunc);
    gx.GetIntegerv(0x84E0, &emS.unit); o_glActiveTexture(0x84C0); gx.GetIntegerv(0x8069, &emS.tex0);
    emS.blend = gx.IsEnabled(0x0BE2); emS.scissor = gx.IsEnabled(0x0C11); emS.depthTest = gx.IsEnabled(0x0B71); gx.GetBooleanv(0x0C23, emS.cm); gx.GetBooleanv(0x0B72, &emS.dm);
    // 1) scene depth of FB0 (includes the emitter's own draw, just issued) -> full-res depth texture
    o_glBindFramebuffer(0x8CA8, 0);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, g_emitDepthFull);
    o_glCopyTexImage2D(0x0DE1, 0, 0x81A6, 0, 0, g_emitFullW, g_emitFullH, 0);
    // 2) max of each 2x2 -> half-res depth of the emission buffer (colour is not written: colour mask off)
    o_glBindFramebuffer(0x8D40, g_emitFbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g_emitTex[0], 0);
    o_glFramebufferTexture2D(0x8D40, 0x8D00, 0x0DE1, g_emitDepthHalf, 0);
    GLenum b0[1] = {0x8CE0}; emDrawBufs(1, b0);
    o_glColorMask(1, 1, 1, 1); o_glDisable(0x0C11);
    if (!g_emitCleared) { static const GLfloat zero[4] = {0, 0, 0, 0}; emClearBufv(0x1800 /*COLOR*/, 0, zero); g_emitCleared = true; }
    o_glViewport(0, 0, g_emitW, g_emitH);
    o_glColorMask(0, 0, 0, 0); o_glEnable(0x0B71); emDepthFunc(0x0207 /*ALWAYS*/); o_glDepthMask(1);
    o_glUseProgram(g_emitProgDepth); gx.Uniform1i(o_glGetUniformLocation(g_emitProgDepth, "u_d"), 0);
    gx.BindVertexArray(g_emitVao);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, g_emitDepthFull);
    o_glDrawArrays(4, 0, 3);
    // 3) splat state: the emitter's own program and VAO, depth LEQUAL against the copied scene depth, additive albedo
    o_glUseProgram((GLuint)emS.prog); gx.BindVertexArray((GLuint)emS.vao);
    emDepthFunc(0x0203 /*LEQUAL*/); o_glDepthMask(0); o_glColorMask(1, 1, 1, 1);
    o_glEnable(0x0BE2); o_glBlendFunc(0x0302 /*SRC_ALPHA*/, 1 /*ONE*/);
    emUniform1f(tyLoc, 1.0f);
    return true;
}
static void EmitEnd() {
    if (!g_emitActive) return;
    GLint tyLoc = g_progTyEmitLoc[g_curProg < 8192 ? g_curProg : 0];
    if (tyLoc >= 0) emUniform1f(tyLoc, 0.0f);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, (GLuint)emS.tex0); o_glActiveTexture((GLenum)emS.unit);
    o_glBindFramebuffer(0x8CA8, (GLuint)emS.readFbo);
    o_glBindFramebuffer(0x8D40, (GLuint)emS.fbo); o_glViewport(emS.vp[0], emS.vp[1], emS.vp[2], emS.vp[3]);
    o_glBlendFunc((GLenum)emS.bs, (GLenum)emS.bd); if (emS.blend) o_glEnable(0x0BE2); else o_glDisable(0x0BE2);
    if (emS.scissor) o_glEnable(0x0C11);
    emDepthFunc((GLenum)emS.depthFunc); if (emS.depthTest) o_glEnable(0x0B71); else o_glDisable(0x0B71);
    o_glColorMask(emS.cm[0], emS.cm[1], emS.cm[2], emS.cm[3]); o_glDepthMask(emS.dm);
    g_emitActive = false;
}

// Once per frame, from the end-of-3D-pass block (after GI, before atmos): blur the emission buffer and add it to FB0.
static void RunEmit(int w, int h) {
    if (!g_emitEnable || !EmitInit(w, h)) return;
    if (g_emitFrame != g_frame || !g_emitCount || g_emitComposed == g_frame) return;
    g_emitComposed = g_frame;
    GLint prevFBO, prevVP[4], prevVAO, prevProg, prevBS, prevBD, prevUnit, prevTex0; GLboolean prevCM[4], prevDM, blend, depth, cull, stencil, scissor;
    gx.GetIntegerv(0x8CA6, &prevFBO); gx.GetIntegerv(0x0BA2, prevVP); gx.GetIntegerv(0x85B5, &prevVAO); gx.GetIntegerv(0x8B8D, &prevProg);
    gx.GetIntegerv(0x80C9, &prevBS); gx.GetIntegerv(0x80C8, &prevBD); gx.GetBooleanv(0x0C23, prevCM); gx.GetBooleanv(0x0B72, &prevDM);
    gx.GetIntegerv(0x84E0, &prevUnit); o_glActiveTexture(0x84C0); gx.GetIntegerv(0x8069 /*TEXTURE_BINDING_2D*/, &prevTex0);
    blend = gx.IsEnabled(0x0BE2); depth = gx.IsEnabled(0x0B71); cull = gx.IsEnabled(0x0B44); stencil = gx.IsEnabled(0x0B90); scissor = gx.IsEnabled(0x0C11);
    gx.BindVertexArray(g_emitVao);
    o_glDisable(0x0B71); o_glDisable(0x0B44); o_glDisable(0x0B90); o_glDisable(0x0C11); o_glDepthMask(0);
    // 4 separable passes (H, V, H, V) ping-ponging between the two textures; the result ends in g_emitTex[0]
    GLuint src = g_emitTex[0], dst = g_emitTex[1];
    o_glUseProgram(g_emitProgBlur); int locSrc = o_glGetUniformLocation(g_emitProgBlur, "u_src"), locDir = o_glGetUniformLocation(g_emitProgBlur, "u_dir");
    gx.Uniform1i(locSrc, 0);
    for (int pass = 0; pass < 4; pass++) {
        int horiz = (pass % 2) == 0;
        float step = g_emitRadius * 0.5f;
        o_glBindFramebuffer(0x8D40, g_emitFbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, dst, 0);
        o_glViewport(0, 0, g_emitW, g_emitH);
        gx.Uniform2f(locDir, horiz ? step / g_emitW : 0.0f, horiz ? 0.0f : step / g_emitH);
        o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, src);
        o_glDrawArrays(4, 0, 3);
        GLuint t = src; src = dst; dst = t;
    }
    // composite: additive onto FB0, RGB only (alpha is the GI source mask, must not change)
    o_glBindFramebuffer(0x8D40, 0); o_glViewport(0, 0, w, h);
    o_glColorMask(1, 1, 1, 0); o_glEnable(0x0BE2); o_glBlendFunc(1, 1);
    o_glUseProgram(g_emitProgComp); gx.Uniform1i(o_glGetUniformLocation(g_emitProgComp, "u_em"), 0); gx.Uniform1f(o_glGetUniformLocation(g_emitProgComp, "u_strength"), g_emitStrength);
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, g_emitTex[0]);
    o_glDrawArrays(4, 0, 3);
    g_sceneModified = true;   // bloom reads the engine's colour copy: it must include the emitted light
    // restore
    o_glActiveTexture(0x84C0); o_glBindTexture(0x0DE1, (GLuint)prevTex0); o_glActiveTexture((GLenum)prevUnit);
    o_glBindFramebuffer(0x8D40, (GLuint)prevFBO); o_glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    o_glUseProgram((GLuint)prevProg); gx.BindVertexArray((GLuint)prevVAO);
    o_glBlendFunc((GLenum)prevBS, (GLenum)prevBD); if (blend) o_glEnable(0x0BE2); else o_glDisable(0x0BE2);
    if (depth) o_glEnable(0x0B71); if (cull) o_glEnable(0x0B44); if (stencil) o_glEnable(0x0B90); if (scissor) o_glEnable(0x0C11);
    o_glColorMask(prevCM[0], prevCM[1], prevCM[2], prevCM[3]); o_glDepthMask(prevDM);
}
