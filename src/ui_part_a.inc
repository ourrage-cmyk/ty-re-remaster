// ------------------------------------------------------------ settings model (included by TYLauncher.c)
// Toggles/combos map to tyrender.ini; sliders map to float keys. While the game runs, every change is written immediately and
// tyrender\reload is created, so the DLL re-reads the ini on the fly (live balancing).
typedef struct { int ao, aoSlices, aoSteps, atmos, atmosSteps, tess, tessAll, tessLevel, post, aa, shScale, shTaps, gi, giRays, contact; } Settings;
#define NPRESET 4   // Low, Medium, High, Ultra ; no button selected = custom mix
static const wchar_t* kPresets[NPRESET] = { L"Low", L"Medium", L"High", L"Ultra" };
static const float kPresetHaze[NPRESET] = { 2.5f, 2.2f, 2.2f, 2.1f };   // [atmos] Intensity written by the preset
static const Settings kPresetVals[NPRESET] = {
    // ao sl st atm st  tes all lvl post aa shS taps gi rays contact     (measured on the merged build, RX 7600 XT, 2560x1440: stock 5.7 ms, Low 6.0, Medium 11.1, High 17.2, Ultra 32.1)
    {  0, 2, 4,  0, 12,  0, 0,  4,  1,  1,  1,  8,  0, 1, 0 },
    {  1, 3, 6,  1, 24,  1, 0,  8,  1,  2,  2, 16,  0, 1, 0 },
    {  1, 3, 6,  1, 24,  1, 0,  8,  1,  2,  2, 16,  1, 1, 1 },
    {  1, 4, 8,  1, 32,  1, 1, 24,  1,  2,  4, 32,  1, 2, 1 },
};
typedef struct { const wchar_t* sec; const wchar_t* key; const wchar_t* label; float mn, mx, def; const wchar_t* tip; HWND bar, val, tag, lab; } Slider;
static Slider g_sl[] = {
    { L"post",    L"Brightness",    L"Brightness",          0.70f, 1.50f, 1.00f, L"Turn the sun up or down. Sunglasses for the whole outback." },
    { L"post",    L"Contrast",      L"Contrast",            0.80f, 1.30f, 1.00f, L"Darks get darker, lights get lighter. Too much and even the kangaroos look dramatic." },
    { L"post",    L"Saturation",    L"Colour intensity",    0.60f, 1.60f, 1.03f, L"Colour volume. Low = faded old photograph, high = candy shop." },
    { L"ao",      L"Strength",      L"Crevice shadows",     0.00f, 1.50f, 0.60f, L"How dark the nooks and crannies get (needs Ambient occlusion on). Changing it costs nothing; the switch on the left pays the bill." },
    { L"ao",      L"WideStrength",  L"Corner darkness",     0.00f, 2.00f, 1.00f, L"Big-picture darkening where walls meet the floor, under rocks and in hollows, so corners finally look like corners (needs Ambient occlusion on)." },
    { L"shadows", L"Strength",      L"Shadow darkness",     0.00f, 1.00f, 0.50f, L"How dark shadows are. Zero = ghost shadows, one = film noir." },
    { L"shadows", L"GrassStrength", L"Shadows on grass",    0.00f, 1.00f, 0.35f, L"Shadow strength on grass blades. Keep it low or the lawn gets patchy." },
    { L"gi",      L"Strength",      L"Bounce light",        0.00f, 3.00f, 0.70f, L"How loudly light gossips between surfaces (needs Global illumination on)." },
    { L"fog",     L"Amount",        L"Fog amount",          0.00f, 3.00f, 1.00f, L"How much of the game's own fog you get. More fog = more mystery and less visibility." },
    { L"fog",     L"Distance",      L"Fog distance",        0.30f, 4.00f, 1.00f, L"How far you can see before the world dissolves into haze." },
    { L"atmos",   L"Intensity",     L"Distance haze",       0.00f, 4.00f, 2.20f, L"Dusty air that washes out far-away rocks and the painted mountains on the horizon. Lower = crisp postcard, higher = hot dusty afternoon (needs Volumetric fog on)." },
    { L"atmos",   L"SunIntensity",  L"Light shafts",        0.00f, 8.00f, 2.00f, L"Brightness of the sun beams cutting through the haze (needs Volumetric fog on)." },
    { L"atmos",   L"ContactShadows",L"Contact shadows",     0.00f, 1.00f, 0.00f, L"Tiny sharp shadows where things touch the ground (Ty's feet, fence posts). It is the one slider here that costs performance: zero is free, anything above that is not." },
    { L"tess",    L"Displacement",  L"Surface relief",      0.00f, 6.00f, 2.50f, L"How deep cracks, grain and rope strands are carved. High values look rugged, very high values look like cardboard." },
    { L"post",    L"Sharpen",       L"Sharpening",          0.00f, 1.00f, 0.35f, L"Edge crispness. Too much and you can count the pixels, too little and it looks like a watercolour." },
};
#define NSL (int)(sizeof g_sl / sizeof g_sl[0])
#define SL_CONTACT 12   // index of the slider that has a performance cost

