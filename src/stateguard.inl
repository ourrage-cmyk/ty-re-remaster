// stateguard.inl — opt-in GL state leak detector ([debug] StateGuard=1 in tyrender.ini). Included by gpuprof.inl.
// At the begin mark of each custom pass (GProf marks 1,3,5,7) it snapshots ~60 pieces of GL state; at the end mark (2,4,6,8) it snapshots again and logs every
// difference as "[state] LEAK after <pass>: <name> <before> -> <after>" (max 4 distinct lines per pass, then a counter). A pass must leave the engine's state exactly
// as it found it; a leaked program/buffer/VAO/image-unit/pixel-buffer binding corrupts engine draws (e.g. skinned meshes) without any GL error.
#include <string>
#include <vector>
static int g_sgEnable = -1;   // -1 = read ini on first use
struct SGSnap { std::vector<std::pair<std::string, int>> v; };
static SGSnap g_sgBegin[5]; static int g_sgLeaks[5]; static std::vector<std::string> g_sgSeen[5];
static const char* SG_PASS[5] = {"", "ao", "gi", "atmos", "post"};
typedef void(WINAPI* sg_geti_t)(GLenum, GLuint, GLint*);
static void SGCapture(SGSnap& s) {
    s.v.clear(); auto add = [&](const char* n, int x) { s.v.push_back({n, x}); }; GLint t[4];
    auto I = [&](const char* n, GLenum e) { t[0] = -999; gx.GetIntegerv(e, t); add(n, t[0]); };
    I("CURRENT_PROGRAM", 0x8B8D); I("ARRAY_BUFFER_BINDING", 0x8894); I("ELEMENT_ARRAY_BUFFER_BINDING", 0x8895); I("VERTEX_ARRAY_BINDING", 0x85B5);
    I("DRAW_FRAMEBUFFER_BINDING", 0x8CA6); I("READ_FRAMEBUFFER_BINDING", 0x8CAA); I("PIXEL_PACK_BUFFER_BINDING", 0x88ED); I("PIXEL_UNPACK_BUFFER_BINDING", 0x88EF);
    I("UNIFORM_BUFFER_BINDING", 0x8A28); I("SHADER_STORAGE_BUFFER_BINDING", 0x90D3); I("DISPATCH_INDIRECT_BUFFER_BINDING", 0x90EE);
    I("DEPTH_FUNC", 0x0B74); I("BLEND_SRC", 0x0BE1); I("BLEND_DST", 0x0BE0); I("CULL_FACE_MODE", 0x0B45); I("FRONT_FACE", 0x0B46); I("PATCH_VERTICES", 0x8E72); I("ACTIVE_TEXTURE", 0x84E0);
    gx.GetIntegerv(0x0BA2, t); for (int i = 0; i < 4; i++) add("VIEWPORT", t[i]); gx.GetIntegerv(0x0C10, t); for (int i = 0; i < 4; i++) add("SCISSOR_BOX", t[i]);
    GLboolean b[4]; gx.GetBooleanv(0x0B72, b); add("DEPTH_WRITEMASK", b[0]); gx.GetBooleanv(0x0C23, b); add("COLOR_WRITEMASK", b[0] | (b[1] << 1) | (b[2] << 2) | (b[3] << 3));
    const GLenum en[] = {0x0B71, 0x0BE2, 0x0B44, 0x0C11, 0x0B90}; const char* enn[] = {"DEPTH_TEST", "BLEND", "CULL_FACE", "SCISSOR_TEST", "STENCIL_TEST"}; for (int i = 0; i < 5; i++) add(enn[i], gx.IsEnabled(en[i]));
    GLint unit = 0; gx.GetIntegerv(0x84E0, &unit);
    for (int u = 0; u < 8; u++) { o_glActiveTexture(0x84C0 + u); char nm[40]; t[0] = 0; gx.GetIntegerv(0x8069, t); snprintf(nm, sizeof nm, "TEX2D_UNIT%d", u); add(nm, t[0]);
        t[0] = 0; gx.GetIntegerv(0x806A, t); snprintf(nm, sizeof nm, "TEX3D_UNIT%d", u); add(nm, t[0]); }
    o_glActiveTexture(unit);
    static sg_geti_t geti = (sg_geti_t)GLProc("glGetIntegeri_v");
    if (geti) for (int i = 0; i < 4; i++) { t[0] = 0; geti(0x8F3A /*IMAGE_BINDING_NAME*/, i, t); char nm[32]; snprintf(nm, sizeof nm, "IMAGE_UNIT%d", i); add(nm, t[0]); }
    static GLenum(WINAPI* getErr)() = (GLenum(WINAPI*)())GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetError");
    unsigned err = 0; while (getErr && err < 8 && getErr() != 0) err++; add("GL_ERRORS_PENDING", err);
}
static void SGMark(int m) {
    static unsigned lastRead = 0; if (g_sgEnable < 0 || g_frame - lastRead >= 300) { lastRead = g_frame; char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); int e = GetPrivateProfileIntA("debug", "StateGuard", 0, ini); if (e && e != g_sgEnable) logf("[state] StateGuard ON: logging GL state leaked by custom passes"); g_sgEnable = e; }
    if (!g_sgEnable || !gx.GetIntegerv || m < 1 || m > 8) return;
    int pass = (m + 1) / 2;
    if (m & 1) { SGCapture(g_sgBegin[pass]); return; }
    SGSnap now; SGCapture(now); if (now.v.size() != g_sgBegin[pass].v.size()) return;
    for (size_t i = 0; i < now.v.size(); i++) if (now.v[i].second != g_sgBegin[pass].v[i].second) {
        g_sgLeaks[pass]++; char key[96]; snprintf(key, sizeof key, "%s %d>%d", now.v[i].first.c_str(), g_sgBegin[pass].v[i].second, now.v[i].second); bool seen = false; for (auto& k : g_sgSeen[pass]) if (k == key) seen = true;   // each DISTINCT leak is logged once per pass
        if (!seen && g_sgSeen[pass].size() < 24) { g_sgSeen[pass].push_back(key); logf("[state] LEAK after %s: %s %d -> %d (total leaks this pass so far: %d)", SG_PASS[pass], now.v[i].first.c_str(), g_sgBegin[pass].v[i].second, now.v[i].second, g_sgLeaks[pass]); } }
}
