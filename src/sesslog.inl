// sesslog.inl - per-session report file for testers. Written only to tyrender\logs\ next to the game, never sent anywhere.
// File: TYHD_report_<yyyymmdd-hhmmss>_<build>.txt, one per game start. Sections, in order:
//   1. header: what the file contains, build, and hardware/software basics (no names, IDs, paths or process lists);
//   2. frame-time rows, one per second, for the first [report] CaptureSeconds (default 300), then a summary;
//   3. event lines: GL errors and shader failures (first time logged, then counted), settings changes, F10 overlay,
//      screenshots, window size or fullscreen changes, and a "session end" line (clean exit or crash).
// Config tyrender.ini [report]: Enable=1 (0 = no file at all), CaptureSeconds=300, MaxBytes=1048576, KeepFiles=10.
// Every entry point returns early when the file is not open, and all I/O is best effort: a read-only folder just means no file.
#include <intrin.h>
#include <map>
#include <algorithm>
#include <string>
#include "build_id.h"   // TY_BUILD_ID_A, written by build.ps1

#ifndef TY_VERSION_A
#define TY_VERSION_A "v0.3.5-rc"   // keep in step with TY_VERSION in TYLauncher.c
#endif

static struct SessState {
    bool on = false, opened = false, perfDone = false, dead = false, capped = false;
    FILE* f = nullptr;
    std::string name;
    long long bytes = 0, maxBytes = 1048576;
    int capSec = 300, keep = 10;
    LARGE_INTEGER t0 = {}, prev = {};
    unsigned frames = 0, cur = 0, hitches = 0;
    int cw = -1, ch = -1, fs = -1;
    std::vector<float> bucket, all;
    double bGpu[5] = {}; unsigned bGpuN = 0;
    double wGpu[5] = {}; unsigned wGpuN = 0;
    std::map<unsigned, unsigned> glErr;
    std::map<std::string, unsigned> failKind;
    std::set<std::string> failSeen;
    std::map<std::string, std::string> iniPrev;
} SL;

static double SessSec(LARGE_INTEGER a, LARGE_INTEGER b) { return (double)(b.QuadPart - a.QuadPart) / (double)g_qpf.QuadPart; }

// Every write goes through here. Ordinary lines stop 4 KB before the size cap so the exit or crash line always fits.
static void SessRaw(const char* s, bool force) {
    if (!SL.f) return;
    long long n = (long long)strlen(s), limit = force ? SL.maxBytes : SL.maxBytes - 4096;
    if (SL.bytes + n > limit) {
        if (!SL.capped) { SL.capped = true; fputs("[cap] size limit reached, further events are dropped\n", SL.f); }
        return;
    }
    if (fputs(s, SL.f) < 0) { SL.dead = true; return; }
    SL.bytes += n;
}

static void SessText(bool force, const char* fmt, ...) {
    char b[1024]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
    SessRaw(b, force);
}

static void SessPut(bool force, const char* fmt, va_list a) {
    if (!SL.f) return;
    LARGE_INTEGER now; QueryPerformanceCounter(&now); double t = SessSec(SL.t0, now);
    char b[600]; vsnprintf(b, sizeof b, fmt, a);
    char line[700]; snprintf(line, sizeof line, "[%02d:%02d.%03d f%u] %s\n", (int)t / 60, (int)t % 60, (int)(t * 1000) % 1000, SL.frames, b);
    SessRaw(line, force);
}
static void SessEvent(const char* fmt, ...) { va_list a; va_start(a, fmt); SessPut(false, fmt, a); va_end(a); }
static void SessEventForce(const char* fmt, ...) { va_list a; va_start(a, fmt); SessPut(true, fmt, a); va_end(a); }

