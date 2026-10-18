// Shared code of TY HD Renderer Setup.exe (setup.c) and TY HD Renderer Uninstall.exe (uninstall.c).
// Both work on one game folder. Everything the install creates or replaces is listed in
// <game>\tyrender\install.ini, so the uninstaller removes exactly those files and restores the originals.
// Options (both programs): --dir <folder>  --dry-run (logs the plan to a file next to the exe, changes nothing)  --quiet (no dialogs)
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <stdarg.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define GAME_NAME L"TY the Tasmanian Tiger"
#define MOD_NAME L"TY HD Renderer"
#define UNINSTALL_EXE L"TY HD Renderer Uninstall.exe"
#define SETUP_EXE L"TY HD Renderer Setup.exe"
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\TYHDRenderer"

static wchar_t g_dir[MAX_PATH];        // game folder (TY.exe lives here)
static wchar_t g_tyr[MAX_PATH];        // <game>\tyrender  (mod work folder: manifest, backups, reload flag)
static wchar_t g_log[MAX_PATH];        // log file (install.log in the game folder, or the dry-run log next to the exe)
static wchar_t g_self[MAX_PATH];       // this program's own folder
static int g_dry, g_quiet;
static const wchar_t* g_version = L"v0.3.5-rc";

static void Join(wchar_t* out, const wchar_t* dir, const wchar_t* name) { swprintf(out, MAX_PATH, L"%ls%ls", dir, name); }
static void JoinDir(wchar_t* out, const wchar_t* dir, const wchar_t* name) { swprintf(out, MAX_PATH, L"%ls%ls\\", dir, name); }
static int Exists(const wchar_t* p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

// Message boxes are skipped in --quiet mode; the log always gets the text.
static int Say(const wchar_t* text, UINT icon, UINT buttons) {
    if (g_quiet) return IDYES;
    return MessageBoxW(NULL, text, MOD_NAME, icon | buttons);
}
static void Log(const wchar_t* fmt, ...) {
    wchar_t msg[1200]; va_list ap; va_start(ap, fmt); vswprintf(msg, 1200, fmt, ap); va_end(ap);
    if (g_log[0]) {
        HANDLE h = CreateFileW(g_log, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            SetFilePointer(h, 0, NULL, FILE_END);
            char u[4000]; int n = WideCharToMultiByte(CP_UTF8, 0, msg, -1, u, sizeof u, NULL, NULL);
            DWORD wr = 0; if (n > 1) WriteFile(h, u, n - 1, &wr, NULL);
            WriteFile(h, "\r\n", 2, &wr, NULL); CloseHandle(h);
        }
    }
}
static void Fail(const wchar_t* what) { wchar_t m[700]; swprintf(m, 700, L"%ls\n\n(error %lu)", what, GetLastError()); Log(L"FAIL %ls (error %lu)", what, GetLastError()); Say(m, MB_ICONERROR, MB_OK); }

// Creates <game>\tyrender and sets the log path. Dry runs write only the log next to the exe.
static void SetupLog(void) {
    JoinDir(g_tyr, g_dir, L"tyrender");
    if (g_dry) Join(g_log, g_self, L"dry-run.log");
    else { CreateDirectoryW(g_tyr, NULL); Join(g_log, g_tyr, L"install.log"); }
}

// Command line: --dir <folder>, --dry-run, --quiet. Returns 0 when the folder was given.
static int ParseArgs(wchar_t* dirOut) {
    int argc; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc); int have = 0;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--dry-run")) g_dry = 1;
        else if (!wcscmp(argv[i], L"--quiet")) g_quiet = 1;
        else if (!wcscmp(argv[i], L"--dir") && i + 1 < argc) { wcscpy(dirOut, argv[++i]); have = 1; }
    }
    if (have) { size_t L = wcslen(dirOut); if (L && dirOut[L - 1] != L'\\') wcscat(dirOut, L"\\"); }
    LocalFree(argv); return have;
}

static void SelfFolder(void) {
    GetModuleFileNameW(NULL, g_self, MAX_PATH); wchar_t* s = wcsrchr(g_self, L'\\'); if (s) s[1] = 0;
}

