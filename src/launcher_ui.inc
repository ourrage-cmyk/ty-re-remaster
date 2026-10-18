// ------------------------------------------------------------ settings model (included by TYLauncher.c)
// Toggles/combos map to tyrender.ini; sliders map to float keys. While the game runs, every change is written immediately and
// tyrender\reload is created, so the DLL re-reads the ini on the fly (live balancing).
typedef struct { int ao, aoSlices, aoSteps, atmos, atmosSteps, tess, tessAll, tessLevel, post, aa, shScale, shTaps, gi, giRays, contact; } Settings;
#define NPRESET 4   // Low, Medium, High, Ultra ; no button selected = custom mix
static const wchar_t* kPresets[NPRESET] = { L"Low", L"Medium", L"High", L"Ultra" };
static const float kPresetHaze[NPRESET] = { 2.5f, 1.2f, 1.2f, 1.0f };
static const float kPresetGrade[NPRESET][2] = { { 1.00f, 1.03f }, { 0.94f, 1.18f }, { 0.94f, 1.18f }, { 0.94f, 1.18f } };   // brightness, saturation: Medium+ match the original game (measured vs vanilla A/B shots)   // [atmos] Intensity written by the preset
static const Settings kPresetVals[NPRESET] = {
    // ao sl st atm st  tes all lvl post aa shS taps gi rays contact     (measured on the merged build, RX 7600 XT, 2560x1440: stock 5.7 ms, Low 6.0, Medium 11.1, High 17.2, Ultra 32.1)
    {  0, 2, 4,  0, 12,  0, 0,  4,  1,  1,  1,  8,  0, 1, 0 },
    {  1, 3, 6,  1, 24,  1, 0,  8,  1,  2,  2, 16,  0, 1, 0 },
    {  1, 3, 6,  1, 24,  1, 0,  8,  1,  2,  2, 16,  1, 1, 1 },
    {  1, 4, 8,  1, 32,  1, 1, 24,  1,  2,  4, 32,  1, 2, 1 },
};
typedef struct { const wchar_t* sec; const wchar_t* key; const wchar_t* label; float mn, mx, def; const wchar_t* tip; HWND bar, val, tag, lab; } Slider;
static Slider g_sl[] = {
    { L"post",    L"Brightness",    L"Brightness",          0.70f, 1.50f, 0.94f, L"Turn the sun up or down. Sunglasses for the whole outback." },
    { L"post",    L"Contrast",      L"Contrast",            0.80f, 1.30f, 1.00f, L"Darks get darker, lights get lighter. Too much and even the kangaroos look dramatic." },
    { L"post",    L"Saturation",    L"Colour intensity",    0.60f, 1.60f, 1.18f, L"Colour volume. Low = faded old photograph, high = candy shop." },
    { L"ao",      L"Strength",      L"Crevice shadows",     0.00f, 1.50f, 0.60f, L"How dark the nooks and crannies get (needs Ambient occlusion on). Changing it costs nothing; the switch on the left pays the bill." },
    { L"ao",      L"WideStrength",  L"Corner darkness",     0.00f, 2.00f, 1.00f, L"Big-picture darkening where walls meet the floor, under rocks and in hollows, so corners finally look like corners (needs Ambient occlusion on)." },
    { L"shadows", L"Strength",      L"Shadow darkness",     0.00f, 1.00f, 0.50f, L"How dark shadows are. Zero = ghost shadows, one = film noir." },
    { L"shadows", L"GrassStrength", L"Shadows on grass",    0.00f, 1.00f, 0.35f, L"Shadow strength on grass blades. Keep it low or the lawn gets patchy." },
    { L"gi",      L"Strength",      L"Bounce light",        0.00f, 3.00f, 0.70f, L"How loudly light gossips between surfaces (needs Global illumination on)." },
    { L"fog",     L"Amount",        L"Fog amount",          0.00f, 3.00f, 1.00f, L"How much of the game's own fog you get. More fog = more mystery and less visibility." },
    { L"fog",     L"Distance",      L"Fog distance",        0.30f, 4.00f, 1.00f, L"How far you can see before the world dissolves into haze." },
    { L"atmos",   L"Intensity",     L"Distance haze",       0.00f, 4.00f, 1.20f, L"Dusty air that washes out far-away rocks and the painted mountains on the horizon. Lower = crisp postcard, higher = hot dusty afternoon (needs Volumetric fog on)." },
    { L"atmos",   L"SunIntensity",  L"Light shafts",        0.00f, 8.00f, 2.00f, L"Brightness of the sun beams cutting through the haze (needs Volumetric fog on)." },
    { L"atmos",   L"ContactShadows",L"Contact shadows",     0.00f, 1.00f, 0.00f, L"Tiny sharp shadows where things touch the ground (Ty's feet, fence posts). It is the one slider here that costs performance: zero is free, anything above that is not." },
    { L"tess",    L"ReliefDepth",   L"Surface relief",      0.00f, 10.00f, 3.00f, L"How deep cracks, grain and rope strands are carved. High values look rugged, very high values look like cardboard." },
    { L"post",    L"Sharpen",       L"Sharpening",          0.00f, 1.00f, 0.35f, L"Edge crispness. Too much and you can count the pixels, too little and it looks like a watercolour." },
};
#define NSL (int)(sizeof g_sl / sizeof g_sl[0])
#define SL_CONTACT 12   // index of the slider that has a performance cost