// A shader compile or link failure: the first occurrence of each distinct log is written, every occurrence is counted.
static void SessFailure(const char* kind, const char* log) {
    if (!SL.on) return;
    SL.failKind[kind]++;
    if (!SL.failSeen.insert(std::string(kind) + "|" + log).second) return;
    char d[161]; size_t j = 0;
    for (const char* p = log; *p && j < 160; p++) d[j++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
    d[j] = 0;
    SessEvent("shader failure (%s), first time: %s", kind, d);
}

// GL entry points by name (core 1.1 functions live in OPENGL32.dll; the rest come from wglGetProcAddress).
typedef GLenum(WINAPI* SessGetError_t)(void);
typedef void(WINAPI* SessGetIntegerv_t)(GLenum, GLint*);
static void* SessGL(const char* name) {
    static HMODULE gl = GetModuleHandleA("OPENGL32.dll");
    void* p = gl ? (void*)GetProcAddress(gl, name) : nullptr;
    if (!p && o_wglGetProcAddress) p = (void*)o_wglGetProcAddress(name);
    return p;
}
static const char* SessGlName(GLenum e) {
    switch (e) {
        case 0x0500: return "INVALID_ENUM";
        case 0x0501: return "INVALID_VALUE";
        case 0x0502: return "INVALID_OPERATION";
        case 0x0505: return "OUT_OF_MEMORY";
        case 0x0506: return "INVALID_FRAMEBUFFER_OPERATION";
    }
    return "unknown";
}
static void SessGlErrors() {
    static SessGetError_t ge = (SessGetError_t)SessGL("glGetError");
    if (!ge) return;
    for (int i = 0; i < 16; i++) {
        GLenum e = ge();
        if (!e) break;
        if (!SL.glErr[e]++) SessEvent("GL error 0x%04x %s (later repeats are counted, see session end)", (unsigned)e, SessGlName(e));
    }
}

// tyrender.ini as "section.key" -> value, comments and blank lines skipped. Missing file gives an empty map.
static std::string SessTrim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
static std::map<std::string, std::string> SessIniMap() {
    std::map<std::string, std::string> m;
    char path[MAX_PATH]; snprintf(path, sizeof path, "%styrender.ini", g_dir);
    FILE* f = fopen(path, "rb"); if (!f) return m;
    std::string txt; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) txt.append(b, n);
    fclose(f);
    std::string sec; size_t pos = 0;
    while (pos < txt.size()) {
        size_t e = txt.find('\n', pos); if (e == std::string::npos) e = txt.size();
        std::string line = SessTrim(txt.substr(pos, e - pos)); pos = e + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') { size_t r = line.find(']'); sec = line.substr(1, r == std::string::npos ? std::string::npos : r - 1); continue; }
        size_t eq = line.find('='); if (eq == std::string::npos) continue;
        m[sec + "." + SessTrim(line.substr(0, eq))] = SessTrim(line.substr(eq + 1));
    }
    return m;
}

// Called after every settings write (launcher slider, reload trigger, F10 overlay): logs what changed, at most 20 lines per write.
static void SessIniCheck(const char* why) {
    if (!SL.opened || SL.dead) return;
    std::map<std::string, std::string> cur = SessIniMap();
    int total = 0;
    for (auto& kv : cur) {
        auto it = SL.iniPrev.find(kv.first);
        if (it != SL.iniPrev.end() && it->second == kv.second) continue;
        if (++total <= 20) SessEvent("setting %s (%s): %s = %s", it == SL.iniPrev.end() ? "added" : "changed", why, kv.first.c_str(), kv.second.c_str());
    }
    for (auto& kv : SL.iniPrev)
        if (!cur.count(kv.first) && ++total <= 20) SessEvent("setting removed (%s): %s", why, kv.first.c_str());
    if (total > 20) SessEvent("... %d more setting changes not listed", total - 20);
    SL.iniPrev = cur;
}