// ------------------------------------------------------------ embedded payload
static const BYTE* Res(int id, DWORD* size) {
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA); if (!r) return NULL;
    HGLOBAL g = LoadResource(NULL, r); if (!g) return NULL;
    *size = SizeofResource(NULL, r); return (const BYTE*)LockResource(g);
}
static int WriteBytes(const wchar_t* path, const BYTE* data, DWORD size) {
    HANDLE w = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (w == INVALID_HANDLE_VALUE) return 0;
    DWORD wr = 0; int ok = WriteFile(w, data, size, &wr, NULL) && wr == size; CloseHandle(w); return ok;
}

// ------------------------------------------------------------ manifest: <game>\tyrender\install.ini
// [install] version/build/date, [files] name=new (created by the install: delete on uninstall)
// or name=backup (replaced: the original is in tyrender\backup\name and is restored on uninstall).
static void ManifestPath(wchar_t* p) { Join(p, g_tyr, L"install.ini"); }
static void ManifestSet(const wchar_t* key, const wchar_t* val) { if (g_dry) return; wchar_t p[MAX_PATH]; ManifestPath(p); WritePrivateProfileStringW(L"files", key, val, p); }
static int ManifestHas(const wchar_t* key) { wchar_t p[MAX_PATH], b[16]; ManifestPath(p); GetPrivateProfileStringW(L"files", key, L"", b, 16, p); return b[0] != 0; }
static int ManifestExists(void) { wchar_t p[MAX_PATH]; ManifestPath(p); return Exists(p); }

// ------------------------------------------------------------ game processes (never change files under a running game)
static int GameRunning(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof pe }; int found = 0;
    if (Process32FirstW(snap, &pe)) do {
        if (!_wcsicmp(pe.szExeFile, L"TY.exe") || !_wcsicmp(pe.szExeFile, L"TY_tyrender.exe")) found = 1;
    } while (!found && Process32NextW(snap, &pe));
    CloseHandle(snap); return found;
}

// ------------------------------------------------------------ registry: Apps and Features entry (per user, no admin)
static void RegSetStr(HKEY k, const wchar_t* name, const wchar_t* val) { RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t))); }
static void RegSetDword(HKEY k, const wchar_t* name, DWORD v) { RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof v); }
static void RegAddEntry(void) {
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS) { Log(L"registry: cannot create entry"); return; }
    wchar_t un[MAX_PATH + 40], icon[MAX_PATH], loc[MAX_PATH];
    swprintf(un, MAX_PATH + 40, L"\"%ls%ls\"", g_dir, UNINSTALL_EXE); Join(icon, g_dir, L"TYLauncher.exe");
    wcscpy(loc, g_dir);
    RegSetStr(k, L"DisplayName", L"TY HD Renderer (mod)");   // distinct from the game's own uninstall entry
    RegSetStr(k, L"DisplayVersion", g_version);
    RegSetStr(k, L"Publisher", L"TY HD Renderer");
    RegSetStr(k, L"InstallLocation", loc);
    RegSetStr(k, L"DisplayIcon", icon);
    RegSetStr(k, L"UninstallString", un);
    RegSetDword(k, L"NoModify", 1); RegSetDword(k, L"NoRepair", 1);
    RegCloseKey(k); Log(L"registry: entry added");
}
static void RegRemoveEntry(void) { RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_KEY); Log(L"registry: entry removed"); }

// ------------------------------------------------------------ settings file: add missing keys only
// Walks the bundled default ini; a key the user already has keeps its value. Missing keys get the default.
static int IniMergeMissing(const BYTE* data, DWORD size, const wchar_t* ini) {
    char* text = (char*)malloc(size + 1); if (!text) return 0; memcpy(text, data, size); text[size] = 0;
    wchar_t sec[64] = L"", key[128], val[512], probe[4];
    int added = 0;
    for (char* line = strtok(text, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char* s = line; while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == ';') continue;
        if (*s == '[') { char* e = strchr(s, ']'); if (!e) continue; *e = 0; MultiByteToWideChar(CP_UTF8, 0, s + 1, -1, sec, 64); continue; }
        char* eq = strchr(s, '='); if (!eq) continue;
        char* ke = eq; while (ke > s && (ke[-1] == ' ' || ke[-1] == '\t')) ke--; *ke = 0;
        char* vs = eq + 1; while (*vs == ' ' || *vs == '\t') vs++;
        MultiByteToWideChar(CP_UTF8, 0, s, -1, key, 128); MultiByteToWideChar(CP_UTF8, 0, vs, -1, val, 512);
        GetPrivateProfileStringW(sec, key, L"\x01", probe, 4, ini);
        if (!wcscmp(probe, L"\x01")) { if (!g_dry) WritePrivateProfileStringW(sec, key, val, ini); Log(L"settings: added [%ls] %ls=%ls", sec, key, val); added++; }
    }
    free(text); return added;
}

