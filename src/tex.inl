// tex.inl - texture tags (R4 2026-10-13): GL texture id -> {known, emissive, transparent, character} from the user's list (texture_tags.csv).
// How a texture gets its name: the engine reads textures either as loose files (PC_External\<name>.dds) or from Data_PC.rkv at a known offset.
//  * kernel32 IAT hooks (CreateFileW, CloseHandle, SetFilePointerEx, ReadFile) track those handles and record every read buffer range -> name in a ring.
//  * glCompressedTexImage2D / glTexImage2D (level 0) look the data pointer up in the ring -> name -> flags (embedded table tex_tags.inc, built by
//    gen_tex_tags.ps1 from texture_tags.csv + the rkv index; tyrender\texture_tags.csv, if present, overrides flags by name).
// API: TexFlags(id) (raw tag bits), MatClass(id) (one class per texture), MatPolicy(id) (its row of kMatPolicy: every consumer asks the policy, not the bits).
//      ini [tex] Enable=1 Debug=0.  Class model: docs/MATERIAL_CLASSIFICATION_PLAN.md section 3 (phase 1: behaviour unchanged).
#include <vector>
#include <mutex>
#include <algorithm>
#include "tex_tags.inc"
enum { TF_KNOWN = 1, TF_EMISSIVE = 2, TF_TRANSPARENT = 4, TF_CHARACTER = 8, TF_DECAL = 16 };   // TF_DECAL: named in ini [decal] Names (not a CSV tag)
static int g_texEnable = 1, g_texDebug = 0, g_texSkinTags = 1, g_texLogUnknown = 0;   // g_texLogUnknown: ini [tex] LogUnknown (default 0): draws per untagged texture, by name, every 600 frames
static const GLuint kTexProgs = 8192;
static GLint g_progTyTexLoc[kTexProgs]; static float g_progTyTexLast[kTexProgs];   // per program: tyTex uniform location, last value (g_progDiffUnit = albedo sampler unit, in tyrender.cpp)
static GLint g_progTyWaterLoc[kTexProgs]; static float g_progTyWaterLast[kTexProgs]; static int g_texWaterNoFresnel; static int g_dbgWater = 0;   // [pbr] DebugWater bitmask (issue #5 bisect), see LoadPbrConfig   // ini [pbr] SkyFresnelSkipWater (default 0 = off): per-draw tyWater uniform, 1 = water-tagged draw
static std::unordered_map<std::string, uint8_t> g_tagByName;      // lowercase file name -> flags
static int g_spillLog = 0; static std::unordered_map<GLuint, unsigned> g_spillCnt; static std::unordered_map<GLuint, std::string> g_texNameById;   // #9 step 1 (SpillLogDraw)
// ini [decal] (#8 footprint glow): Clamp=0 (default off) caps the luminance of decal draws at Max; Names = file names that count as decals.
// Per program: tyDecal uniform (1 = decal draw), set per draw from the bound albedo flags (TexDrawPrep).
static int g_decalClamp = 0; static float g_decalMax = 0.9f; static std::vector<std::string> g_decalNames;
static GLint g_progTyDecalLoc[kTexProgs]; static float g_progTyDecalLast[kTexProgs];
static std::mutex g_texMu; static std::unordered_map<GLuint, std::string> g_texInfo;   // debug: id -> "name WxH"
static unsigned g_texUploads, g_texNamed, g_texTagged, g_texReads;

static uint8_t* g_texFlagArr = nullptr;                           // flags by GL texture id (ids are small ints)
static uint8_t* g_texClassArr = nullptr;                          // MatClass by GL texture id, filled at upload from the flags (0 = unknown)
static const GLuint kTexArr = 1 << 18;
static inline uint8_t TexFlags(GLuint id) { return (g_texEnable && g_texFlagArr && id < kTexArr) ? g_texFlagArr[id] : 0; }