// Frame times and GPU pass times (gpuprof.inl calls this once per GPU sample), grouped per second.
static void SessGpuSample(double frame, double ao, double gi, double atm, double post) {
    if (!SL.opened || SL.perfDone || SL.dead) return;
    SL.bGpu[0] += frame; SL.bGpu[1] += ao; SL.bGpu[2] += gi; SL.bGpu[3] += atm; SL.bGpu[4] += post; SL.bGpuN++;
}
static void SessFlushRow() {
    if (SL.bucket.empty()) return;
    std::vector<float> v = SL.bucket;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    double sum = 0; for (float x : v) sum += x;
    double avg = sum / n;
    size_t k = std::max<size_t>(1, n / 100); double w = 0; for (size_t i = 0; i < k; i++) w += v[n - 1 - i]; w /= k;
    for (float x : v) if (x > 50.0f) SL.hitches++;
    char g[200] = "";
    if (SL.bGpuN) {
        double d = SL.bGpuN;
        snprintf(g, sizeof g, " | gpu ms: frame %.2f ao %.2f gi %.2f atmos %.2f post %.2f", SL.bGpu[0] / d, SL.bGpu[1] / d, SL.bGpu[2] / d, SL.bGpu[3] / d, SL.bGpu[4] / d);
        for (int i = 0; i < 5; i++) SL.wGpu[i] += SL.bGpu[i];
        SL.wGpuN += SL.bGpuN;
    }
    SessText(false, "t=%4u s  frames %4zu  frame ms avg %6.2f min %6.2f max %7.2f  fps %6.1f  1%% low fps %6.1f%s\n",
        SL.cur, n, avg, (double)v[0], (double)v[n - 1], 1000.0 / avg, 1000.0 / w, g);
    SL.bucket.clear(); memset(SL.bGpu, 0, sizeof SL.bGpu); SL.bGpuN = 0;
}
static void SessSummary(bool partial, double secs) {
    std::vector<float> v = SL.all;
    SessText(false, "\n== performance summary: %s, %.0f s, %zu frames ==\n", partial ? "ended early" : "capture window", secs, v.size());
    if (v.empty()) { SessText(false, "no frames timed\n"); return; }
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    double sum = 0; for (float x : v) sum += x;
    double avg = sum / n;
    auto low = [&](double pct) { size_t k = std::max<size_t>(1, (size_t)(n * pct)); double s = 0; for (size_t i = 0; i < k; i++) s += v[n - 1 - i]; return 1000.0 / (s / k); };
    unsigned hitch = 0; for (float x : v) if (x > 50.0f) hitch++;
    SessText(false, "average frame %.2f ms (%.1f fps)\nmedian frame %.2f ms (%.1f fps)\n1%% low %.1f fps   0.1%% low %.1f fps\nworst frame %.2f ms\nhitches (frame over 50 ms) %u\n",
        avg, 1000.0 / avg, (double)v[n / 2], 1000.0 / v[n / 2], low(0.01), low(0.001), (double)v[n - 1], hitch);
    if (SL.wGpuN) {
        double d = SL.wGpuN;
        SessText(false, "gpu ms averages: frame %.2f ao %.2f gi %.2f atmos %.2f post %.2f\n", SL.wGpu[0] / d, SL.wGpu[1] / d, SL.wGpu[2] / d, SL.wGpu[3] / d, SL.wGpu[4] / d);
    }
    SessText(false, "\n== events after the capture window ==\n");
}

// Keeps the newest KeepFiles reports (names sort by time).
static void SessPrune(const char* dir) {
    std::vector<std::string> names;
    char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s\\TYHD_report_*.txt", dir);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) { do names.push_back(fd.cFileName); while (FindNextFileA(h, &fd)); FindClose(h); }
    std::sort(names.begin(), names.end());
    while ((int)names.size() > SL.keep) {
        char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\%s", dir, names.front().c_str());
        DeleteFileA(p); names.erase(names.begin());
    }
}

static void SessPerf(double ms, LARGE_INTEGER now) {
    double t = SessSec(SL.t0, now);
    if (t >= SL.capSec) {
        SessFlushRow();
        SessSummary(false, (double)SL.capSec);
        SL.perfDone = true;
        SessEvent("capture window closed after %d s, only events from here", SL.capSec);
        return;
    }
    unsigned sec = (unsigned)t;
    if (sec != SL.cur) { SessFlushRow(); SL.cur = sec; }
    SL.bucket.push_back((float)ms); SL.all.push_back((float)ms);
}