// ------------------------------------------------------------ Steam: find the game folder
// Looks at the Steam install and every library listed in libraryfolders.vdf. Returns 1 and the folder with TY.exe in it.
static int FindSteamGame(wchar_t* out) {
    HKEY k; wchar_t steam[MAX_PATH] = L""; DWORD n = sizeof steam;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"SteamPath", NULL, NULL, (BYTE*)steam, &n); RegCloseKey(k);
    }
    if (!steam[0]) return 0;
    for (wchar_t* p = steam; *p; p++) if (*p == L'/') *p = L'\\';
    wchar_t libs[MAX_PATH]; Join(libs, steam, L"\\steamapps\\libraryfolders.vdf");
    wchar_t cand[MAX_PATH];
    swprintf(cand, MAX_PATH, L"%ls\\steamapps\\common\\%ls\\", steam, GAME_NAME);
    if (Exists(cand)) { wchar_t ty[MAX_PATH]; Join(ty, cand, L"TY.exe"); if (Exists(ty)) { wcscpy(out, cand); return 1; } }
    HANDLE f = CreateFileW(libs, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD size = GetFileSize(f, NULL), rd = 0; char* b = (char*)malloc(size + 1); if (!b) { CloseHandle(f); return 0; }
    ReadFile(f, b, size, &rd, NULL); CloseHandle(f); b[rd] = 0;
    // "path"<tabs>"D:\\SteamLibrary"  (escaped backslashes)
    for (char* p = strstr(b, "\"path\""); p; p = strstr(p + 6, "\"path\"")) {
        char* q = p + 6; while (*q && *q != '"') q++; if (!*q) break; q++;
        char lib[MAX_PATH] = ""; int li = 0;
        for (; *q && *q != '"' && li < MAX_PATH - 1; q++) { if (*q == '\\' && q[1]) q++; lib[li++] = *q; }
        lib[li] = 0; wchar_t wl[MAX_PATH]; MultiByteToWideChar(CP_UTF8, 0, lib, -1, wl, MAX_PATH);
        swprintf(cand, MAX_PATH, L"%ls\\steamapps\\common\\%ls\\", wl, GAME_NAME);
        wchar_t ty[MAX_PATH]; Join(ty, cand, L"TY.exe");
        if (Exists(ty)) { wcscpy(out, cand); free(b); return 1; }
    }
    free(b); return 0;
}

// Folder picker for a manual install. Returns 1 with the chosen folder.
static int BrowseGame(wchar_t* out) {
    BROWSEINFOW bi = {0}; wchar_t name[MAX_PATH];
    bi.lpszTitle = L"Choose the folder that contains TY.exe (the game folder)"; bi.pszDisplayName = name; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi); if (!pidl) return 0;
    wchar_t p[MAX_PATH] = L""; int ok = SHGetPathFromIDListW(pidl, p); CoTaskMemFree(pidl);
    if (!ok || !p[0]) return 0;
    size_t L = wcslen(p); if (p[L - 1] != L'\\') wcscat(p, L"\\"); wcscpy(out, p); return 1;
}

// ------------------------------------------------------------ small file helpers
static void InitCom(void) { CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); }
static void MkDirs(const wchar_t* dir) { CreateDirectoryW(dir, NULL); }
// Saves the original of a file the install is about to replace. Returns 0 = backed up, 2 = no original (a new file), -1 = failed.
static int BackupOriginal(const wchar_t* name) {
    wchar_t src[MAX_PATH], dstDir[MAX_PATH], dst[MAX_PATH];
    Join(src, g_dir, name); if (!Exists(src)) return 2;
    JoinDir(dstDir, g_tyr, L"backup"); MkDirs(dstDir); Join(dst, dstDir, name);
    Log(L"backup: %ls -> %ls", src, dst);
    if (g_dry) return 0;
    return CopyFileW(src, dst, FALSE) ? 0 : -1;
}
