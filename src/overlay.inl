// overlay.inl - in-game settings overlay (issue #12). Toggle with [overlay] Key= (F1..F24 or a virtual-key number; default 0 = off, the panel is disabled until a key is set).
// The panel lists a few tyrender.ini settings that already have live reload paths. Keys while it is open:
//   Up / Down = select, Left / Right = change the value, Enter = keep and close, Esc or the toggle key = discard and close.
// Every change is written to tyrender.ini at once and the owning Load*Config() runs again, so the effect is visible immediately.
// Discard writes each touched key back to its value from when the panel opened (a key that was absent is removed again).
// The launcher stays the main settings UI. The panel only reads keys while the game window is in front, like the A/B hotkey.
// Known v1 limits: arrow keys also reach the game while the panel is open; the panel is drawn with GDI into a DIB and uploaded with glDrawPixels.
#include "build_id.h"

static int g_ovVk = 0;                          // off until [overlay] Key= is set (see default_tyrender.ini)
static bool g_ovOpen;
static int g_ovSel;
static bool g_ovKeyDown[8];                     // edge detect: 0 toggle, 1 up, 2 down, 3 left, 4 right, 5 enter, 6 esc
static HDC g_ovDc;
static HBITMAP g_ovBmp;
static HFONT g_ovFont;
static unsigned char* g_ovPix;                  // BGRA, bottom-up (the order glDrawPixels wants)
static const int OV_W = 470, OV_LINES = 9, OV_LH = 20, OV_PAD = 10;
static const int OV_H = OV_LINES * OV_LH + OV_PAD * 2;

struct OvItem { const char* sec; const char* key; float lo, hi, step; bool isInt; const char* label; };
static const OvItem g_ovItems[] = {
    {"post",    "Brightness", 0.50f, 1.50f, 0.02f, false, "Brightness"},
    {"post",    "Saturation", 0.00f, 2.00f, 0.05f, false, "Saturation"},
    {"post",    "Sharpen",    0.00f, 1.00f, 0.05f, false, "Sharpen"},
    {"post",    "Vignette",   0.00f, 1.00f, 0.05f, false, "Vignette"},
    {"atmos",   "Intensity",  0.00f, 4.00f, 0.10f, false, "Atmos intensity"},
    {"ao",      "Enable",     0.00f, 1.00f, 1.00f, true,  "AO enable"},
    {"tess",    "MaxLevel",   1.00f, 32.00f, 1.00f, true, "Tess max level"},
    {"shadows", "Taps",       4.00f, 32.00f, 4.00f, true, "Shadow taps"},
};
static const int OV_NITEMS = (int)(sizeof g_ovItems / sizeof g_ovItems[0]);
static float g_ovVal[OV_NITEMS];                // current value, mirrors the ini
static std::string g_ovOrig[OV_NITEMS];         // raw ini text at open ("\x01" = key absent)
static const char* OV_MISSING = "\x01";