// System basics only: OS version, CPU brand string and core count, RAM size, display mode. No names of the computer or user.
typedef struct { DWORD sz, maj, min, build, plat; WCHAR csd[128]; } SessOSVI;
typedef LONG(WINAPI* SessRtlGetVersion_t)(SessOSVI*);
static void SessSystem() {
    SessOSVI v = {}; v.sz = sizeof v;
    SessRtlGetVersion_t rv = (SessRtlGetVersion_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (rv && rv(&v) == 0) SessText(false, "Windows: %lu.%lu build %lu\n", (unsigned long)v.maj, (unsigned long)v.min, (unsigned long)v.build);
    else SessText(false, "Windows: unknown\n");
    SYSTEM_INFO si = {}; GetNativeSystemInfo(&si);
    const char* arch = si.wProcessorArchitecture == 9 ? "64-bit (x64)" : si.wProcessorArchitecture == 12 ? "64-bit (ARM64)" : si.wProcessorArchitecture == 0 ? "32-bit (x86)" : "other";
    SessText(false, "OS architecture: %s\n", arch);
    int r[4] = {}; char brand[49] = {};
    __cpuid(r, 0x80000000);
    if ((unsigned)r[0] >= 0x80000004u) for (int i = 0; i < 3; i++) { __cpuid(r, 0x80000002 + i); memcpy(brand + 16 * i, r, 16); }
    char* b = brand; while (*b == ' ') b++;
    SessText(false, "CPU: %s, %u logical cores\n", b[0] ? b : "unknown", (unsigned)si.dwNumberOfProcessors);
    MEMORYSTATUSEX ms = {}; ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) SessText(false, "RAM: %llu MB\n", (unsigned long long)(ms.ullTotalPhys / (1024ull * 1024ull)));
    DEVMODEA dm = {}; dm.dmSize = sizeof dm;
    if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &dm))
        SessText(false, "display: %lux%lu at %lu Hz\n", (unsigned long)dm.dmPelsWidth, (unsigned long)dm.dmPelsHeight, (unsigned long)dm.dmDisplayFrequency);
}

static void SessGraphics() {
    const char* ven = (const char*)o_glGetString(GL_VENDOR);
    const char* ren = (const char*)o_glGetString(GL_RENDERER);
    const char* ver = (const char*)o_glGetString(GL_VERSION);
    const char* glsl = (const char*)o_glGetString(GL_SHADING_LANGUAGE_VERSION);
    SessText(false, "GPU vendor: %s\nGPU model (OpenGL renderer string, so the driver's own name; can be generic): %s\nOpenGL version (includes the driver version): %s\nGLSL: %s\n", ven ? ven : "?", ren ? ren : "?", ver ? ver : "?", glsl ? glsl : "?");
    SessGetIntegerv_t giv = (SessGetIntegerv_t)SessGL("glGetIntegerv");
    auto lim = [&](GLenum e) { GLint v = -1; if (giv) giv(e, &v); return (int)v; };
    SessText(false, "limits: max texture size %d, texture units %d, draw buffers %d, max samples %d\n", lim(0x0D33), lim(0x8872), lim(0x8824), lim(0x8D57));
    const char* ex = (const char*)o_glGetString(0x1F03);   // GL_EXTENSIONS
    std::string es = std::string(" ") + (ex ? ex : "") + " ";
    static const char* kExt[] = { "GL_ARB_compute_shader", "GL_ARB_tessellation_shader", "GL_ARB_shader_image_load_store", "GL_ARB_texture_storage", "GL_ARB_timer_query", "GL_ARB_direct_state_access", "GL_KHR_debug", "GL_ARB_bindless_texture" };
    std::string line = "extensions used by the mod:";
    for (const char* e : kExt) { line += " "; line += e; line += es.find(std::string(" ") + e + " ") != std::string::npos ? "=yes" : "=no"; }
    SessRaw((line + "\n").c_str(), false);
}