// ------------------------------------------------------------ performance cost model
// ms of GPU frame time added by each effect at 2560x1440, measured with the renderer's GPU timestamp queries on an AMD RX 7600 XT
// (stock engine = about 4.6 ms there). Fitted points: AO 7.4 (4 slices x 8 steps), GI 13.1 (3 rays) / 32.4 (8 rays), atmosphere 10.2 (48 steps, half-res),
// tessellation env relief 3.3 (level 8) / 5.6 (level 24), shadows 3.4 (4096) / 7.2 (8192) at 32 taps, TAA 0.8. PBR 0.0. See PERFORMANCE.md.
static float Rf(const wchar_t* sec, const wchar_t* key, float def);
enum { C_AO, C_GI, C_ATM, C_TESS, C_POST, C_SHADOW, C_N };
static float g_cost[C_N], g_costTotal, g_pixScale = 1.0f; static int g_resW = 2560, g_resH = 1440;
static float CostEstimate(const Settings* s, float* out) {
    // fitted to the merged build (default view, 2560x1440, RX 7600 XT, stock 5.4-5.7 ms): AO v2 +1.42 at 3x6; froxel fog +2.1, contact shadows +1.6; detail layer +0.3 (Scope=skin), world tessellation (Scope=all) ~+4.2;
    // shadows 4096/16 taps +1.06, 8192/32 taps ~+5.7; TAA +0.8; Hi-Z GI +4.7 for 1 ray (+0.8 per extra). Preset totals measured: Medium 6.8, Balanced 5.4, High 17.9, Ultra 26.4 ms over stock.
    float px = g_pixScale, hx = 0.5f + 0.5f * px, t = 0; float c[C_N];
    c[C_AO]     = s->ao ? (0.5f + 0.051f * (s->aoSlices * s->aoSteps)) * px : 0;
    c[C_GI]     = s->gi ? (4.7f + 0.8f * (s->giRays - 1)) * px : 0;
    c[C_ATM]    = s->atmos ? (2.1f + (s->contact ? 1.6f : 0.0f)) * px : (s->contact ? 1.6f * px : 0.0f);
    c[C_TESS]   = s->tess ? (s->tessAll ? 4.2f + 0.02f * (s->tessLevel - 8) : 0.3f) * hx : 0;
    c[C_POST]   = s->post ? (s->aa == 0 ? 0.3f : s->aa == 1 ? 0.5f : 0.8f) * px : 0;
    c[C_SHADOW] = (s->shScale == 4 ? 5.7f * (0.6f + 0.4f * s->shTaps / 32.0f) : s->shScale == 2 ? 1.06f * (0.6f + 0.4f * s->shTaps / 16.0f) : 0.6f) * hx;
    for (int i = 0; i < C_N; i++) { t += c[i]; if (out) out[i] = c[i]; }
    return t * (1.0f + 0.0075f * (t > 7.0f ? t - 7.0f : 0.0f));   // effects interact: High/Ultra measure 7-18% above the sum of single costs
}
static void W(const wchar_t* sec, const wchar_t* key, int v) { wchar_t b[32]; swprintf(b, 32, L"%d", v); WritePrivateProfileStringW(sec, key, b, g_ini); }
static void Wf(const wchar_t* sec, const wchar_t* key, float v) { wchar_t b[32]; swprintf(b, 32, L"%.3f", v); WritePrivateProfileStringW(sec, key, b, g_ini); }
static int R(const wchar_t* sec, const wchar_t* key, int def) { return GetPrivateProfileIntW(sec, key, def, g_ini); }
static float Rf(const wchar_t* sec, const wchar_t* key, float def) { wchar_t b[32], d[32]; swprintf(d, 32, L"%.3f", def); GetPrivateProfileStringW(sec, key, d, b, 32, g_ini); return (float)_wtof(b); }
static void SaveSettings(const Settings* s) {
    W(L"ao", L"Enable", s->ao); W(L"ao", L"Legacy", 0); W(L"ao", L"Slices", s->aoSlices); W(L"ao", L"Steps", s->aoSteps);
    W(L"atmos", L"Enable", s->atmos); W(L"atmos", L"Froxel", 1); W(L"atmos", L"Steps", s->atmosSteps);
    W(L"tess", L"Enable", s->tess); WritePrivateProfileStringW(L"tess", L"Scope", s->tessAll ? L"all" : L"skin", g_ini); W(L"tess", L"MaxLevel", s->tessLevel); W(L"tess", L"Debug", 0);
    W(L"post", L"Enable", s->post); W(L"post", L"AA", s->aa);
    W(L"shadows", L"Patch", 1); W(L"shadows", L"MapScale", s->shScale); W(L"shadows", L"Taps", s->shTaps);
    W(L"gi", L"Enable", s->gi); W(L"gi", L"Mode", 1); W(L"gi", L"Rays", s->giRays);
    W(L"debug", L"CaptureFrame", 0); W(L"debug", L"CaptureFrame2", 0);
}
static void LoadSettings(Settings* s) {
    wchar_t b[16];
    s->ao = R(L"ao", L"Enable", 1); s->aoSlices = R(L"ao", L"Slices", 3); s->aoSteps = R(L"ao", L"Steps", 6);
    s->atmos = R(L"atmos", L"Enable", 1); s->atmosSteps = R(L"atmos", L"Steps", 24);
    s->tess = R(L"tess", L"Enable", 1); GetPrivateProfileStringW(L"tess", L"Scope", L"skin", b, 16, g_ini); s->tessAll = !_wcsicmp(b, L"all"); s->tessLevel = R(L"tess", L"MaxLevel", 8);
    s->post = R(L"post", L"Enable", 1); s->aa = R(L"post", L"AA", 2);
    s->shScale = R(L"shadows", L"MapScale", 2); s->shTaps = R(L"shadows", L"Taps", 16);
    s->gi = R(L"gi", L"Enable", 0); s->giRays = R(L"gi", L"Rays", 1); s->contact = Rf(L"atmos", L"ContactShadows", 0.0f) > 0.0f;
}

// The game's own settings.ini (Documents\TY the Tasmanian Tiger) uses "Key = value"; rewrite values in place, keeping the format.
static void GameSettingsPath(wchar_t* out) { wchar_t docs[MAX_PATH] = L""; SHGetFolderPathW(NULL, 0x0005 /*CSIDL_PERSONAL*/, NULL, 0, docs); swprintf(out, MAX_PATH, L"%ls\\TY the Tasmanian Tiger\\settings.ini", docs); }
static void SetGameDetail(int high) {
    wchar_t path[MAX_PATH]; GameSettingsPath(path);
    FILE* f = _wfopen(path, L"rb"); if (!f) return;
    static char buf[16384]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    wchar_t bak[MAX_PATH + 8]; swprintf(bak, MAX_PATH + 8, L"%ls.tyhd_backup", path);
    if (GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) CopyFileW(path, bak, TRUE);       // keep the user's original once
    static const char* keys[] = { "DetailLevel", "Props", "PropDetailLevel", "Grass", "Anisotropic", "Detail", "ReflectionDetail" };
    static char out[20000]; out[0] = 0; char* line = strtok(buf, "\n");
    while (line) {
        char l2[512]; strncpy(l2, line, 511); l2[511] = 0; size_t L = strlen(l2); if (L && l2[L - 1] == '\r') l2[--L] = 0;
        char key[64] = ""; sscanf(l2, " %63[^= ]", key);
        int hit = 0; for (int i = 0; i < 7; i++) if (!strcmp(key, keys[i])) hit = 1;
        if (hit) { char nl[128]; snprintf(nl, sizeof nl, "%s = %s", key, high ? "high" : (!strcmp(key, "Props") || !strcmp(key, "Anisotropic") || !strcmp(key, "ReflectionDetail")) ? "low" : "mid"); strcat(out, nl); }
        else strcat(out, l2);
        strcat(out, "\r\n"); line = strtok(NULL, "\n");
    }
    f = _wfopen(path, L"wb"); if (f) { fwrite(out, 1, strlen(out), f); fclose(f); }
}
static int GameDetailIsHigh(void) { wchar_t path[MAX_PATH]; GameSettingsPath(path); FILE* f = _wfopen(path, L"rb"); if (!f) return 0; char b[16384]; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0; return strstr(b, "Props = high") != NULL; }

// ------------------------------------------------------------ clash guard (issue #13)
// tyrender.ini is the single source of truth. The game's own settings.ini has SSAO and MSAA switches of its own that duplicate
// what tyrender does (the DLL's AO, its Edge smoothing). This only warns, in the status line; nothing is rewritten.
// Rules mirror CheckGameSettingsClash in tyrender.cpp (which logs the same warning). Config tyrender.ini [clash] Check=0 turns it off.
static int GameDisplayValue(const char* key, char* val, int vn) {   // [Display] key of the game's settings.ini; 0 = not found
    wchar_t path[MAX_PATH]; GameSettingsPath(path); FILE* f = _wfopen(path, L"rb"); if (!f) return 0;
    static char b[16384]; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    int in = 0, found = 0;
    for (char* p = b; *p && !found; ) {
        char* e = strchr(p, '\n'); size_t L = e ? (size_t)(e - p) : strlen(p);
        char line[256]; if (L > 255) L = 255; memcpy(line, p, L); line[L] = 0;
        char* q = line; while (*q == ' ' || *q == '\t') q++;
        if (*q == '[') in = !_strnicmp(q, "[Display]", 9);
        else if (in) { char k[64] = "", v[64] = ""; if (sscanf(q, " %63[^= ] = %63s", k, v) == 2 && !_stricmp(k, key)) { strncpy(val, v, vn - 1); val[vn - 1] = 0; found = 1; } }
        p = e ? e + 1 : p + L;
    }
    return found;
}
// Warning text for the status line; empty when the game and tyrender agree or the check is off.
static void GameClashText(wchar_t* out, int n) {
    out[0] = 0; if (!GetPrivateProfileIntW(L"clash", L"Check", 1, g_ini)) return;
    char v[64] = "";
    int ssao = GameDisplayValue("SSAO", v, sizeof v) && !_stricmp(v, "true");
    int msaa = GameDisplayValue("MSAA", v, sizeof v) && atoi(v) >= 2;   // "off", "2x", "4x", "8x"
    int aa = R(L"post", L"AA", 2);                                      // tyrender Edge smoothing: 0 off, 1 FXAA, 2 TAA, 3 TAA + FXAA
    if (!ssao && !(msaa && aa != 0)) return;
    swprintf(out, n, L"Settings clash with the game's settings.ini: %ls%ls%ls. Set them there (Display section).",
        ssao ? L"SSAO is on (AO runs twice)" : L"", (ssao && msaa && aa) ? L" and " : L"", (msaa && aa) ? L"MSAA is on (edges smoothed twice)" : L"");
}

