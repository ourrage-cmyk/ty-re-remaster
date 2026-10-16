// ------------------------------------------------------------ window
enum { ID_P0 = 100, ID_AO = 110, ID_ATMOS, ID_TESS, ID_TESSALL, ID_POST, ID_GI, ID_AA, ID_SHADOW, ID_DETAIL, ID_PLAY = 130, ID_STOCK, ID_STEAM, ID_ADV, ID_RESET, ID_SL0 = 300 };
enum { TG_AO, TG_GI, TG_ATM, TG_TESS, TG_TESSALL, TG_POST, TG_AA, TG_SHADOW, TG_DETAIL, TG_N };
static HWND g_hw, h_preset[NPRESET], h_presetNote, h_ao, h_atmos, h_tess, h_tessAll, h_post, h_gi, h_aa, h_shadow, h_detail, h_status, h_total, h_overall, h_dTitle, h_dText;
static Settings g_s; static int g_updating;
static HWND g_tagH[TG_N]; static int g_tagLv[TG_N], g_slLv[64], g_overallLv;
static HFONT g_font, g_fontB, g_fontT, g_fontBig;
typedef struct { HWND h; const wchar_t* title; const wchar_t* text; } Desc;
static Desc g_desc[160]; static int g_nd, g_descCur = -1;
static const wchar_t* kLvName[4] = { L"Free", L"Low", L"Medium", L"High" };
static const wchar_t* kTotName[4] = { L"LOW", L"MEDIUM", L"HIGH", L"EXTREME" };
static COLORREF LvColor(int lv) { return lv == 0 ? RGB(105, 105, 105) : lv == 1 ? RGB(0, 135, 0) : lv == 2 ? RGB(205, 110, 0) : RGB(200, 0, 0); }
static int Lvl(float ms) { return ms < 0.2f ? 0 : ms < 1.5f ? 1 : ms < 3.5f ? 2 : 3; }   // thresholds on the measured GPU frame-time cost
static int TotLvl(float ms) { return ms < 1.5f ? 0 : ms < 7.0f ? 1 : ms < 14.0f ? 2 : 3; }

static void AddDesc(HWND h, const wchar_t* title, const wchar_t* text) { if (g_nd < 160) { g_desc[g_nd].h = h; g_desc[g_nd].title = title; g_desc[g_nd].text = text; g_nd++; } }
static void ShowDesc(HWND w) {
    for (int i = 0; i < g_nd; i++) if (g_desc[i].h == w) { if (g_descCur != i) { g_descCur = i; SetWindowTextW(h_dTitle, g_desc[i].title); SetWindowTextW(h_dText, g_desc[i].text); } return; }
}
static void HoverUpdate(void) { POINT p; if (GetCursorPos(&p)) { HWND w = WindowFromPoint(p); if (w) ShowDesc(w); } }