static int OvParseVk(const char* s) {
    if ((s[0] == 'F' || s[0] == 'f') && isdigit((unsigned char)s[1])) { int n = atoi(s + 1); if (n >= 1 && n <= 24) return 0x70 + n - 1; }
    int n = atoi(s); return (n > 0 && n < 256) ? n : 0;
}
static void LoadOverlayConfig() {
    char ini[MAX_PATH], b[32]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    GetPrivateProfileStringA("overlay", "Key", "F10", b, sizeof b, ini);
    g_ovVk = OvParseVk(b);
}
static std::string OvIniRaw(const OvItem& it) {
    char ini[MAX_PATH], b[64]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    GetPrivateProfileStringA(it.sec, it.key, OV_MISSING, b, sizeof b, ini);
    return b;
}
static void OvReload(const OvItem& it) {
    if (!strcmp(it.sec, "post")) LoadFXConfig();
    else if (!strcmp(it.sec, "atmos")) LoadAtmosConfig();
    else if (!strcmp(it.sec, "ao")) LoadAOConfig();
    else if (!strcmp(it.sec, "tess")) LoadTessConfig();
    else if (!strcmp(it.sec, "shadows")) LoadShadowConfig();
    g_pbrStamp.clear(); g_tessStamp.clear();
}
static void OvWriteIni(const OvItem& it, const char* text) {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    WritePrivateProfileStringA(it.sec, it.key, text, ini);
    SessIniCheck("F10 overlay");
}
static void OvSetVal(int i, float v) {
    const OvItem& it = g_ovItems[i];
    if (v < it.lo) v = it.lo;
    if (v > it.hi) v = it.hi;
    if (it.isInt) v = (float)(int)(v + (v >= 0 ? 0.5f : -0.5f));
    g_ovVal[i] = v;
    char t[32]; if (it.isInt) snprintf(t, sizeof t, "%d", (int)v); else snprintf(t, sizeof t, "%.2f", v);
    OvWriteIni(it, t);
    OvReload(it);
}
static void OvOpen() {
    for (int i = 0; i < OV_NITEMS; i++) {
        const OvItem& it = g_ovItems[i];
        g_ovOrig[i] = OvIniRaw(it);
        float v = it.lo;
        if (g_ovOrig[i] != OV_MISSING) v = (float)atof(g_ovOrig[i].c_str());
        g_ovVal[i] = v;
    }
    g_ovSel = 0; g_ovOpen = true;
    logf("[overlay] opened (build %s)", TY_BUILD_ID_A); SessEvent("F10 overlay opened");
}
static void OvRevertAndClose() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    for (int i = 0; i < OV_NITEMS; i++) {
        const OvItem& it = g_ovItems[i];
        if (OvIniRaw(it) == g_ovOrig[i]) continue;
        WritePrivateProfileStringA(it.sec, it.key, g_ovOrig[i] == OV_MISSING ? NULL : g_ovOrig[i].c_str(), ini);
        OvReload(it);
    }
    g_ovOpen = false; logf("[overlay] discarded");
}
static void OvKeep() { g_ovOpen = false; logf("[overlay] kept (written to tyrender.ini)"); }