// Writes the header of a new report file: what it contains, then build, system, graphics, window and both settings files.
static void SessOpen(HDC dc) {
    char dir[MAX_PATH]; snprintf(dir, sizeof dir, "%styrender\\logs", g_dir);
    CreateDirectoryA(dir, nullptr);
    SYSTEMTIME st; GetLocalTime(&st);
    char nm[200]; snprintf(nm, sizeof nm, "TYHD_report_%04d%02d%02d-%02d%02d%02d_%s.txt", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, TY_BUILD_ID_A);
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\%s", dir, nm);
    SL.f = fopen(p, "wb");
    if (!SL.f) { SL.dead = true; return; }
    SL.name = nm; SL.opened = true;
    SessPrune(dir);
    LARGE_INTEGER now; QueryPerformanceCounter(&now); SL.t0 = now; SL.prev = now;

    SessText(false, "TY HD renderer session report\n\n");
    SessText(false, "WHAT THIS FILE CONTAINS\n");
    SessText(false, "  - build id and version of the mod, the preset, and the report settings\n");
    SessText(false, "  - Windows version and build, CPU model and core count, RAM size, display resolution and refresh rate\n");
    SessText(false, "  - GPU vendor, model and driver version (from OpenGL), OpenGL limits and a few extensions\n");
    SessText(false, "  - window size and fullscreen state, the game's settings.ini and the mod's tyrender.ini\n");
    SessText(false, "  - frame times and GPU pass times for the first minutes of play, then events (errors, settings changes, screenshots)\n");
    SessText(false, "NOT in this file: usernames, computer name, serial numbers, IP or MAC addresses, installed programs,\n");
    SessText(false, "running processes, file listings or any personal folder paths. Nothing is sent anywhere; you send it yourself.\n\n");

    SessText(false, "file: %s\nbuild: %s   version: %s\n", nm, TY_BUILD_ID_A, TY_VERSION_A);
    SYSTEMTIME lt; GetLocalTime(&lt);
    SessText(false, "started: %04d-%02d-%02d %02d:%02d:%02d (local time)\n", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond);
    char preset[64] = "custom"; { char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir); GetPrivateProfileStringA("shots", "Preset", "custom", preset, sizeof preset, ini); }
    SessText(false, "preset: %s\ncapture window: %d s, file cap %lld bytes, keep %d reports\n\n", preset, SL.capSec, SL.maxBytes, SL.keep);

    SessText(false, "== system ==\n"); SessSystem();
    SessText(false, "\n== graphics ==\n"); SessGraphics();
    HWND hw = WindowFromDC(dc); RECT rc = {};
    if (hw) GetClientRect(hw, &rc);
    int fsm = hw ? ((GetWindowLongA(hw, GWL_STYLE) & WS_POPUP) != 0) : 0;
    SessText(false, "\n== window ==\nclient area %ldx%ld, %s\n", rc.right, rc.bottom, fsm ? "fullscreen" : "windowed");
    SL.cw = rc.right; SL.ch = rc.bottom; SL.fs = fsm;

    SessText(false, "\n== game settings.ini (Documents\\TY the Tasmanian Tiger), comments left out ==\n");
    {
        char docs[MAX_PATH] = "";
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) {
            char path[MAX_PATH]; snprintf(path, sizeof path, "%s\\TY the Tasmanian Tiger\\settings.ini", docs);
            FILE* f = fopen(path, "rb");
            if (f) {
                char l[512]; int shown = 0;
                while (fgets(l, sizeof l, f) && shown < 200) {
                    std::string t = SessTrim(l);
                    if (t.empty() || t[0] == ';' || t[0] == '#') continue;
                    SessText(false, "%s\n", t.c_str()); shown++;
                }
                fclose(f);
            } else SessText(false, "(no settings.ini yet)\n");
        }
    }

    SessText(false, "\n== tyrender.ini, effective values as written (a key left out uses the built-in default) ==\n");
    SL.iniPrev = SessIniMap();
    std::string lastSec = "\x01";
    for (auto& kv : SL.iniPrev) {
        size_t dot = kv.first.find('.');
        std::string sec = kv.first.substr(0, dot), key = kv.first.substr(dot + 1);
        if (sec != lastSec) { SessText(false, "[%s]\n", sec.c_str()); lastSec = sec; }
        SessText(false, "%s = %s\n", key.c_str(), kv.second.c_str());
    }

    SessText(false, "\n== frame times, one row per second, then a summary ==\n");
    SL.perfDone = SL.capSec <= 0;
    if (SL.perfDone) SessText(false, "capture window is off ([report] CaptureSeconds=0)\n");
}

