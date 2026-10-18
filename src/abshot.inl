// abshot.inl - A/B screenshot hotkey (issue #14). One press of the key (default F11, [shots] Key= in tyrender.ini; a virtual-key
// number works too, 0 = off) captures two frames: the current one, then the next one with the relief + PBR extras flipped
// (g_tsLive), then restores the original state. Both PNGs go to <game>\Screenshots\ as
//   ty_<build>_<preset>_<shaders-on|shaders-off>_<yyyymmdd-hhmmss>.png
// The preset name comes from [shots] Preset=, which the launcher writes. The hotkey only fires while the game window is in front.
// It replaces the ABToggle debug-camera file trigger for users; that trigger stays as a harness aid when [tess] ABToggle=1.
#include "build_id.h"                   // TY_BUILD_ID_A, written by build.ps1
static int g_shotVk = 0x7A;             // VK_F11
static float g_shotOrigLive = 1.0f;     // g_tsLive before the press, restored after the pair
static char g_shotStamp[24];
static bool g_shotDown;                 // key state at the previous frame (edge detect)
static HWND g_shotWnd;
static char g_shotTitle[256];
static ULONGLONG g_shotTitleUntil;      // when the window title flash ends (0 = none)

static int ShotParseVk(const char* s) {
    if ((s[0] == 'F' || s[0] == 'f') && isdigit((unsigned char)s[1])) { int n = atoi(s + 1); if (n >= 1 && n <= 24) return 0x70 + n - 1; }   // F1 = 0x70
    int n = atoi(s); return (n > 0 && n < 256) ? n : 0;
}
static void LoadShotConfig() {
    char ini[MAX_PATH], b[32]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    GetPrivateProfileStringA("shots", "Key", "F11", b, sizeof b, ini);
    g_shotVk = ShotParseVk(b);
}
static bool ShotReadFrame(HDC dc, int& w, int& h, std::string& px) {
    HWND wnd = WindowFromDC(dc); RECT rc;
    if (!wnd || !GetClientRect(wnd, &rc)) return false;
    w = rc.right; h = rc.bottom; if (w <= 0 || h <= 0) return false;
    typedef void(WINAPI* gi_t)(GLenum, GLint*); typedef void(WINAPI* ps_t)(GLenum, GLint);
    static gi_t getI = (gi_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glGetIntegerv");
    static ps_t ps = (ps_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), "glPixelStorei");
    GLint prevRead = 0, prevAlign = 4;
    if (getI) { getI(0x8CAA /*GL_READ_FRAMEBUFFER_BINDING*/, &prevRead); getI(0x0D05 /*GL_PACK_ALIGNMENT*/, &prevAlign); }
    if (o_glBindFramebuffer) o_glBindFramebuffer(0x8CA8 /*GL_READ_FRAMEBUFFER*/, 0);
    if (ps) ps(0x0D05, 1);
    px.assign((size_t)w * h * 3, 0);
    o_glReadPixels(0, 0, w, h, 0x80E0 /*GL_BGR*/, 0x1401 /*GL_UNSIGNED_BYTE*/, &px[0]);
    if (o_glBindFramebuffer) o_glBindFramebuffer(0x8CA8, (GLuint)prevRead);   // put the game's read binding back
    if (ps) ps(0x0D05, prevAlign);
    return true;
}
static uint32_t ShotCrc(uint32_t crc, const unsigned char* d, size_t n) {
    static uint32_t tab[256]; static bool init;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; tab[i] = c; } init = true; }
    crc = ~crc; for (size_t i = 0; i < n; i++) crc = tab[(crc ^ d[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
// Writes an 8-bit RGB PNG. px is GL_BGR, bottom row first. Deflate uses stored (uncompressed) blocks: no codec dependency, files are big.
static bool ShotWritePng(const char* path, int w, int h, const std::string& px) {
    size_t rowLen = (size_t)w * 3 + 1;
    std::vector<unsigned char> raw(rowLen * h);
    for (int y = 0; y < h; y++) {
        unsigned char* o = &raw[(size_t)y * rowLen]; const unsigned char* s = (const unsigned char*)&px[(size_t)(h - 1 - y) * w * 3];
        o[0] = 0;   // filter: none
        for (int x = 0; x < w; x++) { o[1 + x * 3] = s[x * 3 + 2]; o[2 + x * 3] = s[x * 3 + 1]; o[3 + x * 3] = s[x * 3]; }
    }
    std::vector<unsigned char> z; z.push_back(0x78); z.push_back(0x01);   // zlib header, no preset dictionary
    size_t pos = 0;
    do {
        size_t n = raw.size() - pos; if (n > 65535) n = 65535;
        bool last = pos + n >= raw.size();
        z.push_back(last ? 1 : 0); z.push_back((unsigned char)(n & 0xFF)); z.push_back((unsigned char)(n >> 8));
        z.push_back((unsigned char)(~n & 0xFF)); z.push_back((unsigned char)((~n >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n); pos += n;
    } while (pos < raw.size());
    uint32_t a = 1, b = 0; for (unsigned char c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    uint32_t adler = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) z.push_back((unsigned char)(adler >> s));
    FILE* f = fopen(path, "wb"); if (!f) return false;
    const unsigned char sig[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10}; fwrite(sig, 1, 8, f);
    auto chunk = [&](const char* type, const unsigned char* d, size_t n) {
        unsigned char len[4] = {(unsigned char)(n >> 24), (unsigned char)(n >> 16), (unsigned char)(n >> 8), (unsigned char)n};
        fwrite(len, 1, 4, f); fwrite(type, 1, 4, f); if (n) fwrite(d, 1, n, f);
        uint32_t c = ShotCrc(ShotCrc(0, (const unsigned char*)type, 4), d, n);
        unsigned char cb[4] = {(unsigned char)(c >> 24), (unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c}; fwrite(cb, 1, 4, f);
    };
    unsigned char ih[13] = {(unsigned char)(w >> 24), (unsigned char)(w >> 16), (unsigned char)(w >> 8), (unsigned char)w,
                            (unsigned char)(h >> 24), (unsigned char)(h >> 16), (unsigned char)(h >> 8), (unsigned char)h, 8, 2, 0, 0, 0};
    chunk("IHDR", ih, sizeof ih);
    chunk("IDAT", z.data(), z.size());
    chunk("IEND", nullptr, 0);
    fclose(f);
    return true;
}
// One PNG of the frame just drawn. label = "shaders-on" or "shaders-off" (taken from g_tsLive at the moment of the capture).
static void ShotCapture(HDC dc, const char* label) {
    int w = 0, h = 0; std::string px; if (!ShotReadFrame(dc, w, h, px)) { logf("[shot] no frame to read"); return; }
    char ini[MAX_PATH], preset[32]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    GetPrivateProfileStringA("shots", "Preset", "custom", preset, sizeof preset, ini);
    for (char* p = preset; *p; p++) if (!isalnum((unsigned char)*p) && *p != '-') *p = '-';   // file-name safe
    if (!preset[0]) strcpy(preset, "custom");
    char dir[MAX_PATH]; snprintf(dir, sizeof dir, "%sScreenshots", g_dir); CreateDirectoryA(dir, nullptr);
    char out[MAX_PATH]; snprintf(out, sizeof out, "%s\\ty_%s_%s_%s_%s.png", dir, TY_BUILD_ID_A, preset, label, g_shotStamp);
    if (ShotWritePng(out, w, h, px)) { logf("[shot] %s %dx%d frame=%u", LogPath(out).c_str(), w, h, g_frame); SessEvent("screenshot %s", strrchr(out, '\\') ? strrchr(out, '\\') + 1 : out); }
    else logf("[shot] could not write %s", LogPath(out).c_str());
    // on-screen confirmation: flash the window title for ~2.5 s
    if (g_shotWnd) { char t[320]; snprintf(t, sizeof t, "%s  |  A/B %s saved", g_shotTitle, label); SetWindowTextA(g_shotWnd, t); g_shotTitleUntil = GetTickCount64() + 2500; }
}
// Push the current relief/PBR values straight to every known program. The pushes otherwise run only from glUseProgram, so a
// program that stays bound across the flip would keep the old values.
static void ApplyShadowStrength(GLuint p);   // tyrender.cpp
static void ABRepush() {
    std::vector<GLuint> progs;
    for (auto& kv : g_pbrLocs) progs.push_back(kv.first);
    for (auto& kv : g_tessLoc) if (!g_pbrLocs.count(kv.first)) progs.push_back(kv.first);
    for (GLuint p : progs) { g_pbrStamp.erase(p); g_tessStamp.erase(p); PbrApply(p); }
    for (auto& kv : g_shadowStrengthLoc) ApplyShadowStrength(kv.first);   // the shadow factor also flips with the off frame
}
// Once per frame, after the frame is drawn and before SwapBuffers.
static void ABShotFrame(HDC dc) {
    if (g_shotTitleUntil && GetTickCount64() > g_shotTitleUntil && g_shotWnd) { SetWindowTextA(g_shotWnd, g_shotTitle); g_shotTitleUntil = 0; }
    if (!g_shotVk) return;
    if (g_shotPhase == 0) {
        bool down = (GetAsyncKeyState(g_shotVk) & 0x8000) != 0; bool fire = down && !g_shotDown; g_shotDown = down;
        if (!fire) return;
        HWND wnd = WindowFromDC(dc); if (!wnd || GetForegroundWindow() != wnd) return;
        SYSTEMTIME st; GetLocalTime(&st);
        snprintf(g_shotStamp, sizeof g_shotStamp, "%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        if (!g_shotTitleUntil) { g_shotWnd = wnd; GetWindowTextA(wnd, g_shotTitle, sizeof g_shotTitle); }
        g_shotOrigLive = g_tsLive;
        ShotCapture(dc, g_tsLive > 0.5f ? "shaders-on" : "shaders-off");
        g_tsLive = g_shotOrigLive > 0.5f ? 0.0f : 1.0f;             // the next frame renders the other state
        g_abOff = g_shotOrigLive > 0.5f; ABRepush();                // with shaders on, the next frame is the vanilla off frame (passes skip)
        g_shotPhase = 1;
        return;
    }
    // phase 1: the frame drawn with the flipped state is now complete
    ShotCapture(dc, g_tsLive > 0.5f ? "shaders-on" : "shaders-off");
    g_tsLive = g_shotOrigLive;
    g_abOff = false; ABRepush();
    g_shotPhase = 0;
}