// ---- material classes (plan section 3). One class per texture; one policy row per class.
// Precedence: emission follows the emissive flag alone; character only switches off relief/tessellation and GI colour bounce; transparent only
// adds see-through (fog, water). An emissive character is EMIT_CHARACTER: it glows, and it is not a colour source.
enum MatClass : uint8_t { MC_UNKNOWN = 0, MC_SOLID, MC_TRANSPARENT, MC_EMITTER, MC_TRANS_EMITTER, MC_CHARACTER, MC_EMIT_CHARACTER, MC_COUNT };
// Columns used by phase 1 consumers. giBits: [gi] ExcludeSources bit that removes the draw as a colour source (1 emissive, 2 transparent, 4 character).
// Columns for emission, AO, shadows and TAA (plan section 3) are added with their phases.
struct MatRow { uint8_t giBits; bool noRelief; bool character; bool emissive; bool waterTag; bool seeThru; };
static const MatRow kMatPolicy[MC_COUNT] = {
    //            giBits  noRelief character emissive waterTag seeThru
    /* UNKNOWN   */ {0,    0,       0,        0,       0,       0},
    /* SOLID     */ {0,    0,       0,        0,       0,       0},
    /* TRANSP    */ {2,    0,       0,        0,       1,       1},
    /* EMITTER   */ {1,    0,       0,        1,       1,       0},
    /* TRANS_EM  */ {3,    0,       0,        1,       1,       1},
    /* CHARACTER */ {4,    1,       1,        0,       0,       0},
    /* EMIT_CHAR */ {4,    1,       1,        1,       0,       0},
};
static uint8_t MatClassOfFlags(uint8_t fl) {
    if (!(fl & TF_KNOWN)) return MC_UNKNOWN;
    if (fl & TF_CHARACTER) return (fl & TF_EMISSIVE) ? MC_EMIT_CHARACTER : MC_CHARACTER;
    if (fl & TF_EMISSIVE) return (fl & TF_TRANSPARENT) ? MC_TRANS_EMITTER : MC_EMITTER;
    return (fl & TF_TRANSPARENT) ? MC_TRANSPARENT : MC_SOLID;
}
static inline uint8_t MatClass(GLuint id) { return (g_texEnable && g_texClassArr && id < kTexArr) ? g_texClassArr[id] : MC_UNKNOWN; }
static inline const MatRow& MatPolicy(GLuint id) { return kMatPolicy[MatClass(id)]; }
// Decal overlay (ini [decal] Names): a separate bit, not a class. Footprints (#8) read it through this helper.
static inline bool MatDecal(GLuint id) { return TexFlags(id) & TF_DECAL; }

static std::string TexBase(const std::string& p) {
    size_t s = p.find_last_of("\\/"); std::string b = s == std::string::npos ? p : p.substr(s + 1);
    for (auto& c : b) c = (char)tolower((unsigned char)c);
    return b;
}
static void LoadTexTags() {
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%styrender.ini", g_dir);
    g_texEnable = GetPrivateProfileIntA("tex", "Enable", 1, ini); g_texDebug = GetPrivateProfileIntA("tex", "Debug", 0, ini);
    g_texLogUnknown = g_texEnable && GetPrivateProfileIntA("tex", "LogUnknown", 0, ini);
    g_spillLog = g_texEnable && GetPrivateProfileIntA("spill", "LogEmitters", 0, ini);   // #9 step 1: log additive draws per texture (every 600 frames)
    g_texSkinTags = g_texEnable && GetPrivateProfileIntA("tex", "SkinnedByTag", 1, ini);   // skinned shaders get the detail code too; tags switch it per draw
    for (unsigned i = 0; i < kTexProgs; i++) g_progTyTexLoc[i] = g_progTyWaterLoc[i] = g_progTyDecalLoc[i] = -1;
    g_decalClamp = GetPrivateProfileIntA("decal", "Clamp", 0, ini);
    char dv[32]; GetPrivateProfileStringA("decal", "Max", "0.9", dv, sizeof dv, ini); g_decalMax = (float)atof(dv);
    char dn[512]; GetPrivateProfileStringA("decal", "Names", "ty_footprint.dds,ty_footprint2.dds", dn, sizeof dn, ini);
    g_decalNames.clear(); for (char* t = strtok(dn, ","); t; t = strtok(nullptr, ",")) { std::string x = TexBase(t); while (!x.empty() && x.back() == ' ') x.pop_back(); if (!x.empty()) g_decalNames.push_back(x); }
    if (!g_texFlagArr) g_texFlagArr = (uint8_t*)calloc(kTexArr, 1);
    if (!g_texClassArr) g_texClassArr = (uint8_t*)calloc(kTexArr, 1);
    g_tagByName.clear();
    for (auto& e : kTagNames) g_tagByName[e.n] = e.f;
    std::string csv; char pth[MAX_PATH]; snprintf(pth, sizeof pth, "%styrender\\texture_tags.csv", g_dir);
    int n = 0;
    if (ReadFileStr(pth, csv)) {   // optional user override: same format as the shipped list
        size_t pos = 0;
        while (pos < csv.size()) {
            size_t e = csv.find('\n', pos); if (e == std::string::npos) e = csv.size();
            std::string line = csv.substr(pos, e - pos); pos = e + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            std::vector<std::string> f; size_t q = 0;
            for (;;) { size_t c = line.find(',', q); f.push_back(line.substr(q, c == std::string::npos ? std::string::npos : c - q)); if (c == std::string::npos) break; q = c + 1; }
            if (f.size() < 5 || f[0].compare(0, 4, "path") == 0) continue;
            uint8_t fl = TF_KNOWN;
            if (f[2] == "yes") fl |= TF_EMISSIVE;
            if (f[3] == "yes") fl |= TF_TRANSPARENT;
            if (f[4] == "yes") fl |= TF_CHARACTER;
            g_tagByName[TexBase(f[1])] = fl; n++;
        }
    }
    logf("[tex] tags: %d embedded, %d overrides from file (enable=%d)", (int)(sizeof kTagNames / sizeof kTagNames[0]), n, g_texEnable);
}

