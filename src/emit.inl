// emit.inl - phase 2 emitter light injection (docs/MATERIAL_CLASSIFICATION_PLAN.md section 4; first user #9, the portal lights the ground).
// Emitter draws (policy column emissive: emitters, transparent emitters, emissive characters) are drawn once more, after the normal draw,
// into a half-res emission buffer with additive blend weighted by alpha. Once per frame, after GI and before atmos, the buffer is blurred and
// added to FB0. Cost: one extra draw per emitter, at most [emit] MaxEmitters per frame, plus a fixed blur and composite. No per-draw lights in shaders.
// ini [emit]: Enable=0 (default: none of this runs), Strength=1.0 (composite gain), Radius=12 (blur step in half-res texels, all emitters share it),
//             MaxEmitters=16 (cap on splatted emitter draws per frame; the first N in draw order, not yet ranked by screen area).
#include <cstdio>

static int g_emitEnable = 0, g_emitMax = 16; static float g_emitStrength = 1.0f, g_emitRadius = 12.0f;
static GLuint g_emitFbo, g_emitVao, g_emitTex[2], g_emitProgBlur, g_emitProgComp; static int g_emitW, g_emitH; static bool g_emitInitFail;
static unsigned g_emitFrame, g_emitCount, g_emitComposed; static bool g_emitCleared, g_emitActive;
typedef void(WINAPI* emDrawBufs_t)(GLsizei, const GLenum*); typedef void(WINAPI* emClearBufv_t)(GLenum, GLint, const GLfloat*); typedef void(WINAPI* emDelTex_t)(GLsizei, const GLuint*);
static emDrawBufs_t emDrawBufs; static emClearBufv_t emClearBufv; static emDelTex_t emDelTex;
static struct { GLint fbo, vp[4], vao, prog, bs, bd; GLboolean blend, scissor, cm[4], dm; } emS;

static void LoadEmitConfig() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    g_emitEnable = GetPrivateProfileIntA("emit", "Enable", 0, ini); g_emitMax = GetPrivateProfileIntA("emit", "MaxEmitters", 16, ini);
    char b[32]; GetPrivateProfileStringA("emit", "Strength", "1.0", b, sizeof b, ini); g_emitStrength = (float)atof(b);
    GetPrivateProfileStringA("emit", "Radius", "12", b, sizeof b, ini); g_emitRadius = (float)atof(b);
    if (g_emitMax < 0) g_emitMax = 0;
    if (g_emitEnable) logf("[emit] enabled: strength %.2f radius %.1f max %d", g_emitStrength, g_emitRadius, g_emitMax);
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

static bool EmitInit(int w, int h) {
    if (g_emitInitFail || w < 2 || h < 2) return false;
    int hw = w / 2, hh = h / 2;
    if (!g_emitProgBlur) {
        emDrawBufs = (emDrawBufs_t)GLProc("glDrawBuffers"); emClearBufv = (emClearBufv_t)GLProc("glClearBufferfv"); emDelTex = (emDelTex_t)GLProc("glDeleteTextures");
        g_emitProgBlur = CompileProg("emit_blur", kVS, kEmitBlur, nullptr, nullptr);
        g_emitProgComp = CompileProg("emit_comp", kVS, kEmitComp, nullptr, nullptr);
        if (!g_emitProgBlur || !g_emitProgComp || !emDrawBufs || !emClearBufv) { logf("[emit] shaders or GL entry points missing, emitter light off"); g_emitInitFail = true; return false; }
        gx.GenVertexArrays(1, &g_emitVao); gx.GenFramebuffers(1, &g_emitFbo);
    }
    if (g_emitW != hw || g_emitH != hh) {
        GLint prevUnit, prevTex0; gx.GetIntegerv(0x84E0, &prevUnit); o_glActiveTexture(0x84C0); gx.GetIntegerv(0x8069, &prevTex0);
        if (emDelTex && g_emitTex[0]) emDelTex(2, g_emitTex);
        for (int i = 0; i < 2; i++) {
            g_emitTex[i] = MakeTex(hw, hh, 0x881A /*RGBA16F*/, 0x1908 /*RGBA*/);
            o_glBindTexture(0x0DE1, g_emitTex[i]);
            gx.TexParameteri(0x0DE1, 0x2801, 0x2601); gx.TexParameteri(0x0DE1, 0x2800, 0x2601); gx.TexParameteri(0x0DE1, 0x2802, 0x812F); gx.TexParameteri(0x0DE1, 0x2803, 0x812F);
        }
        o_glBindTexture(0x0DE1, (GLuint)prevTex0); o_glActiveTexture((GLenum)prevUnit);
        GLint prevFBO; gx.GetIntegerv(0x8CA6, &prevFBO);
        o_glBindFramebuffer(0x8D40, g_emitFbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g_emitTex[0], 0);
        bool ok = gx.CheckFramebufferStatus(0x8D40) == 0x8CD5;
        o_glBindFramebuffer(0x8D40, prevFBO);
        if (!ok) { logf("[emit] emission framebuffer incomplete, emitter light off"); g_emitInitFail = true; return false; }
        g_emitW = hw; g_emitH = hh; logf("[emit] emission buffer %dx%d", hw, hh);
    }
    return true;
}