static int GameRunning(void) { return FindWindowW(NULL, L"TY the Tasmanian Tiger") != NULL; }
static void LiveReload(void) {   // tell a running tyrender to re-read the ini
    wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%lstyrender\\reload", g_dir);
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
static void UpdateStatus(const wchar_t* extra) {
    wchar_t exe[MAX_PATH], m[400]; swprintf(exe, MAX_PATH, L"%lsTY_tyrender.exe", g_dir);
    int patched = GetFileAttributesW(exe) != INVALID_FILE_ATTRIBUTES;
    swprintf(m, 400, L"%ls%ls", GameRunning() ? L"Game running: sliders apply live. " : L"",
        extra ? extra : patched ? L"Ready." : L"First Play creates TY_tyrender.exe (a copy of TY.exe that loads the renderer; TY.exe is never changed).");
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
    for (int i = 0; i < TG_N; i++) if (g_tagH[i]) { SetWindowTextW(g_tagH[i], kLvName[g_tagLv[i]]); InvalidateRect(g_tagH[i], NULL, TRUE); }
    for (int i = 0; i < NSL; i++) if (g_sl[i].tag) { SetWindowTextW(g_sl[i].tag, kLvName[g_slLv[i]]); InvalidateRect(g_sl[i].tag, NULL, TRUE); }
    g_overallLv = TotLvl(g_costTotal);
    wchar_t m[200]; swprintf(m, 200, L"Overall performance impact:  %ls", kTotName[g_overallLv]); SetWindowTextW(h_overall, m); InvalidateRect(h_overall, NULL, TRUE);
    swprintf(m, 200, L"Impact is relative to the original game at your resolution (%dx%d). A faster graphics card shrinks it, a higher resolution grows it.", g_resW, g_resH);
    SetWindowTextW(h_total, m);
    int mp = MatchPreset();
    for (int i = 0; i < NPRESET; i++) SendMessageW(h_preset[i], BM_SETCHECK, i == mp, 0);
    SetWindowTextW(h_presetNote, mp >= 0 ? kPresetNote[mp] : L"Custom mix: you changed something. Click a preset to snap back.");
}
static void SaveAll(void) { FromUI(); RefreshCosts(); SaveSettings(&g_s); for (int i = 0; i < NSL; i++) SliderToIni(i); if (GameRunning()) LiveReload(); }
static HWND Ctl(const wchar_t* cls, const wchar_t* txt, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowW(cls, txt, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_hw, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE); return c;
}
static HWND Label(const wchar_t* txt, int x, int y, int w, int h) { return Ctl(L"STATIC", txt, SS_NOTIFY, x, y, w, h, 0); }
static HWND Tag(int idx, int x, int y) { g_tagH[idx] = Ctl(L"STATIC", L"", SS_CENTER | SS_NOTIFY, x, y, 66, 18, 0); SendMessageW(g_tagH[idx], WM_SETFONT, (WPARAM)g_fontB, TRUE); return g_tagH[idx]; }
static int EnsurePatched(void) {
    wchar_t in[MAX_PATH], out[MAX_PATH], err[300] = L""; swprintf(in, MAX_PATH, L"%lsTY.exe", g_dir); swprintf(out, MAX_PATH, L"%lsTY_tyrender.exe", g_dir);
    if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) return 1;
    if (PatchExe(in, out, err, 300)) return 1;
    MessageBoxW(g_hw, err, L"TYLauncher - could not create TY_tyrender.exe (will try DLL injection)", MB_ICONWARNING); return 0;
}
static const wchar_t* kPresetTitle[NPRESET] = { L"Low - potato mode", L"Medium - the sweet spot", L"High - look at me", L"Ultra - because we can" };
static const wchar_t* kPresetText[NPRESET] = {
    L"The original game with a light polish: gentle edge smoothing and cleaner shadows. No fancy lighting, so it runs happily on old laptops and integrated graphics.\n\nPerformance impact: LOW.",
    L"The outback as it was meant to be seen: crevice shadows, dusty volumetric haze with sun beams, carved cracks and grain on the ground, soft sharp shadows and smooth edges.\n\nThe one we recommend. Performance impact: MEDIUM.",
    L"Everything in Medium plus bounce light (red rocks tint the sand beside them) and tiny contact shadows under Ty's feet. Pretty, hungry and not for shy graphics cards.\n\nPerformance impact: HIGH.",
    L"Maximum everything: razor-sharp shadows, thick haze, extra occlusion and bounce rays, real bumpy geometry on the ground. Bring a strong graphics card and a fan that believes in you.\n\nPerformance impact: EXTREME." };
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hw = h; GameResolution();
        g_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontB = CreateFontW(-13, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontT = CreateFontW(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fontBig = CreateFontW(-15, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        const int LX = 16, TX = 392, MX = 484, PX = 880;
        // ---- left column: presets
        Ctl(L"BUTTON", L"Quality preset", BS_GROUPBOX, 8, 4, 456, 104, 0);
        for (int i = 0; i < NPRESET; i++) {
            h_preset[i] = Ctl(L"BUTTON", kPresets[i], BS_AUTORADIOBUTTON | BS_PUSHLIKE | (i == 0 ? WS_GROUP : 0) | WS_TABSTOP, LX + i * 111, 24, 106, 38, ID_P0 + i);
            SendMessageW(h_preset[i], WM_SETFONT, (WPARAM)g_fontBig, TRUE); AddDesc(h_preset[i], kPresetTitle[i], kPresetText[i]);
        }
        h_presetNote = Label(L"", LX, 68, 440, 36);
        // ---- left column: effects
        Ctl(L"BUTTON", L"Effects  (tag = performance impact if switched on)", BS_GROUPBOX, 8, 112, 456, 254, 0);
        int y = 134; HWND c, l;
        h_ao = Ctl(L"BUTTON", L"Ambient occlusion (soft crevice shadows)", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_AO); c = Tag(TG_AO, TX, y + 2);
        AddDesc(h_ao, L"Ambient occlusion - crevice shadows", L"Adds the soft darkness where things almost touch: under rocks, between fence boards, beside Ty's feet. Without it the world looks like it is floating a millimetre above itself.\n\nImpact: LOW. Cheap for what it does."); AddDesc(c, L"Ambient occlusion - crevice shadows", L"Adds the soft darkness where things almost touch: under rocks, between fence boards, beside Ty's feet. Without it the world looks like it is floating a millimetre above itself.\n\nImpact: LOW. Cheap for what it does."); y += 26;
        h_gi = Ctl(L"BUTTON", L"Global illumination (bounce light, colour bleeding)", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_GI); c = Tag(TG_GI, TX, y + 2);
        AddDesc(h_gi, L"Global illumination - colour gossip", L"Light bounces around: a red rock tints the sand next to it, a green bush glows onto Ty. Lovely, but one of the hungriest effects and quite subtle in this game.\n\nImpact: HIGH. The first thing to switch off if you need frames."); AddDesc(c, L"Global illumination - colour gossip", L"Light bounces around: a red rock tints the sand next to it, a green bush glows onto Ty. Lovely, but one of the hungriest effects and quite subtle in this game.\n\nImpact: HIGH. The first thing to switch off if you need frames."); y += 26;
        h_atmos = Ctl(L"BUTTON", L"Volumetric fog + light shafts", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_ATMOS); c = Tag(TG_ATM, TX, y + 2);
        AddDesc(h_atmos, L"Volumetric fog - outback atmosphere", L"Dusty haze that fades far-away rocks into the distance (and the mountains painted on the horizon), plus sun beams through the air. It is what stops everything looking crisp from here to Uluru.\n\nImpact: MEDIUM."); AddDesc(c, L"Volumetric fog - outback atmosphere", L"Dusty haze that fades far-away rocks into the distance (and the mountains painted on the horizon), plus sun beams through the air. It is what stops everything looking crisp from here to Uluru.\n\nImpact: MEDIUM."); y += 26;
        h_tess = Ctl(L"BUTTON", L"Surface detail (cracks, grain, rope strands)", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_TESS); c = Tag(TG_TESS, TX, y + 2);
        AddDesc(h_tess, L"Surface detail - texture you can almost touch", L"Uses each texture's own paint to carve cracks, wood grain and rope strands into the surface, so the ground has relief instead of being a flat painting. Characters are left alone, so Ty never grows wrinkles.\n\nImpact: LOW."); AddDesc(c, L"Surface detail - texture you can almost touch", L"Uses each texture's own paint to carve cracks, wood grain and rope strands into the surface, so the ground has relief instead of being a flat painting. Characters are left alone, so Ty never grows wrinkles.\n\nImpact: LOW."); y += 26;
        h_tessAll = Ctl(L"BUTTON", L"   ...plus real bumpy geometry on the ground (heavy)", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_TESSALL); c = Tag(TG_TESSALL, TX, y + 2);
        AddDesc(h_tessAll, L"Real bumpy geometry - for show-offs", L"Cuts the ground and props into thousands of tiny triangles so the relief is real geometry, not just clever shading. Most visible when the camera skims low over the sand.\n\nImpact: HIGH. Most of the effect is already in Surface detail."); AddDesc(c, L"Real bumpy geometry - for show-offs", L"Cuts the ground and props into thousands of tiny triangles so the relief is real geometry, not just clever shading. Most visible when the camera skims low over the sand.\n\nImpact: HIGH. Most of the effect is already in Surface detail."); y += 26;
        h_post = Ctl(L"BUTTON", L"Finishing touches (colour grade, sharpening, smoothing)", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_POST); c = Tag(TG_POST, TX, y + 2);
        AddDesc(h_post, L"Finishing touches - the final polish", L"Edge smoothing, a gentle film-like colour curve and sharpening applied to the finished picture. Cheap and makes a surprising difference.\n\nImpact: LOW."); AddDesc(c, L"Finishing touches - the final polish", L"Edge smoothing, a gentle film-like colour curve and sharpening applied to the finished picture. Cheap and makes a surprising difference.\n\nImpact: LOW."); y += 30;
        l = Label(L"Edge smoothing:", LX, y + 3, 110, 18);
        h_aa = Ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 130, y, 240, 120, ID_AA);
        SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"Off (game MSAA only)"); SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"FXAA (fast blur)");
        SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"TAA (smoothest, recommended)"); SendMessageW(h_aa, CB_ADDSTRING, 0, (LPARAM)L"TAA + FXAA"); c = Tag(TG_AA, TX, y + 3);
        { const wchar_t* tt = L"Edge smoothing - jaggy remover"; const wchar_t* tx = L"Off = just the game's own smoothing. FXAA = a quick soften of edges. TAA = borrows detail from previous frames for the smoothest edges and calmest shimmer (recommended). TAA + FXAA = belt and braces.\n\nImpact: LOW.";
          AddDesc(l, tt, tx); AddDesc(h_aa, tt, tx); AddDesc(c, tt, tx); } y += 28;
        l = Label(L"Shadow sharpness:", LX, y + 3, 110, 18);
        h_shadow = Ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 130, y, 240, 120, ID_SHADOW);
        SendMessageW(h_shadow, CB_ADDSTRING, 0, (LPARAM)L"2048 (original, blocky)"); SendMessageW(h_shadow, CB_ADDSTRING, 0, (LPARAM)L"4096 (crisp)");
        SendMessageW(h_shadow, CB_ADDSTRING, 0, (LPARAM)L"8192 (razor sharp, big memory)"); c = Tag(TG_SHADOW, TX, y + 3);
        { const wchar_t* tt = L"Shadow sharpness - how crisp the shade is"; const wchar_t* tx = L"2048 is the original blocky look, 4096 is crisp, 8192 is razor sharp but memory hungry. Takes effect the next time the game starts.\n\nImpact: LOW at 4096, HIGH at 8192.";
          AddDesc(l, tt, tx); AddDesc(h_shadow, tt, tx); AddDesc(c, tt, tx); } y += 28;
        h_detail = Ctl(L"BUTTON", L"Game detail settings: high (props, grass, reflections)", BS_AUTOCHECKBOX | WS_TABSTOP, LX, y, 370, 22, ID_DETAIL); c = Tag(TG_DETAIL, TX, y + 2);
        AddDesc(h_detail, L"Game detail settings - the game's own dials", L"Flips the original game's detail options (props, grass, reflections, anisotropic filtering) to their highest setting. Your own settings.ini is backed up once before the first change.\n\nImpact: depends on your graphics card, usually small."); AddDesc(c, L"Game detail settings - the game's own dials", L"Flips the original game's detail options (props, grass, reflections, anisotropic filtering) to their highest setting. Your own settings.ini is backed up once before the first change.\n\nImpact: depends on your graphics card, usually small.");
        // ---- left column: overall + buttons
        h_overall = Ctl(L"STATIC", L"", SS_LEFT | SS_NOTIFY, LX, 374, 440, 24, 0); SendMessageW(h_overall, WM_SETFONT, (WPARAM)g_fontT, TRUE);
        h_total = Ctl(L"STATIC", L"", SS_NOTIFY, LX, 400, 440, 36, 0);
        AddDesc(h_overall, L"Overall performance impact", L"A quick summary of everything that is switched on: LOW, MEDIUM, HIGH or EXTREME. It is based on measured frame-time costs, so it is a guide: your graphics card and screen resolution decide the real result.\n\nIf the game feels slow, switch off the effects tagged High first."); AddDesc(h_total, L"Overall performance impact", L"A quick summary of everything that is switched on: LOW, MEDIUM, HIGH or EXTREME. It is based on measured frame-time costs, so it is a guide: your graphics card and screen resolution decide the real result.\n\nIf the game feels slow, switch off the effects tagged High first.");
        c = Ctl(L"BUTTON", L"Play with HD renderer", BS_DEFPUSHBUTTON | WS_TABSTOP, LX, 446, 220, 40, ID_PLAY); SendMessageW(c, WM_SETFONT, (WPARAM)g_fontBig, TRUE);
        AddDesc(c, L"Play with HD renderer", L"Starts the game with the renderer and your current settings. Keep this window open while you play: every slider changes the picture live.");
        c = Ctl(L"BUTTON", L"Play original (no mod)", WS_TABSTOP, 244, 446, 212, 40, ID_STOCK); AddDesc(c, L"Play original", L"Starts the untouched original game. Handy for comparing, or for when you just want to feel nostalgic.");
        c = Ctl(L"BUTTON", L"Steam setup...", WS_TABSTOP, LX, 494, 140, 28, ID_STEAM); AddDesc(c, L"Steam setup", L"Copies a launch option so Steam's own Play button starts the game with the HD renderer. One-time setup.");
        c = Ctl(L"BUTTON", L"Advanced (ini)...", WS_TABSTOP, 160, 494, 140, 28, ID_ADV); AddDesc(c, L"Advanced settings", L"Opens tyrender.ini in Notepad for the full list of knobs. For tinkerers.");
        c = Ctl(L"BUTTON", L"Reset sliders", WS_TABSTOP, 304, 494, 152, 28, ID_RESET); AddDesc(c, L"Reset sliders", L"Puts every look slider back to its default position. Your checkboxes and preset stay as they are.");
        h_status = Ctl(L"STATIC", L"", 0, LX, 530, 440, 40, 0);
        // ---- middle column: sliders
        Ctl(L"BUTTON", L"Look && balance  (tag = performance impact; changes apply live)", BS_GROUPBOX, MX - 8, 4, 394, 568, 0);
        int sy = 26;
        for (int i = 0; i < NSL; i++) {
            g_sl[i].lab = Label(g_sl[i].label, MX, sy + 5, 132, 18);
            g_sl[i].bar = Ctl(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, MX + 132, sy, 124, 28, ID_SL0 + i);
            SendMessageW(g_sl[i].bar, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            g_sl[i].val = Label(L"", MX + 258, sy + 5, 38, 18);
            g_sl[i].tag = Ctl(L"STATIC", L"", SS_CENTER | SS_NOTIFY, MX + 300, sy + 5, 66, 18, 0); SendMessageW(g_sl[i].tag, WM_SETFONT, (WPARAM)g_fontB, TRUE);
            AddDesc(g_sl[i].lab, g_sl[i].label, g_sl[i].tip); AddDesc(g_sl[i].bar, g_sl[i].label, g_sl[i].tip); AddDesc(g_sl[i].val, g_sl[i].label, g_sl[i].tip); AddDesc(g_sl[i].tag, g_sl[i].label, g_sl[i].tip);
            sy += 34;
        }
        // ---- right column: description panel
        Ctl(L"BUTTON", L"What does this do?", BS_GROUPBOX, PX - 8, 4, 250, 568, 0);
        h_dTitle = Ctl(L"STATIC", L"", SS_LEFT, PX, 28, 234, 52, 0); SendMessageW(h_dTitle, WM_SETFONT, (WPARAM)g_fontT, TRUE);
        h_dText = Ctl(L"STATIC", L"", SS_LEFT, PX, 88, 234, 470, 0);
        SetWindowTextW(h_dTitle, L"Welcome, mate!");
        SetWindowTextW(h_dText, L"Hover over (or click) anything to find out what it does.\n\nPick a preset on the left to start, then fine-tune with the sliders. Every effect has a performance tag: Free, Low, Medium or High. They tell you how much the effect slows the game relative to its look, without pretending to know your graphics card.\n\nTip: keep this window open while you play, the sliders work live.");
        LoadSettings(&g_s); ToUI(); SlidersFromIni(); SendMessageW(h_detail, BM_SETCHECK, GameDetailIsHigh(), 0); RefreshCosts();
        UpdateStatus(NULL); SetTimer(h, 1, 2000, NULL); SetTimer(h, 2, 90, NULL);
        return 0; }
    case WM_TIMER: if (wp == 2) HoverUpdate(); else UpdateStatus(NULL); return 0;
    case WM_HSCROLL: {
        for (int i = 0; i < NSL; i++) if ((HWND)lp == g_sl[i].bar) { SliderText(i); SliderToIni(i); RefreshCosts(); if (GameRunning()) LiveReload(); ShowDesc(g_sl[i].bar); }
        return 0; }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id >= ID_P0 && id < ID_P0 + NPRESET && code == BN_CLICKED) {
            int p = id - ID_P0; g_s = kPresetVals[p]; ToUI(); Wf(L"atmos", L"ContactShadows", g_s.contact ? 0.35f : 0.0f); Wf(L"atmos", L"Intensity", kPresetHaze[p]); SlidersFromIni(); SaveAll(); ShowDesc(h_preset[p]);
        }
        else if ((id >= ID_AO && id <= ID_SHADOW) && !g_updating && (code == BN_CLICKED || code == CBN_SELCHANGE)) { SaveAll(); ShowDesc((HWND)lp); }
        else if (id == ID_DETAIL && code == BN_CLICKED) SetGameDetail((int)SendMessageW(h_detail, BM_GETCHECK, 0, 0));
        else if (id == ID_PLAY) { SaveAll(); int ok = EnsurePatched(); (void)ok; StartGame(1); UpdateStatus(L"Started. Keep this window open to fine-tune the sliders live."); }
        else if (id == ID_STOCK) { SaveAll(); StartGame(0); }
        else if (id == ID_ADV) { SaveAll(); ShellExecuteW(h, L"open", L"notepad.exe", g_ini, NULL, SW_SHOWNORMAL); }
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
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND c = (HWND)lp; int lv = -1;
        for (int i = 0; i < TG_N; i++) if (c == g_tagH[i]) lv = g_tagLv[i];
        for (int i = 0; i < NSL; i++) if (c == g_sl[i].tag) lv = g_slLv[i];
        if (c == h_overall) lv = g_overallLv == 0 ? 1 : g_overallLv == 1 ? 2 : 3;   // LOW green, MEDIUM amber, HIGH/EXTREME red
        if (lv >= 0) { SetBkMode(dc, TRANSPARENT); SetTextColor(dc, LvColor(lv)); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE); }
        SetBkMode(dc, TRANSPARENT); return (LRESULT)GetSysColorBrush(COLOR_BTNFACE); }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cmd, int show) {
    GetModuleFileNameW(NULL, g_dir, MAX_PATH); wchar_t* s = wcsrchr(g_dir, L'\\'); if (s) s[1] = 0;
    swprintf(g_ini, MAX_PATH, L"%lstyrender.ini", g_dir);
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
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES | ICC_BAR_CLASSES }; InitCommonControlsEx(&icc);
    WNDCLASSW wc = {0}; wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"TYLauncherWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);
    RECT r = {0, 0, 1146, 580}; AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    CreateWindowW(L"TYLauncherWnd", L"TY HD Renderer", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    MSG m; while (GetMessageW(&m, NULL, 0, 0)) { if (!IsDialogMessageW(g_hw, &m)) { TranslateMessage(&m); DispatchMessageW(&m); } }
    return 0;
}