// ---- file tracking (kernel32 IAT)
struct TexHandle { HANDLE h; int kind; std::string name; uint64_t pos; };   // kind 1 = Data_PC.rkv, 2 = loose .dds
static std::vector<TexHandle> g_texHandles;
struct TexRead { const BYTE* b; size_t n; const char* name; };
static TexRead g_texRing[8192]; static unsigned g_texRingW;
typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI* CloseHandle_t)(HANDLE);
typedef BOOL(WINAPI* SetFilePointerEx_t)(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);
typedef BOOL(WINAPI* ReadFile_t)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static CreateFileW_t o_CreateFileW; static CloseHandle_t o_CloseHandle; static SetFilePointerEx_t o_SetFilePointerEx; static ReadFile_t o_ReadFile;

static TexHandle* TexFindHandle(HANDLE h) { for (auto& t : g_texHandles) if (t.h == h) return &t; return nullptr; }
static HANDLE WINAPI h_CreateFileW(LPCWSTR f, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD fl, HANDLE t) {
    HANDLE r = o_CreateFileW(f, a, s, sa, d, fl, t);
    if (f && g_texEnable && r != INVALID_HANDLE_VALUE && (a & GENERIC_READ)) {
        char b[MAX_PATH * 2]; int n = WideCharToMultiByte(CP_UTF8, 0, f, -1, b, sizeof b, nullptr, nullptr);
        if (n > 5) {
            std::string p = TexBase(b); int kind = 0;
            if (p == "data_pc.rkv") kind = 1; else if (p.size() > 4 && p.compare(p.size() - 4, 4, ".dds") == 0) kind = 2;
            if (kind) { std::lock_guard<std::mutex> lk(g_texMu); g_texHandles.erase(std::remove_if(g_texHandles.begin(), g_texHandles.end(), [&](const TexHandle& x) { return x.h == r; }), g_texHandles.end()); g_texHandles.push_back({r, kind, p, 0}); }
        }
    }
    return r;
}
static BOOL WINAPI h_CloseHandle(HANDLE h) {
    if (!g_texHandles.empty()) { std::lock_guard<std::mutex> lk(g_texMu); g_texHandles.erase(std::remove_if(g_texHandles.begin(), g_texHandles.end(), [&](const TexHandle& x) { return x.h == h; }), g_texHandles.end()); }
    return o_CloseHandle(h);
}
static BOOL WINAPI h_SetFilePointerEx(HANDLE h, LARGE_INTEGER d, PLARGE_INTEGER np, DWORD m) {
    BOOL r = o_SetFilePointerEx(h, d, np, m);
    if (r && !g_texHandles.empty()) {
        std::lock_guard<std::mutex> lk(g_texMu);
        if (TexHandle* t = TexFindHandle(h)) { LARGE_INTEGER cur; LARGE_INTEGER z; z.QuadPart = 0; if (np) t->pos = (uint64_t)np->QuadPart; else if (o_SetFilePointerEx(h, z, &cur, FILE_CURRENT)) t->pos = (uint64_t)cur.QuadPart; }
    }
    return r;
}
static void TexRingAdd(const BYTE* b, size_t n, const char* name) { TexRead& e = g_texRing[g_texRingW++ & 8191]; e.b = b; e.n = n; e.name = name; }
static const char* TexNameOfEntry(unsigned idx) { return kTagNames[idx].n; }
static BOOL WINAPI h_ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov) {
    if (g_texHandles.empty()) return o_ReadFile(h, buf, n, got, ov);
    uint64_t startPos = 0; int kind = 0; std::string nm;
    { std::lock_guard<std::mutex> lk(g_texMu); if (TexHandle* t = TexFindHandle(h)) { kind = t->kind; nm = t->name; startPos = ov ? (((uint64_t)ov->OffsetHigh << 32) | ov->Offset) : t->pos; } }
    BOOL r = o_ReadFile(h, buf, n, got, ov);
    if (kind && r) {
        DWORD g = got ? *got : n;
        std::lock_guard<std::mutex> lk(g_texMu);
        TexHandle* t = TexFindHandle(h);
        if (t && !ov) t->pos = startPos + g;
        g_texReads++;
        if (kind == 2) { static std::set<std::string> keep; auto it = keep.insert(nm).first; TexRingAdd((const BYTE*)buf, g, it->c_str()); }
        else if (g) {   // Data_PC.rkv: every dds entry overlapping [startPos, startPos+g)
            uint64_t a = startPos, b = startPos + g;
            size_t lo = 0, hi = sizeof kRkvDds / sizeof kRkvDds[0];
            while (lo < hi) { size_t m = (lo + hi) / 2; if ((uint64_t)kRkvDds[m].off + kRkvDds[m].size <= a) lo = m + 1; else hi = m; }
            for (size_t i = lo; i < sizeof kRkvDds / sizeof kRkvDds[0] && kRkvDds[i].off < b; i++) {
                uint64_t s0 = std::max<uint64_t>(a, kRkvDds[i].off), s1 = std::min<uint64_t>(b, (uint64_t)kRkvDds[i].off + kRkvDds[i].size);
                if (s1 > s0) TexRingAdd((const BYTE*)buf + (s0 - a), (size_t)(s1 - s0), TexNameOfEntry(kRkvDds[i].idx));
            }
        }
    }
    return r;
}
static void TexInstallHooks(HMODULE exe) {
    PatchIAT(exe, "KERNEL32.dll", "CreateFileW", (void*)h_CreateFileW, (void**)&o_CreateFileW);
    PatchIAT(exe, "KERNEL32.dll", "CloseHandle", (void*)h_CloseHandle, (void**)&o_CloseHandle);
    PatchIAT(exe, "KERNEL32.dll", "SetFilePointerEx", (void*)h_SetFilePointerEx, (void**)&o_SetFilePointerEx);
    PatchIAT(exe, "KERNEL32.dll", "ReadFile", (void*)h_ReadFile, (void**)&o_ReadFile);
}