// Draws the panel into g_ovPix: GDI text on black, then alpha from luminance (text) or a dark panel background.
static void OvRender() {
    if (!g_ovDc) {
        g_ovDc = CreateCompatibleDC(NULL);
        BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = OV_W; bi.bmiHeader.biHeight = OV_H;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        g_ovBmp = CreateDIBSection(g_ovDc, &bi, DIB_RGB_COLORS, (void**)&g_ovPix, NULL, 0);
        SelectObject(g_ovDc, g_ovBmp);
        g_ovFont = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        SelectObject(g_ovDc, g_ovFont);
        SetBkMode(g_ovDc, TRANSPARENT);
    }
    if (!g_ovPix) return;
    RECT all = {0, 0, OV_W, OV_H}; FillRect(g_ovDc, &all, (HBRUSH)GetStockObject(BLACK_BRUSH));
    char line[160];
    snprintf(line, sizeof line, "TY overlay  build %s", TY_BUILD_ID_A);
    SetTextColor(g_ovDc, RGB(200, 200, 210)); TextOutA(g_ovDc, OV_PAD, OV_PAD, line, (int)strlen(line));
    for (int i = 0; i < OV_NITEMS; i++) {
        const OvItem& it = g_ovItems[i];
        if (it.isInt) snprintf(line, sizeof line, "%s %-18s %d", i == g_ovSel ? ">" : " ", it.label, (int)g_ovVal[i]);
        else snprintf(line, sizeof line, "%s %-18s %.2f", i == g_ovSel ? ">" : " ", it.label, g_ovVal[i]);
        SetTextColor(g_ovDc, i == g_ovSel ? RGB(255, 220, 80) : RGB(235, 235, 235));
        TextOutA(g_ovDc, OV_PAD, OV_PAD + (i + 1) * OV_LH, line, (int)strlen(line));
    }
    snprintf(line, sizeof line, "Up/Down select  Left/Right change");
    SetTextColor(g_ovDc, RGB(160, 160, 170)); TextOutA(g_ovDc, OV_PAD, OV_PAD + (OV_NITEMS + 1) * OV_LH, line, (int)strlen(line));
    snprintf(line, sizeof line, "Enter keep   Esc / F10 discard");
    TextOutA(g_ovDc, OV_PAD, OV_PAD + (OV_NITEMS + 2) * OV_LH, line, (int)strlen(line));
    GdiFlush();
    // GDI wrote BGRA with black background. Luminance gives text alpha; un-premultiply so the text keeps full colour.
    for (int p = 0; p < OV_W * OV_H; p++) {
        unsigned char* px = g_ovPix + p * 4;
        int lum = px[0] > px[1] ? (px[0] > px[2] ? px[0] : px[2]) : (px[1] > px[2] ? px[1] : px[2]);
        if (lum > 0) {
            for (int c = 0; c < 3; c++) px[c] = (unsigned char)(px[c] * 255 / lum);
            px[3] = (unsigned char)lum;
        } else {
            px[0] = 16; px[1] = 16; px[2] = 20; px[3] = 190;
        }
    }
}
// Log-only GL state dump for the first frames the panel draws (TEST uses it to find what differs before and after a draw).
typedef void(WINAPI* ov_gi_t)(GLenum, GLint*);
typedef GLboolean(WINAPI* ov_isen_t)(GLenum);
typedef GLenum(WINAPI* ov_err_t)(void);
static void OvDumpState(const char* tag) {
    static ov_gi_t getI = (ov_gi_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    static ov_isen_t isEn = (ov_isen_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glIsEnabled");
    static ov_err_t getErr = (ov_err_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetError");
    if (!getI) return;
    struct { const char* n; GLenum e; } ints[] = {
        {"program", 0x8B8D}, {"drawFB", 0x8CA6}, {"readFB", 0x8CAA}, {"vao", 0x85B5}, {"arrayBuf", 0x8894}, {"elemBuf", 0x8895},
        {"packBuf", 0x88ED}, {"unpackBuf", 0x88EF}, {"activeTex", 0x84E0}, {"tex2D", 0x8069}, {"drawBuf", 0x0C01}, {"readBuf", 0x0C02},
        {"matrixMode", 0x0BA0}, {"unpackAlign", 0x0CF5}, {"unpackRowLen", 0x0CF2}, {"blendSrc", 0x0BE1}, {"blendDst", 0x0BE0}, {"depthFunc", 0x0B74}};
    GLint v = 0; char line[640]; int n = snprintf(line, sizeof line, "[overlay-state] %s", tag);
    for (auto& i : ints) { getI(i.e, &v); n += snprintf(line + n, sizeof line - n, " %s=%d", i.n, v); }
    GLint vp[4] = {0, 0, 0, 0}, sc[4] = {0, 0, 0, 0}; getI(0x0BA2, vp); getI(0x0C10, sc);
    n += snprintf(line + n, sizeof line - n, " viewport=%d,%d,%d,%d scissor=%d,%d,%d,%d", vp[0], vp[1], vp[2], vp[3], sc[0], sc[1], sc[2], sc[3]);
    if (isEn) n += snprintf(line + n, sizeof line - n, " depth=%d stencil=%d scissorOn=%d blend=%d cull=%d alphaTest=%d",
        isEn(0x0B71), isEn(0x0B90), isEn(0x0C11), isEn(0x0BE2), isEn(0x0B44), isEn(0x0BC0));
    if (getErr) n += snprintf(line + n, sizeof line - n, " glError=%#x", (unsigned)getErr());
    logf("%s", line);
}

static void OvDraw(HDC dc) {
    HWND wnd = WindowFromDC(dc); RECT rc;
    if (!wnd || !GetClientRect(wnd, &rc)) return;
    OvRender(); if (!g_ovPix) return;
    typedef void(WINAPI* wp_t)(GLint, GLint); typedef void(WINAPI* pd_t)(GLsizei, GLsizei, GLenum, GLenum, const void*);
    typedef void(WINAPI* up_t)(GLenum); typedef void(WINAPI* pa_t)(GLbitfield); typedef void(WINAPI* pca_t)(GLbitfield); typedef void(WINAPI* v_t)(void);
    typedef void(WINAPI* gi_t)(GLenum, GLint*); typedef void(WINAPI* fb_t)(GLenum, GLuint); typedef void(WINAPI* en_t)(GLenum);
    typedef void(WINAPI* bva_t)(GLuint); typedef void(WINAPI* bb_t)(GLenum, GLuint); typedef void(WINAPI* at_t)(GLenum);
    static wp_t wp = (wp_t)GLProc("glWindowPos2i");
    static pd_t pd = (pd_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glDrawPixels");
    // The raw entry points, not GLProc(): GLProc returns the DLL hooks. h_glUseProgram runs PbrApply/LightApply and rewrites g_curProg,
    // and h_glActiveTexture / h_glBindTexture rewrite the g_activeUnit / g_bound2D caches. Raw calls leave those caches alone.
    static up_t up = (up_t)o_glUseProgram;
    static fb_t bindFb = o_glBindFramebuffer;
    static bva_t bindVao = (bva_t)GLProc("glBindVertexArray");
    static bb_t bindBuf = (bb_t)GLProc("glBindBuffer");
    static at_t activeTex = (at_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glActiveTexture");
    static pa_t pushA = (pa_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPushAttrib");
    static v_t popA = (v_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPopAttrib");
    static pca_t pushC = (pca_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPushClientAttrib");
    static v_t popC = (v_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPopClientAttrib");
    static gi_t getI = (gi_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    static en_t enable = (en_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glEnable");
    static en_t disable = (en_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glDisable");
    typedef void(WINAPI* cm_t)(GLboolean, GLboolean, GLboolean, GLboolean); typedef void(WINAPI* db_t)(GLenum);
    typedef void(WINAPI* pz_t)(GLfloat, GLfloat); typedef void(WINAPI* bf_t)(GLenum, GLenum);
    typedef void(WINAPI* ps_t)(GLenum, GLint);
    static cm_t colorMask = (cm_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glColorMask");
    static db_t drawBuffer = (db_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glDrawBuffer");
    static pz_t pixelZoom = (pz_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPixelZoom");
    static bf_t blendFunc = (bf_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glBlendFunc");
    static ps_t ps = (ps_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPixelStorei");
    if (!wp || !pd || !pushA || !popA || !pushC || !popC || !getI || !enable || !disable) {
        static bool logged; if (!logged) { logged = true; logf("[overlay] draw skipped: missing GL entry point (wp=%p pd=%p)", (void*)wp, (void*)pd); }
        return;
    }
    static int dumped;                             // log the state before and after the first three draws
    bool dump = dumped < 3;
    if (dump) OvDumpState("before draw");
    // Everything the draw can touch. GL_ALL_ATTRIB_BITS covers enables, colour, pixel, depth, blend, scissor, viewport, texture-env and
    // current state, and the client bit covers pixel-store and client vertex arrays. Bindings that the attribute stack does not save
    // (program, FBO, VAO, buffers, active texture) are saved by hand below. Matrices are not touched: glWindowPos2i and glDrawPixels ignore them.
    GLint prog = 0, drawFb = 0, readFb = 0, vao = 0, arrBuf = 0, elemBuf = 0, packBuf = 0, unpackBuf = 0, activeT = 0;
    getI(0x8B8D, &prog); getI(0x8CA6, &drawFb); getI(0x8CAA, &readFb); getI(0x85B5, &vao); getI(0x8894, &arrBuf); getI(0x8895, &elemBuf);
    getI(0x88ED, &packBuf); getI(0x88EF, &unpackBuf); getI(0x84E0, &activeT);
    pushA(0xFFFFFFFF /*GL_ALL_ATTRIB_BITS*/);
    pushC(0xFFFFFFFF /*GL_CLIENT_ALL_CLIENT_ATTRIB_BITS*/);
    if (up) up(0);
    if (bindFb) { bindFb(0x8CA9 /*GL_DRAW_FRAMEBUFFER*/, 0); bindFb(0x8CAA /*GL_READ_FRAMEBUFFER*/, 0); }
    if (bindVao) bindVao(0);
    if (bindBuf) { bindBuf(0x8894, 0); bindBuf(0x8895, 0); bindBuf(0x88EF /*PIXEL_UNPACK*/, 0); bindBuf(0x88ED /*PIXEL_PACK*/, 0); }
    if (activeTex) activeTex(0x84C0 /*GL_TEXTURE0*/);      // glDrawPixels samples no textures; the unit is set back below
    disable(0x0B71 /*DEPTH_TEST*/); disable(0x0B90 /*STENCIL_TEST*/); disable(0x0C11 /*SCISSOR_TEST*/); disable(0x0BC0 /*ALPHA_TEST*/); enable(0x0BE2 /*BLEND*/);
    if (colorMask) colorMask(1, 1, 1, 1);          // a leftover colour mask would hide the panel
    if (drawBuffer) drawBuffer(0x0405 /*GL_BACK*/);
    if (pixelZoom) pixelZoom(1.0f, 1.0f);
    if (blendFunc) blendFunc(0x0302 /*SRC_ALPHA*/, 0x0303 /*ONE_MINUS_SRC_ALPHA*/);
    if (ps) { ps(0x0CF5 /*UNPACK_ALIGNMENT*/, 4); ps(0x0CF2 /*UNPACK_ROW_LENGTH*/, 0); ps(0x0CF3 /*UNPACK_SKIP_ROWS*/, 0); ps(0x0CF4 /*UNPACK_SKIP_PIXELS*/, 0); }
    if (up) up(0);                                 // program 0 for the fixed-function path; restored from prog below
    wp(12, rc.bottom - OV_PAD - OV_H);
    pd(OV_W, OV_H, 0x80E1 /*GL_BGRA*/, 0x1401 /*GL_UNSIGNED_BYTE*/, g_ovPix);
    // restore: hand-saved bindings first, then the attribute stacks
    if (up) up((GLuint)prog);
    if (bindFb) { bindFb(0x8CA9, (GLuint)drawFb); bindFb(0x8CAA, (GLuint)readFb); }
    if (bindVao) bindVao((GLuint)vao);
    if (bindBuf) { bindBuf(0x8894, (GLuint)arrBuf); bindBuf(0x8895, (GLuint)elemBuf); bindBuf(0x88EF, (GLuint)unpackBuf); bindBuf(0x88ED, (GLuint)packBuf); }
    if (activeTex) activeTex((GLenum)activeT);
    popC(); popA();
    if (dump) { OvDumpState("after draw"); dumped++; }
}

// Edge-detected keys, read while the game window is in front. Called once per frame, before the swap.
static void OverlayFrame(HDC dc) {
    if (!g_ovVk) { if (g_ovOpen) OvRevertAndClose(); return; }
    HWND wnd = WindowFromDC(dc);
    bool front = wnd && GetForegroundWindow() == wnd;
    auto edge = [&](int idx, int vk) { bool d = front && (GetAsyncKeyState(vk) & 0x8000) != 0; bool f = d && !g_ovKeyDown[idx]; g_ovKeyDown[idx] = d; return f; };
    bool tog = false;
    { bool d = (GetAsyncKeyState(g_ovVk) & 0x8000) != 0; tog = d && !g_ovKeyDown[0]; g_ovKeyDown[0] = d; }
    bool up = edge(1, VK_UP), dn = edge(2, VK_DOWN), lf = edge(3, VK_LEFT), rt = edge(4, VK_RIGHT), en = edge(5, VK_RETURN), es = edge(6, VK_ESCAPE);
    if (!g_ovOpen) { if (tog) OvOpen(); }
    else {
        if (tog || es) OvRevertAndClose();
        else if (en) OvKeep();
        else {
            if (up && g_ovSel > 0) g_ovSel--;
            if (dn && g_ovSel < OV_NITEMS - 1) g_ovSel++;
            const OvItem& it = g_ovItems[g_ovSel];
            if (lf) OvSetVal(g_ovSel, g_ovVal[g_ovSel] - it.step);
            if (rt) OvSetVal(g_ovSel, g_ovVal[g_ovSel] + it.step);
        }
    }
    if (g_ovOpen) {
        OvDraw(dc);
        static int frames; if (++frames == 1 || frames == 120) logf("[overlay] drawn frame %d (front=%d)", frames, (int)front);
    }
}