// ------------------------------------------------------------ window
enum { ID_P0 = 100, ID_AO = 110, ID_ATMOS, ID_TESS, ID_TESSALL, ID_POST, ID_GI, ID_AA, ID_SHADOW, ID_DETAIL, ID_PLAY = 130, ID_STOCK, ID_STEAM, ID_ADV, ID_RESET, ID_MORE, ID_TAB = 136, ID_REPORT = 137, ID_SL0 = 300 };
enum { TG_AO, TG_GI, TG_ATM, TG_TESS, TG_TESSALL, TG_POST, TG_AA, TG_SHADOW, TG_DETAIL, TG_N };
static HWND g_hw, h_preset[NPRESET], h_presetNote, h_ao, h_atmos, h_tess, h_tessAll, h_post, h_gi, h_aa, h_shadow, h_detail, h_status, h_total, h_overall, h_dTitle, h_dText;
static Settings g_s; static int g_updating, g_presetSel = -1, g_expanded;
static HWND h_aaLab, h_shLab; static HWND g_tagH[TG_N]; static int g_tagLv[TG_N], g_slLv[64], g_overallLv;
static HFONT g_font, g_fontB, g_fontT, g_fontBig;
// Controls are grouped into pages: five tabs (shown in the expanded view, one at a time), the expanded-only extras, and PG_ANY = always shown.
enum { PG_T0, PG_T1, PG_T2, PG_T3, PG_T4, PG_EXP, PG_N, PG_ANY = -1 };
static HWND g_page[PG_N][64]; static int g_np[PG_N], g_pg = PG_ANY, g_tab;
static HWND h_play, h_stock, h_more, h_tab;
typedef struct { HWND h; const wchar_t* title; const wchar_t* text; const int* lv; } Desc;
static Desc g_desc[160]; static int g_nd, g_descCur = -1;
static const wchar_t* kLvName[4] = { L"Free", L"Low", L"Medium", L"High" };
static const wchar_t* kTotName[4] = { L"LOW", L"MEDIUM", L"HIGH", L"EXTREME" };
static COLORREF LvColor(int lv) { return lv == 0 ? RGB(105, 105, 105) : lv == 1 ? RGB(0, 135, 0) : lv == 2 ? RGB(205, 110, 0) : RGB(200, 0, 0); }
static int Lvl(float ms) { return ms < 0.2f ? 0 : ms < 1.5f ? 1 : ms < 3.5f ? 2 : 3; }   // thresholds on the measured GPU frame-time cost
static int TotLvl(float ms) { return ms < 1.5f ? 0 : ms < 7.0f ? 1 : ms < 14.0f ? 2 : 3; }

static void AddDesc(HWND h, const wchar_t* title, const wchar_t* text) { if (g_nd < 160) { g_desc[g_nd].h = h; g_desc[g_nd].title = title; g_desc[g_nd].text = text; g_desc[g_nd].lv = NULL; g_nd++; } }
static void ShowDescIdx(int i) {
    wchar_t b[1500]; SetWindowTextW(h_dTitle, g_desc[i].title);
    if (g_desc[i].lv) swprintf(b, 1500, L"%ls\n\nPerformance impact: %ls", g_desc[i].text, kLvName[*g_desc[i].lv]); else wcsncpy(b, g_desc[i].text, 1499);
    b[1499] = 0; SetWindowTextW(h_dText, b);
}
static void ShowDesc(HWND w) {
    for (int i = 0; i < g_nd; i++) if (g_desc[i].h == w) { if (g_descCur != i) { g_descCur = i; ShowDescIdx(i); } return; }
}
static void HoverUpdate(void) {   // rectangle test (not WindowFromPoint: that skips disabled controls)
    POINT p; if (!GetCursorPos(&p)) return; RECT wr; GetWindowRect(g_hw, &wr); if (!PtInRect(&wr, p)) return;
    for (int i = 0; i < g_nd; i++) { RECT r; if (IsWindowVisible(g_desc[i].h) && GetWindowRect(g_desc[i].h, &r) && PtInRect(&r, p)) { ShowDesc(g_desc[i].h); return; } }
}

