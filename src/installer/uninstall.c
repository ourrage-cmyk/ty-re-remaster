// TY HD Renderer Uninstall.exe: removes only what TY HD Renderer Setup.exe put in the game folder.
// Files the install added are deleted. Files it replaced get their originals back from tyrender\backup.
// The game's own files (TY.exe, its data) are never touched. Then the Apps and Features entry is removed.
#include "common.inc"

static int g_errors;

// Copies the saved original back over a replaced file (or deletes a file the install created).
static void RestoreOne(const wchar_t* name, const wchar_t* how) {
    wchar_t dst[MAX_PATH], bak[MAX_PATH], bdir[MAX_PATH];
    Join(dst, g_dir, name); JoinDir(bdir, g_tyr, L"backup"); Join(bak, bdir, name);
    if (!_wcsicmp(how, L"backup")) {
        Log(L"restore: %ls <- %ls", dst, bak);
        if (!g_dry && !CopyFileW(bak, dst, FALSE)) { Log(L"FAIL restore %ls (error %lu)", name, GetLastError()); g_errors++; }
        if (!g_dry) DeleteFileW(bak);
    } else {
        Log(L"delete: %ls", dst);
        if (!g_dry && Exists(dst) && !DeleteFileW(dst)) { Log(L"FAIL delete %ls (error %lu)", name, GetLastError()); g_errors++; }
    }
}

// Runs through the manifest's [files] section. Keeps tyrender.ini when the user asked to keep their settings.
static void RestoreAll(int keepSettings) {
    wchar_t p[MAX_PATH], buf[8192] = L""; ManifestPath(p);
    GetPrivateProfileSectionW(L"files", buf, 8192, p);
    for (const wchar_t* e = buf; *e; e += wcslen(e) + 1) {
        const wchar_t* eq = wcschr(e, L'='); if (!eq) continue;
        wchar_t name[MAX_PATH] = L""; wcsncpy(name, e, eq - e); name[eq - e] = 0;
        if (!_wcsicmp(name, UNINSTALL_EXE)) continue;               // this program: removed last, after it exits
        if (keepSettings && !_wcsicmp(name, L"tyrender.ini")) {   // keep the user's settings; drop the saved original
            wchar_t bak[MAX_PATH], bdir[MAX_PATH]; JoinDir(bdir, g_tyr, L"backup"); Join(bak, bdir, name);
            Log(L"kept: tyrender.ini (user settings)"); if (!g_dry) DeleteFileW(bak); continue;
        }
        RestoreOne(name, eq + 1);
    }
}

// Mod-made files that are not in the manifest: the runtime log and reload flag. Removed only when they exist.
static void RemoveRuntimeFiles(void) {
    wchar_t p[MAX_PATH];
    Join(p, g_dir, L"tyrender.log"); if (Exists(p)) { Log(L"delete: %ls", p); if (!g_dry) DeleteFileW(p); }
    Join(p, g_tyr, L"reload");       if (Exists(p)) { Log(L"delete: %ls", p); if (!g_dry) DeleteFileW(p); }
}

// This exe cannot delete itself while it runs. A detached cmd waits a moment, then removes it (or puts its original back),
// then clears the install record and the work folder if nothing else is left in it (rmdir never deletes user files).
static void ScheduleSelfRemoval(void) {
    wchar_t self[MAX_PATH], bak[MAX_PATH], bdir[MAX_PATH], log[MAX_PATH], tmp[MAX_PATH], ini[MAX_PATH], cmd[2048];
    Join(self, g_dir, UNINSTALL_EXE); JoinDir(bdir, g_tyr, L"backup"); Join(bak, bdir, UNINSTALL_EXE);
    Join(log, g_tyr, L"install.log"); Join(ini, g_tyr, L"install.ini"); GetTempPathW(MAX_PATH, tmp);
    wchar_t selfAction[MAX_PATH * 3];
    if (Exists(bak)) swprintf(selfAction, MAX_PATH * 3, L"move /y \"%ls\" \"%ls\"", bak, self);
    else swprintf(selfAction, MAX_PATH * 3, L"del /f /q \"%ls\"", self);
    wchar_t bdirOnly[MAX_PATH]; JoinDir(bdirOnly, g_tyr, L"backup");
    swprintf(cmd, 2048, L"cmd.exe /d /c ping -n 3 127.0.0.1 >nul & %ls & del /f /q \"%ls\" & del /f /q \"%ls\" & rmdir \"%ls\" 2>nul & rmdir \"%ls\" 2>nul",
        selfAction, ini, log, bdirOnly, g_tyr);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, tmp, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    else Log(L"FAIL could not start the cleanup step (error %lu); remove the files by hand", GetLastError());
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, LPWSTR cmd, int show) {
    (void)hi; (void)hp; (void)cmd; (void)show;
    InitCom(); SelfFolder();
    wchar_t arg[MAX_PATH] = L"";
    if (ParseArgs(arg)) wcscpy(g_dir, arg); else wcscpy(g_dir, g_self);
    SetupLog();
    Log(L"=== uninstall %ls dir=%ls dry=%d", g_version, g_dir, g_dry);
    if (!ManifestExists()) {
        Say(L"TY HD Renderer is not installed in this folder (no tyrender\\install.ini record), so there is nothing to remove.\n\nIf you installed the files by hand, delete tyrender.dll, TYLauncher.exe and TY_tyrender.exe from the game folder.", MB_ICONINFORMATION, MB_OK);
        return 1;
    }
    if (GameRunning()) { Fail(L"TY the Tasmanian Tiger is running. Close the game first, then uninstall again."); return 1; }
    wchar_t q[MAX_PATH + 400];
    swprintf(q, MAX_PATH + 400, L"Remove the TY HD Renderer from:\n\n%ls\n\nFiles the install added are deleted. Files it replaced get their original back. The original game is not touched.\n\nContinue?", g_dir);
    if (Say(q, MB_ICONQUESTION, MB_YESNO) != IDYES) { Log(L"cancelled"); return 0; }
    int keep = 0;
    if (!g_quiet) keep = Say(L"Keep your renderer settings (tyrender.ini)?\n\nYes = keep the file as it is now.\nNo = restore the settings file from before the install (or delete it if there was none).", MB_ICONQUESTION, MB_YESNO) == IDYES;
    RestoreAll(keep);
    RemoveRuntimeFiles();
    if (!g_dry) {
        wchar_t p[MAX_PATH]; ManifestPath(p); DeleteFileW(p);   // the record goes last, after the restores
        RegRemoveEntry();
        ScheduleSelfRemoval();
    }
    Log(g_errors ? L"uninstall finished with %d error(s)" : L"uninstall done");
    if (g_errors) { wchar_t m[300]; swprintf(m, 300, L"TY HD Renderer was removed, but %d file(s) could not be restored or deleted. The game may be running. See tyrender\\install.log.", g_errors); Say(m, MB_ICONWARNING, MB_OK); return 1; }
    Say(L"TY HD Renderer has been removed. The game is back to its original state.\n\nIf you added the Steam launch option, remove it in Steam (Properties > Launch options).", MB_ICONINFORMATION, MB_OK);
    return 0;
}
