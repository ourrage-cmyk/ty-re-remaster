// TY HD Renderer Setup.exe: one file that installs the HD renderer into the game folder.
// It finds the game (Steam libraries first, else a folder picker), backs up every file it replaces, writes
// tyrender.dll, TYLauncher.exe, the uninstaller, TY_tyrender.exe (patched copy of TY.exe; TY.exe is never changed)
// and tyrender.ini (missing keys only; existing settings are kept), then adds an Apps and Features entry.
// Payload resources (build.ps1 / build_setup.ps1): 1 TYLauncher.exe, 2 TY HD Renderer Uninstall.exe, 3 tyrender.dll, 4 default_tyrender.ini
#include "build_id.h"   // TY_BUILD_ID, written by build.ps1
#include "common.inc"
#include "../patch_exe.inc"   // PatchExe (also used by TYLauncher.exe)

#define RES_LAUNCHER 1
#define RES_UNINSTALL 2
#define RES_DLL 3
#define RES_INI 4

static int Payload(int id, const wchar_t* name) {
    DWORD size = 0; const BYTE* d = Res(id, &size);
    if (!d || !size) { wchar_t m[200]; swprintf(m, 200, L"Setup is damaged: missing payload %ls.", name); Fail(m); return 0; }
    wchar_t dst[MAX_PATH]; Join(dst, g_dir, name);
    if (!ManifestHas(name)) {
        int rc = BackupOriginal(name);
        if (rc < 0) { Fail(L"Could not back up an existing file. Close programs that may use it and try again."); return 0; }
        ManifestSet(name, rc == 0 ? L"backup" : L"new");
    }
    Log(L"write: %ls (%lu bytes)", dst, size);
    if (!g_dry && !WriteBytes(dst, d, size)) { Fail(L"Could not write a file into the game folder. Close the game and any running launcher, then try again."); return 0; }
    return 1;
}

static int InstallPatched(void) {
    wchar_t in[MAX_PATH], out[MAX_PATH], err[300] = L"";
    Join(in, g_dir, L"TY.exe"); Join(out, g_dir, L"TY_tyrender.exe");
    if (!ManifestHas(L"TY_tyrender.exe")) {
        int rc = BackupOriginal(L"TY_tyrender.exe");
        if (rc < 0) { Fail(L"Could not back up TY_tyrender.exe."); return 0; }
        ManifestSet(L"TY_tyrender.exe", rc == 0 ? L"backup" : L"new");
    }
    Log(L"patch: %ls -> %ls", in, out);
    if (g_dry) return 1;
    if (!PatchExe(in, out, err, 300)) { wchar_t m[400]; swprintf(m, 400, L"Could not make the renderer copy of TY.exe: %ls", err); Fail(m); return 0; }
    return 1;
}

static int InstallSettings(void) {
    wchar_t ini[MAX_PATH]; Join(ini, g_dir, L"tyrender.ini");
    DWORD size = 0; const BYTE* d = Res(RES_INI, &size);
    if (!d || !size) { Fail(L"Setup is damaged: missing default settings."); return 0; }
    if (!Exists(ini)) {
        ManifestSet(L"tyrender.ini", L"new");
        Log(L"settings: new tyrender.ini");
        if (!g_dry && !WriteBytes(ini, d, size)) { Fail(L"Could not write tyrender.ini."); return 0; }
        return 1;
    }
    if (!ManifestHas(L"tyrender.ini")) {
        int rc = BackupOriginal(L"tyrender.ini");
        if (rc < 0) { Fail(L"Could not back up tyrender.ini."); return 0; }
        ManifestSet(L"tyrender.ini", L"backup");
    }
    int added = IniMergeMissing(d, size, ini);   // keys already in the file keep their values
    Log(L"settings: %d missing key(s) added", added);
    // The shipped ini used to have [overlay] Key=0 (the F10 panel was off). Installs that still have that get F10; any other key is the user's choice.
    if (GetPrivateProfileIntW(L"overlay", L"Key", -1, ini) == 0 && !g_dry) {
        WritePrivateProfileStringW(L"overlay", L"Key", L"F10", ini); Log(L"settings: [overlay] Key 0 -> F10 (overlay panel on by default)");
    }
    return 1;
}