static int GameRunning(void) { return FindWindowW(NULL, L"TY the Tasmanian Tiger") != NULL; }
static void LiveReload(void) {   // tell a running tyrender to re-read the ini
    wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%lstyrender\\reload", g_dir);
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
static void UpdateStatus(const wchar_t* extra) {
    wchar_t exe[MAX_PATH], m[400], clash[300]; swprintf(exe, MAX_PATH, L"%lsTY_tyrender.exe", g_dir);
    int patched = GetFileAttributesW(exe) != INVALID_FILE_ATTRIBUTES;
    GameClashText(clash, 300);   // re-read every timer tick, so a change in the game's settings.ini shows up within 2 s
    swprintf(m, 400, L"%ls%ls", GameRunning() ? L"Game running: sliders apply live. " : L"",
        extra ? extra : clash[0] ? clash : patched ? L"Ready." : L"Ready to play. The first launch sets itself up; your original game files are never changed.");
    SetWindowTextW(h_status, m);
}
static void SliderText(int i) {
    int pos = (int)SendMessageW(g_sl[i].bar, TBM_GETPOS, 0, 0); float v = g_sl[i].mn + (g_sl[i].mx - g_sl[i].mn) * pos / 100.0f;
    wchar_t b[32]; swprintf(b, 32, L"%.2f", v); SetWindowTextW(g_sl[i].val, b);
}
static void SliderToIni(int i) {
    int pos = (int)SendMessageW(g_sl[i].bar, TBM_GETPOS, 0, 0); Wf(g_sl[i].sec, g_sl[i].key, g_sl[i].mn + (g_sl[i].mx - g_sl[i].mn) * pos / 100.0f);
}
static void SlidersFromIni(void) {
    for (int i = 0; i < NSL; i++) { float v = Rf(g_sl[i].sec, g_sl[i].key, g_sl[i].def); int pos = (int)((v - g_sl[i].mn) / (g_sl[i].mx - g_sl[i].mn) * 100.0f + 0.5f);
        if (pos < 0) pos = 0; if (pos > 100) pos = 100; SendMessageW(g_sl[i].bar, TBM_SETPOS, TRUE, pos); SliderText(i); }
}
static void ToUI(void) {
    g_updating = 1;
    SendMessageW(h_ao, BM_SETCHECK, g_s.ao, 0); SendMessageW(h_atmos, BM_SETCHECK, g_s.atmos, 0); SendMessageW(h_tess, BM_SETCHECK, g_s.tess, 0);
    SendMessageW(h_tessAll, BM_SETCHECK, g_s.tessAll, 0); SendMessageW(h_post, BM_SETCHECK, g_s.post, 0); SendMessageW(h_gi, BM_SETCHECK, g_s.gi, 0);
    SendMessageW(h_aa, CB_SETCURSEL, g_s.aa, 0); SendMessageW(h_shadow, CB_SETCURSEL, g_s.shScale == 4 ? 2 : g_s.shScale == 2 ? 1 : 0, 0);
    g_updating = 0;
}
static void FromUI(void) {
    g_s.ao = (int)SendMessageW(h_ao, BM_GETCHECK, 0, 0); g_s.atmos = (int)SendMessageW(h_atmos, BM_GETCHECK, 0, 0); g_s.tess = (int)SendMessageW(h_tess, BM_GETCHECK, 0, 0);
    g_s.tessAll = (int)SendMessageW(h_tessAll, BM_GETCHECK, 0, 0); g_s.post = (int)SendMessageW(h_post, BM_GETCHECK, 0, 0); g_s.gi = (int)SendMessageW(h_gi, BM_GETCHECK, 0, 0);
    g_s.aa = (int)SendMessageW(h_aa, CB_GETCURSEL, 0, 0); int sh = (int)SendMessageW(h_shadow, CB_GETCURSEL, 0, 0); g_s.shScale = sh == 2 ? 4 : sh == 1 ? 2 : 1;
}
static void GameResolution(void) {
    wchar_t path[MAX_PATH]; GameSettingsPath(path); FILE* f = _wfopen(path, L"rb"); if (!f) return;
    char b[16384]; size_t n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    int full = strstr(b, "Fullscreen = true") != NULL, w = 0, h = 0; const char* kw = full ? "FullscreenWidth" : "WindowWidth"; const char* kh = full ? "FullscreenHeight" : "WindowHeight";
    char* p = strstr(b, kw); if (p) sscanf(p + strlen(kw), " = %d", &w); p = strstr(b, kh); if (p) sscanf(p + strlen(kh), " = %d", &h);
    if (w > 200 && h > 200) { g_resW = w; g_resH = h; }
    g_pixScale = (float)g_resW * g_resH / (2560.0f * 1440.0f);
}
static int MatchPreset(void) {
    for (int p = 0; p < NPRESET; p++) { const Settings* k = &kPresetVals[p];
        if (g_s.ao == k->ao && g_s.aoSlices == k->aoSlices && g_s.aoSteps == k->aoSteps && g_s.atmos == k->atmos && g_s.atmosSteps == k->atmosSteps && g_s.tess == k->tess &&
            g_s.tessAll == k->tessAll && g_s.tessLevel == k->tessLevel && g_s.post == k->post && g_s.aa == k->aa && g_s.shScale == k->shScale && g_s.shTaps == k->shTaps &&
            g_s.gi == k->gi && g_s.giRays == k->giRays && g_s.contact == k->contact) return p; }
    return -1;
}
static const wchar_t* kPresetNote[NPRESET] = {
    L"Low: the original look with a light polish. For old laptops and integrated graphics.",
    L"Medium (recommended): the sweet spot. Smooth on most mid-range graphics cards.",
    L"High: adds bounce light and contact shadows. Wants a decent graphics card.",
    L"Ultra: everything, as sharp as it goes. Wants a strong graphics card." };
static void RefreshCosts(void) {
    g_s.contact = Rf(L"atmos", L"ContactShadows", 0.0f) > 0.0f;   // follows the Contact shadows slider
    g_costTotal = CostEstimate(&g_s, g_cost);
    // what each option WOULD cost if it were on (so the tag is useful before ticking the box)
    Settings t = g_s; float c[C_N]; t.ao = t.gi = t.atmos = t.tess = t.post = 1; t.contact = 0; t.tessAll = 0; CostEstimate(&t, c);
    g_tagLv[TG_AO] = Lvl(c[C_AO]); g_tagLv[TG_GI] = Lvl(c[C_GI]); g_tagLv[TG_ATM] = Lvl(c[C_ATM]); g_tagLv[TG_TESS] = Lvl(c[C_TESS]); g_tagLv[TG_POST] = Lvl(0.3f * g_pixScale);
    g_tagLv[TG_SHADOW] = Lvl(c[C_SHADOW]); g_tagLv[TG_AA] = Lvl(g_s.aa == 0 ? 0.0f : g_s.aa == 1 ? 0.5f * g_pixScale : 0.8f * g_pixScale); g_tagLv[TG_DETAIL] = 0;
    t.tessAll = 1; CostEstimate(&t, c); g_tagLv[TG_TESSALL] = Lvl(c[C_TESS]);
    for (int i = 0; i < NSL; i++) g_slLv[i] = 0;
    { int pos = (int)SendMessageW(g_sl[SL_CONTACT].bar, TBM_GETPOS, 0, 0); g_slLv[SL_CONTACT] = pos > 0 ? Lvl(1.6f * g_pixScale) : 0; }
    for (int i = 0; i < TG_N; i++) if (g_tagH[i]) { SetWindowTextW(g_tagH[i], i == TG_DETAIL ? L"Varies" : kLvName[g_tagLv[i]]); InvalidateRect(g_tagH[i], NULL, TRUE); }
    for (int i = 0; i < NSL; i++) if (g_sl[i].tag) { SetWindowTextW(g_sl[i].tag, kLvName[g_slLv[i]]); InvalidateRect(g_sl[i].tag, NULL, TRUE); }
    g_overallLv = TotLvl(g_costTotal);
    { static const int kDep[] = { 5, 5, 5, 1, 1, 0, 0, 2, 0, 0, 3, 3, 0, 4, 5 };   // slider -> effect it needs: 1 AO, 2 GI, 3 fog, 4 surface detail, 5 post; 0 = always live
      int on[6] = { 1, g_s.ao, g_s.gi, g_s.atmos, g_s.tess, g_s.post };
      for (int i = 0; i < NSL; i++) { BOOL en = on[kDep[i]] ? TRUE : FALSE; EnableWindow(g_sl[i].bar, en); EnableWindow(g_sl[i].lab, en); EnableWindow(g_sl[i].val, en); EnableWindow(g_sl[i].tag, en); } }
    wchar_t m[200]; swprintf(m, 200, L"Overall performance impact:  %ls", kTotName[g_overallLv]); SetWindowTextW(h_overall, m); InvalidateRect(h_overall, NULL, TRUE);
    swprintf(m, 200, L"How much this slows the game compared with the original. A faster graphics card or a lower screen resolution (now %dx%d) makes it lighter.", g_resW, g_resH);
    SetWindowTextW(h_total, m);
    int mp = MatchPreset();
    g_presetSel = mp; for (int i = 0; i < NPRESET; i++) InvalidateRect(h_preset[i], NULL, TRUE);
    if (GetFileAttributesW(g_ini) != INVALID_FILE_ATTRIBUTES) WritePrivateProfileStringW(L"shots", L"Preset", mp >= 0 ? kPresets[mp] : L"Custom", g_ini);   // names the A/B screenshots (abshot.inl)
    SetWindowTextW(h_presetNote, mp >= 0 ? kPresetNote[mp] : L"Custom mix: you changed something. Click a preset to snap back.");
    if (g_descCur >= 0) ShowDescIdx(g_descCur);
}
static void SaveAll(void) { FromUI(); RefreshCosts(); SaveSettings(&g_s); for (int i = 0; i < NSL; i++) SliderToIni(i); if (GameRunning()) LiveReload(); }
static HWND Ctl(const wchar_t* cls, const wchar_t* txt, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowW(cls, txt, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_hw, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (g_pg >= 0 && g_np[g_pg] < 64) g_page[g_pg][g_np[g_pg]++] = c;
    return c;
}
static HWND Label(const wchar_t* txt, int x, int y, int w, int h) { return Ctl(L"STATIC", txt, SS_NOTIFY, x, y, w, h, 0); }
static HWND Tag(int idx, int x, int y) { g_tagH[idx] = Ctl(L"STATIC", L"", SS_CENTER | SS_NOTIFY, x, y, 66, 18, 0); SendMessageW(g_tagH[idx], WM_SETFONT, (WPARAM)g_fontB, TRUE); return g_tagH[idx]; }
static int EnsurePatched(void) {
    wchar_t in[MAX_PATH], out[MAX_PATH], err[300] = L""; swprintf(in, MAX_PATH, L"%lsTY.exe", g_dir); swprintf(out, MAX_PATH, L"%lsTY_tyrender.exe", g_dir);
    if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) return 1;
    if (PatchExe(in, out, err, 300)) return 1;
    MessageBoxW(g_hw, err, L"TYLauncher - could not create TY_tyrender.exe (will try DLL injection)", MB_ICONWARNING); return 0;
}
static void AssignLv(void) {   // the description panel shows the live impact level of the control under the mouse
    for (int i = 0; i < g_nd; i++) { HWND h = g_desc[i].h; const int* lv = NULL;
        if (h == h_ao || h == g_tagH[TG_AO]) lv = &g_tagLv[TG_AO]; else if (h == h_gi || h == g_tagH[TG_GI]) lv = &g_tagLv[TG_GI]; else if (h == h_atmos || h == g_tagH[TG_ATM]) lv = &g_tagLv[TG_ATM];
        else if (h == h_tess || h == g_tagH[TG_TESS]) lv = &g_tagLv[TG_TESS]; else if (h == h_tessAll || h == g_tagH[TG_TESSALL]) lv = &g_tagLv[TG_TESSALL]; else if (h == h_post || h == g_tagH[TG_POST]) lv = &g_tagLv[TG_POST];
        else if (h == h_aa || h == h_aaLab || h == g_tagH[TG_AA]) lv = &g_tagLv[TG_AA]; else if (h == h_shadow || h == h_shLab || h == g_tagH[TG_SHADOW]) lv = &g_tagLv[TG_SHADOW];
        else for (int s = 0; s < NSL; s++) if (h == g_sl[s].lab || h == g_sl[s].bar || h == g_sl[s].val || h == g_sl[s].tag) lv = &g_slLv[s];
        g_desc[i].lv = lv; }
}
static const wchar_t* kPresetTitle[NPRESET] = { L"Low - potato mode", L"Medium - the sweet spot", L"High - look at me", L"Ultra - because we can" };
static const wchar_t* kPresetText[NPRESET] = {
    L"The original game with a light polish: gentle edge smoothing and cleaner shadows. No fancy lighting, so it runs happily on old laptops and integrated graphics.\n\nPerformance impact: LOW.",
    L"The outback as it was meant to be seen: crevice shadows, dusty volumetric haze with sun beams, carved cracks and grain on the ground, soft sharp shadows and smooth edges.\n\nThe one we recommend. Performance impact: MEDIUM.",
    L"Everything in Medium plus bounce light (red rocks tint the sand beside them) and tiny contact shadows under Ty's feet. Pretty, hungry and not for shy graphics cards.\n\nPerformance impact: HIGH.",
    L"Maximum everything: razor-sharp shadows, thick haze, extra occlusion and bounce rays, real bumpy geometry on the ground. Bring a strong graphics card and a fan that believes in you.\n\nPerformance impact: EXTREME." };
static void AddDesc2(HWND a, HWND b, const wchar_t* title, const wchar_t* text) { AddDesc(a, title, text); AddDesc(b, title, text); }
// Simple view: presets, impact, Play and the Customise button. Expanded view: compact presets, tabs and the help panel.
static void ApplyLayout(void) {
    int exp = g_expanded;
    for (int i = 0; i < NPRESET; i++) MoveWindow(h_preset[i], exp ? 10 + i * 148 : 16 + i * 114, exp ? 10 : 16, exp ? 140 : 108, exp ? 30 : 40, TRUE);
    MoveWindow(h_presetNote, 16, exp ? 46 : 62, exp ? 568 : 448, exp ? 20 : 34, TRUE);
    MoveWindow(h_overall, 16, exp ? 70 : 106, exp ? 568 : 448, 26, TRUE);
    MoveWindow(h_play, 16, exp ? 512 : 144, exp ? 200 : 226, exp ? 36 : 46, TRUE);
    MoveWindow(h_stock, exp ? 224 : 250, exp ? 512 : 144, exp ? 170 : 214, exp ? 36 : 46, TRUE);
    MoveWindow(h_more, exp ? 402 : 16, exp ? 512 : 204, exp ? 190 : 200, exp ? 36 : 28, TRUE);
    SetWindowTextW(h_more, exp ? L"<<  Simpler view" : L"Customise settings  >>");
    MoveWindow(h_status, 16, exp ? 552 : 250, exp ? 568 : 448, exp ? 40 : 36, TRUE);
    for (int p = 0; p < PG_N; p++) for (int k = 0; k < g_np[p]; k++)
        ShowWindow(g_page[p][k], (p == PG_EXP ? exp : exp && p == g_tab) ? SW_SHOW : SW_HIDE);
    ShowWindow(h_tab, exp ? SW_SHOW : SW_HIDE);
    RECT r = { 0, 0, exp ? 600 : 480, exp ? 596 : 300 };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    SetWindowPos(g_hw, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
}
// ------------------------------------------------------------ gamepad (XInput)
// A controller drives the same controls as the mouse and keyboard, so Steam Big Picture and Steam Deck work without either.
// D-pad or left stick: move the focus to the nearest control in that direction (Left/Right change a slider or combo box instead).
// A: press the focused control. B: close a dropdown, or go back to the simple view. LB/RB: switch tabs in the expanded view.
// Mouse and keyboard handling is untouched. Focus cues appear only after the first pad press. XInput is loaded at run time,
// so the build needs no extra library. Steam Input presents its pads as XInput, so this covers Steam's controller setup too.
typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } GpPad;
typedef struct { DWORD dwPacketNumber; GpPad Gamepad; } GpState;
typedef DWORD (WINAPI* GpGetStateFn)(DWORD, GpState*);
enum { GP_UP, GP_DOWN, GP_LEFT, GP_RIGHT, GP_N };
#define GP_BTN_A 0x1000
#define GP_BTN_B 0x2000
#define GP_BTN_LB 0x0100
#define GP_BTN_RB 0x0200
static GpGetStateFn g_gpGet; static int g_gpTried, g_gpSeen, g_gpDirOn[GP_N], g_gpRep[GP_N]; static WORD g_gpLast;
static HWND g_gpFrom, g_gpBest; static int g_gpDir, g_gpFx, g_gpFy; static long g_gpScore;
static void GpCentre(HWND c, int* x, int* y) {   // screen coordinates; the tab strip stands for the whole tab control
    RECT r; GetWindowRect(c, &r); *x = (r.left + r.right) / 2; *y = c == h_tab ? r.top + 10 : (r.top + r.bottom) / 2;
}
static BOOL CALLBACK GpCand(HWND c, LPARAM lp) {
    (void)lp;
    if (c == g_gpFrom || !IsWindowVisible(c) || !IsWindowEnabled(c) || !(GetWindowLongW(c, GWL_STYLE) & WS_TABSTOP)) return TRUE;
    int x, y; GpCentre(c, &x, &y);
    int dx = x - g_gpFx, dy = y - g_gpFy, p, s;   // p = distance in the wanted direction, s = sideways offset
    switch (g_gpDir) {
    case GP_UP:   p = -dy; s = abs(dx); break;
    case GP_DOWN: p = dy;  s = abs(dx); break;
    case GP_LEFT: p = -dx; s = abs(dy); break;
    default:      p = dx;  s = abs(dy); break;
    }
    if (p <= 0) return TRUE;
    long score = p + 2L * s;
    if (score < g_gpScore) { g_gpScore = score; g_gpBest = c; }
    return TRUE;
}
static HWND GpNext(HWND from, int dir) {
    g_gpFrom = from; g_gpDir = dir; g_gpBest = NULL; g_gpScore = 0x7fffffffL;
    GpCentre(from, &g_gpFx, &g_gpFy); EnumChildWindows(g_hw, GpCand, 0); return g_gpBest;
}
static void GpFocusTo(HWND c) {
    SetFocus(c); ShowDesc(c);
    for (int i = 0; i < NSL; i++) InvalidateRect(g_sl[i].lab, NULL, TRUE);   // the focused slider's name is drawn in the accent colour
}
static HWND GpCurrent(void) {   // the focused control; with nothing focused yet, the first press only picks a starting point
    HWND f = GetFocus();
    if (f && IsChild(g_hw, f) && IsWindowVisible(f)) return f;
    GpFocusTo(g_expanded ? h_tab : h_preset[0]); return NULL;
}
static void GpTab(int delta) {
    if (!g_expanded) return;
    int n = (int)SendMessageW(h_tab, TCM_GETITEMCOUNT, 0, 0);
    g_tab = (g_tab + delta + n) % n; SendMessageW(h_tab, TCM_SETCURSEL, g_tab, 0); ApplyLayout();
    HWND f = GetFocus(); if (!f || !IsWindowVisible(f)) GpFocusTo(h_tab);
}
static void GpMove(int dir) {
    HWND f = GpCurrent(); if (!f) return;
    wchar_t cls[32] = L""; GetClassNameW(f, cls, 32);
    if (!wcscmp(cls, TRACKBAR_CLASSW) && (dir == GP_LEFT || dir == GP_RIGHT)) { SendMessageW(f, WM_KEYDOWN, dir == GP_LEFT ? VK_LEFT : VK_RIGHT, 0); return; }
    if (!wcscmp(cls, L"ComboBox") && (dir == GP_LEFT || dir == GP_RIGHT || SendMessageW(f, CB_GETDROPPEDSTATE, 0, 0))) {
        int n = (int)SendMessageW(f, CB_GETCOUNT, 0, 0), cur = (int)SendMessageW(f, CB_GETCURSEL, 0, 0);
        int nx = cur + (dir == GP_RIGHT || dir == GP_DOWN ? 1 : -1);
        if (nx >= 0 && nx < n) { SendMessageW(f, CB_SETCURSEL, nx, 0); SendMessageW(g_hw, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(f), CBN_SELCHANGE), (LPARAM)f); }
        return;
    }
    if (f == h_tab && (dir == GP_LEFT || dir == GP_RIGHT)) { GpTab(dir == GP_RIGHT ? 1 : -1); return; }
    HWND n = GpNext(f, dir); if (n) GpFocusTo(n);
}
static void GpActivate(void) {
    HWND f = GpCurrent(); if (!f) return;
    wchar_t cls[32] = L""; GetClassNameW(f, cls, 32);
    if (!wcscmp(cls, L"Button")) SendMessageW(f, BM_CLICK, 0, 0);
    else if (!wcscmp(cls, L"ComboBox")) SendMessageW(f, CB_SHOWDROPDOWN, !SendMessageW(f, CB_GETDROPPEDSTATE, 0, 0), 0);
    else if (f == h_tab) GpMove(GP_DOWN);
}
static void GpBack(void) {
    HWND f = GetFocus(); wchar_t cls[32] = L""; if (f) GetClassNameW(f, cls, 32);
    if (!wcscmp(cls, L"ComboBox") && SendMessageW(f, CB_GETDROPPEDSTATE, 0, 0)) { SendMessageW(f, CB_SHOWDROPDOWN, FALSE, 0); return; }
    if (g_expanded) { SendMessageW(h_more, BM_CLICK, 0, 0); GpFocusTo(h_more); }
}
// Called every 30 ms by the timer. Pad input is ignored while another window (the game) is in front.
static void GamepadTick(void) {
    if (!g_gpTried) { HMODULE x; g_gpTried = 1; x = LoadLibraryW(L"xinput1_4.dll"); if (!x) x = LoadLibraryW(L"xinput1_3.dll");
        if (x) g_gpGet = (GpGetStateFn)GetProcAddress(x, "XInputGetState"); }
    GpState st;
    if (!g_gpGet || g_gpGet(0, &st) != ERROR_SUCCESS || GetForegroundWindow() != g_hw) { g_gpLast = 0; memset(g_gpDirOn, 0, sizeof g_gpDirOn); return; }
    WORD b = st.Gamepad.wButtons, fresh = b & ~g_gpLast; g_gpLast = b;
    int on[GP_N];
    on[GP_UP] = (b & 0x0001) || st.Gamepad.sThumbLY > 12000;
    on[GP_DOWN] = (b & 0x0002) || st.Gamepad.sThumbLY < -12000;
    on[GP_LEFT] = (b & 0x0004) || st.Gamepad.sThumbLX < -12000;
    on[GP_RIGHT] = (b & 0x0008) || st.Gamepad.sThumbLX > 12000;
    int any = fresh != 0; for (int d = 0; d < GP_N; d++) any |= on[d];
    if (any && !g_gpSeen) { g_gpSeen = 1; SendMessageW(g_hw, WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0); }   // show focus cues from now on
    for (int d = 0; d < GP_N; d++) {   // a held direction repeats: first after about 0.4 s, then about 8 times a second
        if (!on[d]) { g_gpDirOn[d] = 0; continue; }
        if (!g_gpDirOn[d]) { g_gpDirOn[d] = 1; g_gpRep[d] = 14; GpMove(d); }
        else if (--g_gpRep[d] <= 0) { g_gpRep[d] = 4; GpMove(d); }
    }
    if (fresh & GP_BTN_A) GpActivate();
    if (fresh & GP_BTN_B) GpBack();
    if (fresh & GP_BTN_LB) GpTab(-1);
    if (fresh & GP_BTN_RB) GpTab(1);
}
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hw = h; GameResolution();
        g_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontB = CreateFontW(-13, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontT = CreateFontW(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontBig = CreateFontW(-15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HWND c, l; int y;
        // ---- always shown: presets, impact and the main actions. ApplyLayout places them for the simple or the expanded view.
        g_pg = PG_ANY;
        for (int i = 0; i < NPRESET; i++) {
            h_preset[i] = Ctl(L"BUTTON", kPresets[i], BS_OWNERDRAW | (i == 0 ? WS_GROUP : 0) | WS_TABSTOP, 16 + i * 114, 16, 108, 40, ID_P0 + i);
            SendMessageW(h_preset[i], WM_SETFONT, (WPARAM)g_fontBig, TRUE); AddDesc(h_preset[i], kPresetTitle[i], kPresetText[i]);
        }
        h_presetNote = Label(L"", 16, 62, 448, 34);
        h_overall = Ctl(L"STATIC", L"", SS_LEFT | SS_NOTIFY, 16, 106, 448, 26, 0); SendMessageW(h_overall, WM_SETFONT, (WPARAM)g_fontT, TRUE);
        AddDesc(h_overall, L"Overall performance impact", L"A quick summary of everything that is switched on: LOW, MEDIUM, HIGH or EXTREME. It is based on measured frame-time costs, so it is a guide: your graphics card and screen resolution decide the real result.\n\nIf the game feels slow, switch off the effects tagged High first.");
        h_play = Ctl(L"BUTTON", L"Play with HD renderer", BS_DEFPUSHBUTTON | WS_TABSTOP, 16, 144, 226, 46, ID_PLAY); SendMessageW(h_play, WM_SETFONT, (WPARAM)g_fontBig, TRUE);
        AddDesc(h_play, L"Play with HD renderer", L"Starts the game with the renderer and your current settings. Keep this window open while you play: every slider changes the picture live.");
        h_stock = Ctl(L"BUTTON", L"Play original (no mod)", WS_TABSTOP, 250, 144, 214, 46, ID_STOCK); AddDesc(h_stock, L"Play original", L"Starts the untouched original game. Handy for comparing, or for when you just want to feel nostalgic.");
        h_more = Ctl(L"BUTTON", L"Customise settings  >>", WS_TABSTOP, 16, 204, 200, 28, ID_MORE); AddDesc(h_more, L"Customise", L"Opens the effects and look controls in tabs, plus the advanced tools. You do not need any of it: the presets already pick good settings.");
        h_status = Ctl(L"STATIC", L"", 0, 16, 250, 448, 36, 0);   // 2 lines: the clash warning can be long
        // ---- expanded view only: impact detail, tabs and the help panel
        g_pg = PG_EXP;
        h_total = Ctl(L"STATIC", L"", SS_NOTIFY, 16, 96, 568, 36, 0);
        AddDesc(h_total, L"Overall performance impact", L"A quick summary of everything that is switched on: LOW, MEDIUM, HIGH or EXTREME. It is based on measured frame-time costs, so it is a guide: your graphics card and screen resolution decide the real result.\n\nIf the game feels slow, switch off the effects tagged High first.");
        h_tab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 136, 580, 290, g_hw, (HMENU)(INT_PTR)ID_TAB, NULL, NULL);
        SendMessageW(h_tab, WM_SETFONT, (WPARAM)g_font, TRUE);
        static const wchar_t* kTabName[] = { L"Effects", L"Light & shadow", L"Air & fog", L"Picture", L"Advanced" };
        for (int t = 0; t < 5; t++) { TCITEMW ti = { TCIF_TEXT }; ti.pszText = (LPWSTR)kTabName[t]; SendMessageW(h_tab, TCM_INSERTITEMW, t, (LPARAM)&ti); }
        h_dTitle = Ctl(L"STATIC", L"", SS_LEFT, 16, 432, 568, 24, 0); SendMessageW(h_dTitle, WM_SETFONT, (WPARAM)g_fontT, TRUE);
        h_dText = Ctl(L"STATIC", L"", SS_LEFT, 16, 456, 568, 50, 0);
        SetWindowTextW(h_dTitle, L"Welcome, mate!");
        SetWindowTextW(h_dText, L"Hover over (or click) anything to find out what it does. Pick a preset to start, then fine-tune on the tabs. Every effect has a performance tag: Free, Low, Medium or High.");
        // ---- tab 0: effects (checkbox, impact tag, help text)
        g_pg = PG_T0; y = 170;
        h_ao = Ctl(L"BUTTON", L"Ambient occlusion (soft crevice shadows)", BS_AUTOCHECKBOX | WS_TABSTOP, 30, y, 440, 22, ID_AO); c = Tag(TG_AO, 490, y + 2);
        AddDesc2(h_ao, c, L"Ambient occlusion - crevice shadows", L"Adds the soft darkness where things almost touch: under rocks, between fence boards, beside Ty's feet. Without it the world looks like it is floating a millimetre above itself."); y += 26;
        h_gi = Ctl(L"BUTTON", L"Global illumination (bounce light, colour bleeding)", BS_AUTOCHECKBOX | WS_TABSTOP, 30, y, 440, 22, ID_GI); c = Tag(TG_GI, 490, y + 2);
        AddDesc2(h_gi, c, L"Global illumination - colour gossip", L"Light bounces around: a red rock tints the sand next to it, a green bush glows onto Ty. Lovely, but one of the hungriest effects and quite subtle in this game.\n\nThe first thing to switch off if you need frames."); y += 26;
        h_atmos = Ctl(L"BUTTON", L"Volumetric fog + light shafts", BS_AUTOCHECKBOX | WS_TABSTOP, 30, y, 440, 22, ID_ATMOS); c = Tag(TG_ATM, 490, y + 2);
        AddDesc2(h_atmos, c, L"Volumetric fog - outback atmosphere", L"Dusty haze that fades far-away rocks into the distance (and the mountains painted on the horizon), plus sun beams through the air. It is what stops everything looking crisp from here to Uluru."); y += 26;
        h_tess = Ctl(L"BUTTON", L"Surface detail (cracks, grain, rope strands)", BS_AUTOCHECKBOX | WS_TABSTOP, 30, y, 440, 22, ID_TESS); c = Tag(TG_TESS, 490, y + 2);
        AddDesc2(h_tess, c, L"Surface detail - texture you can almost touch", L"Uses each texture's own paint to carve cracks, wood grain and rope strands into the surface, so the ground has relief instead of being a flat painting. Characters are left alone, so Ty never grows wrinkles."); y += 26;
        h_tessAll = Ctl(L"BUTTON", L"   ...plus real bumpy geometry on the ground (heavy)", BS_AUTOCHECKBOX | WS_TABSTOP, 30, y, 440, 22, ID_TESSALL); c = Tag(TG_TESSALL, 490, y + 2);
        AddDesc2(h_tessAll, c, L"Real bumpy geometry - for show-offs", L"Cuts the ground and props into thousands of tiny triangles so the relief is real geometry, not just clever shading. Most visible when the camera skims low over the sand.\n\nMost of the effect is already in Surface detail."); y += 26;
        h_post = Ctl(L"BUTTON", L"Finishing touches (colour grade, sharpening, smoothing)", BS_AUTOCHECKBOX | WS_TABSTOP, 30, y, 440, 22, ID_POST); c = Tag(TG_POST, 490, y + 2);
        AddDesc2(h_post, c, L"Finishing touches - the final polish", L"Edge smoothing, a gentle film-like colour curve and sharpening applied to the finished picture. Cheap and makes a surprising difference."); y += 30;
        l = h_aaLab = Label(L"Edge smoothing:", 30, y + 3, 110, 18);
        h_aa = Ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 144, y, 300, 120, ID_AA);
        SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"Off (game MSAA only)"); SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"FXAA (fast blur)");
        SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"TAA (smoothest, recommended)"); SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"TAA + FXAA"); c = Tag(TG_AA, 490, y + 3);
        { const wchar_t* tt = L"Edge smoothing - jaggy remover"; const wchar_t* tx = L"Off = just the game's own smoothing. FXAA = a quick soften of edges. TAA = borrows detail from previous frames for the smoothest edges and calmest shimmer (recommended). TAA + FXAA = belt and braces.\n\nImpact: LOW.";
          AddDesc(l, tt, tx); AddDesc(h_aa, tt, tx); AddDesc(c, tt, tx); } y += 30;
        l = h_shLab = Label(L"Shadow sharpness:", 30, y + 3, 110, 18);
        h_shadow = Ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 144, y, 300, 120, ID_SHADOW);
        SendMessageW(h_shadow, CB_ADDSTRING, 0, (LPARAM)L"2048 (original, blocky)"); SendMessageW(h_shadow, CB_ADDSTRING, 0, (LPARAM)L"4096 (crisp)");
        SendMessageW(h_shadow, CB_ADDSTRING, 0, (LPARAM)L"8192 (razor sharp, big memory)"); c = Tag(TG_SHADOW, 490, y + 3);
        { const wchar_t* tt = L"Shadow sharpness - how crisp the shade is"; const wchar_t* tx = L"2048 is the original blocky look, 4096 is crisp, 8192 is razor sharp but memory hungry. Takes effect the next time the game starts.";
          AddDesc(l, tt, tx); AddDesc(h_shadow, tt, tx); AddDesc(c, tt, tx); }
        Ctl(L"STATIC", L"Tags: Free = no cost, Low = barely noticeable, Medium = noticeable on weaker cards, High = switch off first if it stutters.", SS_LEFT, 30, 388, 540, 34, 0);
        // ---- tabs 1-3: look sliders. Each slider lands on the tab in kSlTab (1 Light & shadow, 2 Air & fog, 3 Picture).
        static const int kSlTab[NSL] = { 3, 3, 3, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 3, 3 };
        int sy[5] = { 172, 172, 172, 172, 172 };
        for (int i = 0; i < NSL; i++) {
            int t = kSlTab[i]; g_pg = t; y = sy[t];
            g_sl[i].lab = Label(g_sl[i].label, 30, y + 5, 150, 18);
            g_sl[i].bar = Ctl(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, 184, y, 180, 28, ID_SL0 + i);
            SendMessageW(g_sl[i].bar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            g_sl[i].val = Label(L"", 370, y + 5, 44, 18);
            g_sl[i].tag = Ctl(L"STATIC", L"", SS_CENTER | SS_NOTIFY, 420, y + 5, 70, 18, 0); SendMessageW(g_sl[i].tag, WM_SETFONT, (WPARAM)g_fontB, TRUE);
            AddDesc(g_sl[i].lab, g_sl[i].label, g_sl[i].tip); AddDesc(g_sl[i].bar, g_sl[i].label, g_sl[i].tip); AddDesc(g_sl[i].val, g_sl[i].label, g_sl[i].tip); AddDesc(g_sl[i].tag, g_sl[i].label, g_sl[i].tip);
            sy[t] += 36;
        }
        // ---- tab 4: advanced
        g_pg = PG_T4;
        h_detail = Ctl(L"BUTTON", L"Game detail settings: high (props, grass, reflections)", BS_AUTOCHECKBOX | WS_TABSTOP, 30, 172, 440, 22, ID_DETAIL); c = Tag(TG_DETAIL, 490, 174);
        AddDesc2(h_detail, c, L"Game detail settings - the game's own dials", L"Flips the original game's detail options (props, grass, reflections, anisotropic filtering) to their highest setting. Your own settings.ini is backed up once before the first change.\n\nHow much it costs depends on your graphics card, usually not much.");
        c = Ctl(L"BUTTON", L"Steam setup...", WS_TABSTOP, 30, 222, 160, 30, ID_STEAM); AddDesc(c, L"Steam setup", L"Copies a launch option so Steam's own Play button starts the game with the HD renderer. One-time setup.");
        c = Ctl(L"BUTTON", L"Advanced (ini)...", WS_TABSTOP, 200, 222, 160, 30, ID_ADV); AddDesc(c, L"Advanced settings", L"Opens tyrender.ini in Notepad for the full list of knobs. For tinkerers.");
        c = Ctl(L"BUTTON", L"Reset sliders", WS_TABSTOP, 370, 222, 170, 30, ID_RESET); AddDesc(c, L"Reset sliders", L"Puts every look slider back to its default position. Your checkboxes and preset stay as they are.");
        c = Ctl(L"BUTTON", L"Report a problem / open logs folder", WS_TABSTOP, 30, 262, 340, 30, ID_REPORT); AddDesc(c, L"Report a problem", L"Opens the folder with the session report the mod writes each time you play. It holds hardware basics and frame times, no names or personal paths. Send that file to the author, with a screenshot and your GPU name.");
        Ctl(L"STATIC", L"Nothing here is needed for a good picture: the presets already pick good settings.", SS_LEFT, 30, 306, 540, 36, 0);
        g_pg = PG_ANY;
        AssignLv(); LoadSettings(&g_s); ToUI(); SlidersFromIni(); SendMessageW(h_detail, BM_SETCHECK, GameDetailIsHigh(), 0); RefreshCosts();
        UpdateStatus(NULL); SetTimer(h, 1, 2000, NULL); SetTimer(h, 2, 90, NULL); SetTimer(h, 3, 30, NULL);
        ApplyLayout();
        return 0; }
    case WM_TIMER: if (wp == 3) GamepadTick(); else if (wp == 2) HoverUpdate(); else UpdateStatus(NULL); return 0;
    case WM_HSCROLL: {
        for (int i = 0; i < NSL; i++) if ((HWND)lp == g_sl[i].bar) { SliderText(i); SliderToIni(i); RefreshCosts(); if (GameRunning()) LiveReload(); ShowDesc(g_sl[i].bar); }
        return 0; }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id >= ID_P0 && id < ID_P0 + NPRESET && code == BN_CLICKED) {
            int p = id - ID_P0; g_s = kPresetVals[p]; ToUI(); Wf(L"atmos", L"ContactShadows", g_s.contact ? 0.35f : 0.0f); Wf(L"atmos", L"Intensity", kPresetHaze[p]); Wf(L"atmos", L"Desaturation", p == 0 ? 0.8f : 0.3f); Wf(L"atmos", L"HazeLift", p == 0 ? 0.35f : 0.15f); Wf(L"post", L"Brightness", kPresetGrade[p][0]); Wf(L"post", L"Saturation", kPresetGrade[p][1]); SlidersFromIni(); SaveAll(); ShowDesc(h_preset[p]);
        }
        else if ((id >= ID_AO && id <= ID_SHADOW) && !g_updating && (code == BN_CLICKED || code == CBN_SELCHANGE)) { SaveAll(); ShowDesc((HWND)lp); }
        else if (id == ID_DETAIL && code == BN_CLICKED) SetGameDetail((int)SendMessageW(h_detail, BM_GETCHECK, 0, 0));
        else if (id == ID_PLAY) { SaveAll(); int ok = EnsurePatched(); (void)ok; StartGame(1); UpdateStatus(L"Started. Keep this window open to fine-tune the sliders live."); }
        else if (id == ID_STOCK) { SaveAll(); StartGame(0); }
        else if (id == ID_ADV) { SaveAll(); ShellExecuteW(h, L"open", L"notepad.exe", g_ini, NULL, SW_SHOWNORMAL); }
        else if (id == ID_REPORT) { wchar_t d[MAX_PATH]; swprintf(d, MAX_PATH, L"%lstyrender\\logs", g_dir); CreateDirectoryW(d, NULL); ShellExecuteW(h, L"open", d, NULL, NULL, SW_SHOWNORMAL); UpdateStatus(L"Logs folder opened. Send the newest TYHD_report file, plus a screenshot and your GPU name. See README: Reporting a bug."); }
        else if (id == ID_MORE) { g_expanded = !g_expanded; ApplyLayout(); }
        else if (id == ID_RESET) { for (int i = 0; i < NSL; i++) Wf(g_sl[i].sec, g_sl[i].key, g_sl[i].def); SlidersFromIni(); RefreshCosts(); if (GameRunning()) LiveReload(); }
        else if (id == ID_STEAM) {
            EnsurePatched();
            wchar_t opt[MAX_PATH + 64]; swprintf(opt, MAX_PATH + 64, L"\"%lsTYLauncher.exe\" --play %%command%%", g_dir);
            if (OpenClipboard(h)) { EmptyClipboard(); size_t n = (wcslen(opt) + 1) * sizeof(wchar_t); HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n); memcpy(GlobalLock(g), opt, n); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); CloseClipboard(); }
            wchar_t m[1200]; swprintf(m, 1200,
                L"To start the HD renderer from Steam's Play button:\n\n1. Steam > Library > right-click TY the Tasmanian Tiger > Properties\n2. General > Launch options: paste (already copied to your clipboard):\n\n%ls\n\n"
                L"Steam then runs the launcher in quick mode, which starts the game with the renderer using your saved settings.\nRemove the launch option to go back to the original game.", opt);
            MessageBoxW(h, m, L"Steam setup", MB_ICONINFORMATION);
        }
        return 0; }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* d = (const DRAWITEMSTRUCT*)lp;
        if (d->CtlID >= ID_P0 && d->CtlID < ID_P0 + NPRESET) {
            int p = (int)d->CtlID - ID_P0, sel = p == g_presetSel, down = (d->itemState & ODS_SELECTED) != 0;
            COLORREF fill = sel ? RGB(0, 120, 215) : down ? RGB(215, 215, 215) : RGB(238, 238, 238), edge = sel ? RGB(0, 84, 153) : RGB(150, 150, 150);
            HBRUSH b = CreateSolidBrush(fill), e = CreateSolidBrush(edge); FillRect(d->hDC, &d->rcItem, b); FrameRect(d->hDC, &d->rcItem, e); DeleteObject(b); DeleteObject(e);
            SelectObject(d->hDC, g_fontBig); SetBkMode(d->hDC, TRANSPARENT); SetTextColor(d->hDC, sel ? RGB(255, 255, 255) : RGB(30, 30, 30));
            RECT r = d->rcItem; DrawTextW(d->hDC, kPresets[p], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (d->itemState & ODS_FOCUS) { RECT fr = d->rcItem; InflateRect(&fr, -4, -4); DrawFocusRect(d->hDC, &fr); }
            return TRUE;
        }
        break; }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND c = (HWND)lp; int lv = -1;
        for (int i = 0; i < TG_N; i++) if (c == g_tagH[i]) lv = g_tagLv[i];
        for (int i = 0; i < NSL; i++) if (c == g_sl[i].tag) lv = g_slLv[i];
        if (c == h_overall) lv = g_overallLv == 0 ? 1 : g_overallLv == 1 ? 2 : 3;   // LOW green, MEDIUM amber, HIGH/EXTREME red
        if (g_gpSeen) for (int i = 0; i < NSL; i++) if (c == g_sl[i].lab && GetFocus() == g_sl[i].bar) {   // gamepad focus cue for a slider
            SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(0, 84, 153)); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE); }
        if (lv >= 0) { SetBkMode(dc, TRANSPARENT); SetTextColor(dc, IsWindowEnabled(c) ? LvColor(lv) : RGB(170, 170, 170)); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE); }
        SetBkMode(dc, TRANSPARENT); if (!IsWindowEnabled(c)) SetTextColor(dc, RGB(150, 150, 150)); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE); }
    case WM_NOTIFY:
        if (((NMHDR*)lp)->idFrom == ID_TAB && ((NMHDR*)lp)->code == TCN_SELCHANGE) { g_tab = (int)SendMessageW(h_tab, TCM_GETCURSEL, 0, 0); ApplyLayout(); }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cmd, int show) {
    GetModuleFileNameW(NULL, g_dir, MAX_PATH); wchar_t* s = wcsrchr(g_dir, L'\\'); if (s) s[1] = 0;
    swprintf(g_ini, MAX_PATH, L"%lstyrender.ini", g_dir);
    EnsurePayload();
    int play = 0, inject = 1, argc; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--wait")) { g_wait = 1; continue; }
        if (!wcscmp(argv[i], L"--noinject")) { inject = 0; play = 1; continue; }
        if (!wcscmp(argv[i], L"--play")) { play = 1; continue; }
        size_t L = wcslen(argv[i]);
        if (L >= 6 && !_wcsicmp(argv[i] + L - 6, L"TY.exe")) continue;   // Steam's %command% = path of TY.exe -> not a game argument
        wcscat(g_args, L" "); wcscat(g_args, argv[i]);
    }
    if (play) { if (inject) { wchar_t in[MAX_PATH], out[MAX_PATH], err[300]; swprintf(in, MAX_PATH, L"%lsTY.exe", g_dir); swprintf(out, MAX_PATH, L"%lsTY_tyrender.exe", g_dir);
                              if (GetFileAttributesW(out) == INVALID_FILE_ATTRIBUTES) PatchExe(in, out, err, 300); } return StartGame(inject); }
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES }; InitCommonControlsEx(&icc);
    WNDCLASSW wc = {0}; wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"TYLauncherWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);
    RECT r = {0, 0, 480, 300}; AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    CreateWindowW(L"TYLauncherWnd", TY_WINDOW_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    MSG m; while (GetMessageW(&m, NULL, 0, 0)) { if (!IsDialogMessageW(g_hw, &m)) { TranslateMessage(&m); DispatchMessageW(&m); } }
    return 0;
}