// Called before an emitter draw has been issued (the caller then re-issues the draw with the original gl call, and calls EmitEnd).
// Returns false when the draw is not an emitter, the cap is reached, or the draw is not into the default framebuffer.
static bool EmitBegin() {
    // Init (textures, shaders) happens in RunEmit at end of frame, never inside a draw: texture creation would rebind the draw's textures.
    if (!g_emitEnable || g_emitActive || !g_emitW || g_curFBO != 0 || !g_curProg || g_curProg >= 8192 || !g_texEnable) return false;
    GLuint tex = g_bound2D[g_progDiffUnit[g_curProg] & 31];
    if (!MatPolicy(tex).emissive) return false;
    if (g_emitFrame != g_frame) { g_emitFrame = g_frame; g_emitCount = 0; g_emitCleared = false; }
    if (g_emitCount >= (unsigned)g_emitMax) return false;
    g_emitCount++; g_emitActive = true;
    gx.GetIntegerv(0x8CA6, &emS.fbo); gx.GetIntegerv(0x0BA2, emS.vp); gx.GetIntegerv(0x85B5, &emS.vao); gx.GetIntegerv(0x8B8D, &emS.prog);
    gx.GetIntegerv(0x80C9, &emS.bs); gx.GetIntegerv(0x80C8, &emS.bd);
    emS.blend = gx.IsEnabled(0x0BE2); emS.scissor = gx.IsEnabled(0x0C11); gx.GetBooleanv(0x0C23, emS.cm); gx.GetBooleanv(0x0B72, &emS.dm);
    o_glBindFramebuffer(0x8D40, g_emitFbo); o_glFramebufferTexture2D(0x8D40, 0x8CE0, 0x0DE1, g_emitTex[0], 0);
    GLenum b0[1] = {0x8CE0}; emDrawBufs(1, b0);
    if (!g_emitCleared) { static const GLfloat zero[4] = {0, 0, 0, 0}; emClearBufv(0x1800 /*COLOR*/, 0, zero); g_emitCleared = true; }
    o_glViewport(0, 0, g_emitW, g_emitH);
    o_glDisable(0x0C11); o_glEnable(0x0BE2); o_glBlendFunc(0x0302 /*SRC_ALPHA*/, 1 /*ONE*/); o_glColorMask(1, 1, 1, 1); o_glDepthMask(0);
    return true;
}
static void EmitEnd() {
    if (!g_emitActive) return;
    o_glBindFramebuffer(0x8D40, (GLuint)emS.fbo); o_glViewport(emS.vp[0], emS.vp[1], emS.vp[2], emS.vp[3]);
    o_glBlendFunc((GLenum)emS.bs, (GLenum)emS.bd); if (emS.blend) o_glEnable(0x0BE2); else o_glDisable(0x0BE2);
    if (emS.scissor) o_glEnable(0x0C11);
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