static void SessFrame(HDC dc) {
    if (!SL.on || SL.dead) return;
    if (!SL.opened) { SessOpen(dc); return; }
    if (!SL.f) { SL.dead = true; return; }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    double ms = SessSec(SL.prev, now) * 1000.0;
    SL.prev = now; SL.frames++;
    SessGlErrors();
    HWND hw = WindowFromDC(dc); RECT rc = {};
    if (hw && GetClientRect(hw, &rc)) {
        int fs = (GetWindowLongA(hw, GWL_STYLE) & WS_POPUP) != 0;
        if (rc.right != SL.cw || rc.bottom != SL.ch || fs != SL.fs) {
            if (SL.cw >= 0) SessEvent("window %dx%d %s -> %ldx%ld %s", SL.cw, SL.ch, SL.fs ? "fullscreen" : "windowed", rc.right, rc.bottom, fs ? "fullscreen" : "windowed");
            SL.cw = rc.right; SL.ch = rc.bottom; SL.fs = fs;
        }
    }
    if (!SL.perfDone) SessPerf(ms, now);
}

static void SessShutdown(const char* how) {
    if (!SL.f || SL.dead) return;
    if (!SL.perfDone) {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        SessFlushRow();
        SessSummary(true, SessSec(SL.t0, now));
        SL.perfDone = true;
    }
    unsigned glTotal = 0, failTotal = 0;
    for (auto& kv : SL.glErr) glTotal += kv.second;
    for (auto& kv : SL.failKind) failTotal += kv.second;
    for (auto& kv : SL.glErr) SessEventForce("GL error total 0x%04x %s: %u", (unsigned)kv.first, SessGlName(kv.first), kv.second);
    for (auto& kv : SL.failKind) SessEventForce("shader failure total %s: %u", kv.first.c_str(), kv.second);
    SessEventForce("session end: %s. frames %u, GL errors %u, shader failures %u, file %lld bytes", how, SL.frames, glTotal, failTotal, SL.bytes);
    fclose(SL.f); SL.f = nullptr; SL.dead = true;
}
static void SessCrash(DWORD code, const char* mod, unsigned long off) {
    if (!SL.f || SL.dead) return;
    const char* base = strrchr(mod, '\\'); base = base ? base + 1 : mod;
    SessEventForce("CRASH exception %08lx in %s +0x%lx", (unsigned long)code, base, off);
    SessShutdown("crash");
}

// [report] switches are read once at startup. Any failure leaves the mod exactly as it was before this file existed.
static void SessInit() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    SL.on = GetPrivateProfileIntA("report", "Enable", 1, ini) != 0;
    int cap = GetPrivateProfileIntA("report", "CaptureSeconds", 300, ini); SL.capSec = cap < 0 ? 0 : cap > 3600 ? 3600 : cap;
    int mb = GetPrivateProfileIntA("report", "MaxBytes", 1048576, ini); SL.maxBytes = mb < 65536 ? 65536 : mb;
    int kp = GetPrivateProfileIntA("report", "KeepFiles", 10, ini); SL.keep = kp < 1 ? 1 : kp;
}