// ---- upload hooks: called with the texture currently bound on the active unit
static void TexOnUpload(GLuint id, int lvl, GLenum ifmt, int w, int h, const void* px, int bytes) {
    if (!g_texEnable || lvl != 0 || !id || id >= kTexArr || !g_texFlagArr || !px) return;
    g_texUploads++;
    const char* nm = nullptr;
    { std::lock_guard<std::mutex> lk(g_texMu);
      const BYTE* p = (const BYTE*)px;
      for (unsigned i = 0; i < 8192; i++) { const TexRead& e = g_texRing[(g_texRingW - 1 - i) & 8191]; if (!e.name) break; if (p >= e.b && p < e.b + e.n) { nm = e.name; break; } } }
    uint8_t fl = 0;
    if (!nm && g_texDebug && bytes > 0) {   // diagnose: nearest earlier ring buffer and the last 3 reads
        std::lock_guard<std::mutex> lk(g_texMu); const BYTE* p = (const BYTE*)px; const TexRead* best = nullptr;
        for (unsigned i = 0; i < 8192; i++) { const TexRead& e = g_texRing[(g_texRingW - 1 - i) & 8191]; if (!e.name) break; if (e.b <= p && (!best || e.b > best->b)) best = &e; }
        logf("[tex] MISS id=%u px=%p nearest=%p+%u %s ringW=%u", id, px, best ? best->b : nullptr, best ? (unsigned)best->n : 0, best ? best->name : "-", g_texRingW);
    }
    if (nm) { g_texNamed++; auto it = g_tagByName.find(nm); if (it != g_tagByName.end()) { fl = it->second; if (fl & TF_KNOWN) g_texTagged++; }
        if (g_decalClamp && std::find(g_decalNames.begin(), g_decalNames.end(), std::string(nm)) != g_decalNames.end()) fl |= TF_DECAL; }
    g_texFlagArr[id] = fl; g_texClassArr[id] = MatClassOfFlags(fl);
    if ((g_spillLog || g_texLogUnknown) && nm) g_texNameById[id] = nm;
    if (g_texDebug) { char b[160]; snprintf(b, sizeof b, "%s %dx%d ifmt=%#x", nm ? nm : "?", w, h, ifmt); g_texInfo[id] = b; }
    if (g_texDebug && g_texUploads < 500) logf("[tex] upload id=%u %dx%d ifmt=%#x bytes=%d px=%p name=%s flags=%u", id, w, h, ifmt, bytes, px, nm ? nm : "?", fl);
}
HOOK(void, glCompressedTexImage2D, (GLenum t, GLint lvl, GLenum ifmt, GLsizei w, GLsizei h, GLint b, GLsizei sz, const GLvoid* px)) {
    if (t == 0x0DE1 && g_activeUnit < 32) TexOnUpload(g_bound2D[g_activeUnit], lvl, ifmt, w, h, px, sz);
    o_glCompressedTexImage2D(t, lvl, ifmt, w, h, b, sz, px);
}
// ---- per draw: albedo texture of the bound program -> tyTex uniform (1 = no relief / bump / specular for this draw)
// Rules (policy column noRelief): character class -> 1.  Skinned program (boneMats): relief only if the texture is classed and not a character (unknown = stock).
// Non-skinned program: 0 unless character.  Without tags (Enable=0) skinned programs are not rewritten at all.
static unsigned g_texUntagCnt[1 << 18];   // debug: draws per untagged texture id
static unsigned char g_progKind[kTexProgs];   // 0 = not evaluated, 1 = skinned, 2 = world
static unsigned g_texDrawSet, g_texDrawChar, g_texDrawKnown, g_texDrawUnknown, g_texDraws;
// Water = policy column waterTag (emissive or transparent, not character). Only used when [pbr] SkyFresnelSkipWater=1; the uniform stays 0 otherwise.
static void TexSetWater(GLuint p, GLuint tex) {
    GLint loc = g_progTyWaterLoc[p]; if (!g_texWaterNoFresnel || loc < 0) return;
    float v = MatPolicy(tex).waterTag ? 1.0f : 0.0f;
    if (g_progTyWaterLast[p] == v) return;
    typedef void(WINAPI* pu1_t)(GLuint, GLint, GLfloat); static pu1_t pu1 = (pu1_t)o_wglGetProcAddress("glProgramUniform1f");
    if (!pu1) return;
    pu1(p, loc, v); g_progTyWaterLast[p] = v;
}
static void TexSetDecal(GLuint p, GLuint tex) {
    GLint loc = g_progTyDecalLoc[p]; if (loc < 0) return;
    float v = MatDecal(tex) ? 1.0f : 0.0f;
    if (g_progTyDecalLast[p] == v) return;
    typedef void(WINAPI* pu1_t)(GLuint, GLint, GLfloat); static pu1_t pu1 = (pu1_t)o_wglGetProcAddress("glProgramUniform1f");
    if (!pu1) return;
    pu1(p, loc, v); g_progTyDecalLast[p] = v;
}
static void TexDrawPrep() {
    GLuint p = g_curProg; if (p >= kTexProgs) return;
    GLuint tex = g_bound2D[g_progDiffUnit[p] & 31];
    if (g_decalClamp) TexSetDecal(p, tex);   // before the tyTex early-out: decal programs may have no PBR header
    GLint loc = g_progTyTexLoc[p]; if (loc < 0) return;
    if (!g_progKind[p]) g_progKind[p] = g_skinProgs.count(p) ? 1 : 2;
    uint8_t cls = MatClass(tex); const MatRow& row = kMatPolicy[cls]; float v;
    if (row.noRelief) v = 1.0f; else if (g_progKind[p] == 1) v = (cls == MC_UNKNOWN) ? 1.0f : 0.0f; else v = 0.0f;
    if ((g_texDebug || g_texLogUnknown) && cls == MC_UNKNOWN && tex < kTexArr) g_texUntagCnt[tex]++;
    g_texDraws++; if (row.character) g_texDrawChar++; else if (cls != MC_UNKNOWN) g_texDrawKnown++; else g_texDrawUnknown++;
    TexSetWater(p, tex);
    if (g_progTyTexLast[p] == v) return;
    typedef void(WINAPI* pu1_t)(GLuint, GLint, GLfloat); static pu1_t pu1 = (pu1_t)o_wglGetProcAddress("glProgramUniform1f");
    if (!pu1) return;
    pu1(p, loc, v); g_progTyTexLast[p] = v; g_texDrawSet++;
}
// #9 step 1 (portal / crystal light, design in docs/agent-tasks/light2.md): ini [spill] LogEmitters=1 counts the additive draws
// (glBlendFunc src SRC_ALPHA or ONE, dst ONE) per albedo texture, so the emitter key can be picked from data. Logging only, no visual change.
static void SpillLogDraw() {
    if (!g_spillLog || g_blendD != 1 || (g_blendS != 0x0302 && g_blendS != 1)) return;
    GLuint tex = g_bound2D[g_progDiffUnit[g_curProg < 8192 ? g_curProg : 0] & 31];
    g_spillCnt[tex]++;
}
// #8: decal luminance clamp (ini [decal] Clamp=1). Only shaders ending in the stock output line are rewritten; the uniform is set per draw (TexSetDecal).
static bool DecalRewriteFS(std::string& s) {
    if (!g_decalClamp) return false;
    const char* kOut = "FINAL_COLOR = diffuseColour;";
    if (s.find(kOut) == std::string::npos || s.find("void main") == std::string::npos || s.find("uniform float tyDecal;") != std::string::npos) return false;
    char blk[400];
    snprintf(blk, sizeof blk, "if (tyDecal > 0.5) { float tyDl = dot(diffuseColour.rgb, vec3(0.2126, 0.7152, 0.0722)); if (tyDl > %.4f) diffuseColour.rgb *= %.4f / tyDl; }\n  FINAL_COLOR = diffuseColour;", g_decalMax, g_decalMax);
    ReplaceAll(s, kOut, blk);
    s.insert(s.rfind("void main"), "uniform float tyDecal;\n");
    return true;
}
static void TexOnLink(GLuint p) {
    if (p >= kTexProgs) return;
    g_progTyDecalLoc[p] = o_glGetUniformLocation(p, "tyDecal"); g_progTyDecalLast[p] = -1.0f;
}
static void TexReport() {
    static unsigned last; if (g_frame - last < 600) return; last = g_frame;
    for (auto& kv : g_spillCnt) {
        auto it = g_texNameById.find(kv.first);
        logf("[spill] additive draws=%u tex=%u name=%s emissive=%d (last 600 frames)", kv.second, kv.first, it == g_texNameById.end() ? "?" : it->second.c_str(), MatPolicy(kv.first).emissive ? 1 : 0);
    }
    g_spillCnt.clear();
    logf("[tex] stats frame=%u reads=%u uploads(with data)=%u named=%u tagged=%u | relief-program draws since last: %u (character %u, tagged non-character %u, untagged %u) uniform sets %u", g_frame, g_texReads, g_texUploads, g_texNamed, g_texTagged, g_texDraws, g_texDrawChar, g_texDrawKnown, g_texDrawUnknown, g_texDrawSet);
    if (g_texDebug || g_texLogUnknown) {
        std::vector<std::pair<unsigned, GLuint>> v; for (GLuint i = 0; i < kTexArr; i++) if (g_texUntagCnt[i]) v.push_back({g_texUntagCnt[i], i});
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first > b.first; });
        for (size_t i = 0; i < v.size() && i < 25; i++) {
            auto it = g_texInfo.find(v[i].second); auto nt = g_texNameById.find(v[i].second);
            logf("[tex] untagged draws=%u id=%u name=%s %s", v[i].first, v[i].second, nt != g_texNameById.end() ? nt->second.c_str() : "?", it == g_texInfo.end() ? "(no upload seen)" : it->second.c_str());
        }
        memset(g_texUntagCnt, 0, sizeof g_texUntagCnt);
    }
    g_texDraws = g_texDrawChar = g_texDrawKnown = g_texDrawUnknown = g_texDrawSet = 0;
}
