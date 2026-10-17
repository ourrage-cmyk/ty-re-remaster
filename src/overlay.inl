// overlay.inl - in-game settings overlay (issue #12). Toggle with [overlay] Key= (default F10, F1..F24 or a virtual-key number, 0 = off).
// The panel lists a few tyrender.ini settings that already have live reload paths. Keys while it is open:
//   Up / Down = select, Left / Right = change the value, Enter = keep and close, Esc or the toggle key = discard and close.
// Every change is written to tyrender.ini at once and the owning Load*Config() runs again, so the effect is visible immediately.
// Discard writes each touched key back to its value from when the panel opened (a key that was absent is removed again).
// The launcher stays the main settings UI. The panel only reads keys while the game window is in front, like the A/B hotkey.
// Known v1 limits: arrow keys also reach the game while the panel is open; the panel is drawn with GDI into a DIB and uploaded with glDrawPixels.
#include "build_id.h"

static int g_ovVk = 0x79;                       // VK_F10
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
    logf("[overlay] opened (build %s)", TY_BUILD_ID_A);
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
static void OvDraw(HDC dc) {
    HWND wnd = WindowFromDC(dc); RECT rc;
    if (!wnd || !GetClientRect(wnd, &rc)) return;
    OvRender(); if (!g_ovPix) return;
    typedef void(WINAPI* wp_t)(GLint, GLint); typedef void(WINAPI* pd_t)(GLsizei, GLsizei, GLenum, GLenum, const void*);
    typedef void(WINAPI* up_t)(GLenum); typedef void(WINAPI* pa_t)(GLbitfield); typedef void(WINAPI* pca_t)(GLbitfield); typedef void(WINAPI* v_t)(void);
    typedef void(WINAPI* gi_t)(GLenum, GLint*); typedef void(WINAPI* fb_t)(GLenum, GLuint); typedef void(WINAPI* en_t)(GLenum);
    static wp_t wp = (wp_t)GLProc("glWindowPos2i");
    static pd_t pd = (pd_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glDrawPixels");
    static up_t up = (up_t)GLProc("glUseProgram");
    static pa_t pushA = (pa_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPushAttrib");
    static v_t popA = (v_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPopAttrib");
    static pca_t pushC = (pca_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPushClientAttrib");
    static v_t popC = (v_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPopClientAttrib");
    static gi_t getI = (gi_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    static fb_t bindFb = o_glBindFramebuffer;
    static en_t enable = (en_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glEnable");
    static en_t disable = (en_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glDisable");
    if (!wp || !pd || !pushA || !popA || !pushC || !popC || !getI || !enable || !disable) return;
    GLint prog = 0, drawFb = 0; getI(0x8B8D /*CURRENT_PROGRAM*/, &prog); getI(0x8CA6 /*DRAW_FRAMEBUFFER_BINDING*/, &drawFb);
    pushA(0x00000004 /*GL_ENABLE_BIT*/ | 0x00000400 /*GL_COLOR_BUFFER_BIT*/ | 0x00000020 /*GL_PIXEL_MODE_BIT*/ | 0x00000001 /*GL_CURRENT_BIT*/);
    pushC(0x00000001 /*GL_CLIENT_PIXEL_STORE_BIT*/);
    if (up) up(0);
    if (bindFb) bindFb(0x8CA9 /*GL_DRAW_FRAMEBUFFER*/, 0);
    disable(0x0B71 /*DEPTH_TEST*/); disable(0x0B90 /*STENCIL_TEST*/); enable(0x0BE2 /*BLEND*/);
    typedef void(WINAPI* bf_t)(GLenum, GLenum); static bf_t blendFunc = (bf_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glBlendFunc");
    if (blendFunc) blendFunc(0x0302 /*SRC_ALPHA*/, 0x0303 /*ONE_MINUS_SRC_ALPHA*/);
    typedef void(WINAPI* ps_t)(GLenum, GLint); static ps_t ps = (ps_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPixelStorei");
    if (ps) { ps(0x0CF5 /*UNPACK_ALIGNMENT*/, 4); ps(0x0CF2 /*UNPACK_ROW_LENGTH*/, 0); }
    wp(12, rc.bottom - OV_PAD - OV_H);
    pd(OV_W, OV_H, 0x80E1 /*GL_BGRA*/, 0x1401 /*GL_UNSIGNED_BYTE*/, g_ovPix);
    if (up) up((GLuint)prog);
    if (bindFb) bindFb(0x8CA9 /*GL_DRAW_FRAMEBUFFER*/, (GLuint)drawFb);
    popC(); popA();
}

// Edge-detected keys, read while the game window is in front. Called once per frame, before the swap.
static void OverlayFrame(HDC dc) {
    if (!g_ovVk) { if (g_ovOpen) OvRevertAndClose(); return; }
    HWND wnd = WindowFromDC(dc);
    bool front = wnd && GetForegroundWindow() == wnd;
    auto edge = [&](int idx, int vk) { bool d = front && (GetAsyncKeyState(vk) & 0x8000) != 0; bool f = d && !g_ovKeyDown[idx]; g_ovKeyDown[idx] = d; return f; };
    bool tog = edge(0, g_ovVk), up = edge(1, VK_UP), dn = edge(2, VK_DOWN), lf = edge(3, VK_LEFT), rt = edge(4, VK_RIGHT), en = edge(5, VK_RETURN), es = edge(6, VK_ESCAPE);
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
    if (g_ovOpen) OvDraw(dc);
}