static void WriteInstallInfo(void) {
    if (g_dry) return;
    SYSTEMTIME st; GetLocalTime(&st); wchar_t when[64], p[MAX_PATH];
    swprintf(when, 64, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    ManifestPath(p);
    WritePrivateProfileStringW(L"install", L"version", g_version, p);
    WritePrivateProfileStringW(L"install", L"build", TY_BUILD_ID, p);
    WritePrivateProfileStringW(L"install", L"date", when, p);
}

static int DoInstall(void) {
    if (!g_dry) MkDirs(g_tyr);
    WriteInstallInfo();
    if (!Payload(RES_LAUNCHER, L"TYLauncher.exe")) return 0;
    if (!Payload(RES_DLL, L"tyrender.dll")) return 0;
    if (!Payload(RES_UNINSTALL, UNINSTALL_EXE)) return 0;
    if (!InstallPatched()) return 0;
    if (!InstallSettings()) return 0;
    if (!g_dry) RegAddEntry();
    return 1;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cmd, int show) {
    (void)hi; (void)hp; (void)cmd; (void)show;
    InitCom(); SelfFolder();
    wchar_t arg[MAX_PATH] = L"";
    int haveDir = ParseArgs(arg);
    if (haveDir) wcscpy(g_dir, arg);
    else if (g_quiet) { return 2; }
    else if (FindSteamGame(g_dir)) {
        wchar_t q[MAX_PATH + 300]; swprintf(q, MAX_PATH + 300, L"Found %ls here:\n\n%ls\n\nInstall the HD renderer into this folder?\n\nYes = install here.   No = choose another folder.   Cancel = quit.", GAME_NAME, g_dir);
        int c = Say(q, MB_ICONQUESTION, MB_YESNOCANCEL);
        if (c == IDCANCEL) return 0;
        if (c == IDNO && !BrowseGame(g_dir)) return 0;
    } else if (!BrowseGame(g_dir)) return 0;
    wchar_t exe[MAX_PATH]; Join(exe, g_dir, L"TY.exe");
    if (!Exists(exe)) { wchar_t m[MAX_PATH + 200]; swprintf(m, MAX_PATH + 200, L"TY.exe was not found in:\n%ls\n\nChoose the folder that contains TY.exe.", g_dir); Say(m, MB_ICONERROR, MB_OK); return 1; }
    SetupLog();
    Log(L"=== setup %ls build %ls dir=%ls dry=%d", g_version, TY_BUILD_ID, g_dir, g_dry);
    if (GameRunning()) { Fail(L"TY the Tasmanian Tiger is running. Close the game first, then run setup again."); return 1; }
    wchar_t q[MAX_PATH + 600];
    swprintf(q, MAX_PATH + 600, L"Install the TY HD Renderer into:\n\n%ls\n\nFiles that exist are backed up in tyrender\\backup. Your settings file is kept and only missing settings are added. TY.exe is not changed.%ls\n\nContinue?",
        g_dir, g_dry ? L"\n\n(Dry run: nothing is changed.)" : L"");
    if (Say(q, MB_ICONQUESTION, MB_YESNO) != IDYES) { Log(L"cancelled"); return 0; }
    if (!DoInstall()) { Log(L"install stopped with errors"); return 1; }
    Log(L"install done");
    wchar_t done[MAX_PATH * 2 + 800];
    swprintf(done, MAX_PATH * 2 + 800,
        L"TY HD Renderer %ls is installed%ls.\n\nPlay: start TYLauncher.exe in the game folder.\n\nSteam: right-click the game > Properties > Launch options, and paste:\n\"%lsTYLauncher.exe\" --play %%command%%\n\nUninstall: \"%ls\" in the game folder, or Apps and Features > TY HD Renderer (mod).",
        g_version, g_dry ? L" (dry run, nothing changed)" : L"", g_dir, UNINSTALL_EXE);
    Say(done, MB_ICONINFORMATION, MB_OK);
    return 0;
}
