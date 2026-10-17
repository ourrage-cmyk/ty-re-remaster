// TYLauncher.exe — TY HD renderer launcher: graphics settings (writes tyrender.ini) + starts the game with tyrender.dll.
// Start method: if TY_tyrender.exe (TY.exe copy that imports tyrender.dll) exists it is started; the launcher can create it
// ("Patch" button = same algorithm as SRC\tyrender\patch_imports.ps1, TY.exe itself is never modified). Fallback: start TY.exe
// suspended and inject tyrender.dll (some security tools/sandboxes block this).
// Command line: --play (skip the window, start with current settings)  --wait (wait for game exit)  --noinject (stock game)
// Everything else on the command line is passed to the game.
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <shlobj.h>

static wchar_t g_dir[MAX_PATH], g_ini[MAX_PATH], g_args[4096];
static int g_wait;

// ------------------------------------------------------------ import-table patch (TY.exe -> TY_tyrender.exe)
static DWORD Align(DWORD v, DWORD a) { return (v + a - 1) / a * a; }
static int PatchExe(const wchar_t* in, const wchar_t* out, wchar_t* err, int errLen) {
    HANDLE f = CreateFileW(in, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { swprintf(err, errLen, L"cannot open %ls", in); return 0; }
    DWORD size = GetFileSize(f, NULL), rd; BYTE* b = (BYTE*)malloc(size); ReadFile(f, b, size, &rd, NULL); CloseHandle(f);
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)b; IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(b + dos->e_lfanew);
    if (dos->e_magic != 0x5A4D || nt->Signature != 0x4550 || nt->OptionalHeader.Magic != 0x10B) { wcscpy(err, L"not a 32-bit PE"); free(b); return 0; }
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt); int n = nt->FileHeader.NumberOfSections;
    for (int i = 0; i < n; i++) if (!memcmp(sec[i].Name, ".tyimp", 6)) { wcscpy(err, L"input is already patched"); free(b); return 0; }
    DWORD firstRaw = 0xFFFFFFFF; for (int i = 0; i < n; i++) if (sec[i].PointerToRawData && sec[i].PointerToRawData < firstRaw) firstRaw = sec[i].PointerToRawData;
    DWORD hdrEnd = (DWORD)((BYTE*)&sec[n] - b);
    if (hdrEnd + 40 > nt->OptionalHeader.SizeOfHeaders || hdrEnd + 40 > firstRaw) { wcscpy(err, L"no room for a new section header"); free(b); return 0; }
    DWORD impRva = nt->OptionalHeader.DataDirectory[1].VirtualAddress, impOff = 0;
    for (int i = 0; i < n; i++) { DWORD vs = max(sec[i].Misc.VirtualSize, sec[i].SizeOfRawData); if (impRva >= sec[i].VirtualAddress && impRva < sec[i].VirtualAddress + vs) impOff = impRva - sec[i].VirtualAddress + sec[i].PointerToRawData; }
    if (!impOff) { wcscpy(err, L"import directory not found"); free(b); return 0; }
    int nd = 0; while (((IMAGE_IMPORT_DESCRIPTOR*)(b + impOff))[nd].Name) nd++;
    IMAGE_SECTION_HEADER* last = &sec[n - 1];
    DWORD sa = nt->OptionalHeader.SectionAlignment, fa = nt->OptionalHeader.FileAlignment;
    DWORD newVa = Align(last->VirtualAddress + max(last->Misc.VirtualSize, last->SizeOfRawData), sa), newRaw = Align(size, fa);
    const char* fn = "tyrender_init"; const char* dll = "tyrender.dll";
    DWORD descLen = (nd + 2) * 20, oft = descLen, ft = oft + 8, hn = ft + 8, dllOff = hn + 2 + (DWORD)strlen(fn) + 2, vsize = dllOff + (DWORD)strlen(dll) + 1;
    DWORD rawSize = Align(vsize, fa);
    BYTE* o = (BYTE*)calloc(1, newRaw + rawSize); memcpy(o, b, size);
    BYTE* s = o + newRaw;
    memcpy(s, b + impOff, nd * 20);
    IMAGE_IMPORT_DESCRIPTOR* d = (IMAGE_IMPORT_DESCRIPTOR*)(s + nd * 20);
    d->OriginalFirstThunk = newVa + oft; d->Name = newVa + dllOff; d->FirstThunk = newVa + ft;
    *(DWORD*)(s + oft) = newVa + hn; *(DWORD*)(s + ft) = newVa + hn;
    memcpy(s + hn + 2, fn, strlen(fn)); memcpy(s + dllOff, dll, strlen(dll));
    IMAGE_NT_HEADERS32* ont = (IMAGE_NT_HEADERS32*)(o + dos->e_lfanew); IMAGE_SECTION_HEADER* os = IMAGE_FIRST_SECTION(ont) + n;
    memset(os, 0, 40); memcpy(os->Name, ".tyimp", 6);
    os->Misc.VirtualSize = vsize; os->VirtualAddress = newVa; os->SizeOfRawData = rawSize; os->PointerToRawData = newRaw; os->Characteristics = 0xC0000040;
    ont->FileHeader.NumberOfSections = (WORD)(n + 1);
    ont->FileHeader.Characteristics |= 0x0020;   // IMAGE_FILE_LARGE_ADDRESS_AWARE: 4 GB address space for the copy (HD textures + driver exhaust 2 GB: "CMKHeap - Out of memory")
    ont->OptionalHeader.SizeOfImage = Align(newVa + vsize, sa); ont->OptionalHeader.CheckSum = 0;
    ont->OptionalHeader.DataDirectory[1].VirtualAddress = newVa; ont->OptionalHeader.DataDirectory[1].Size = descLen;
    ont->OptionalHeader.DataDirectory[11].VirtualAddress = 0; ont->OptionalHeader.DataDirectory[11].Size = 0;
    HANDLE w = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD wr = 0;
    int ok = w != INVALID_HANDLE_VALUE && WriteFile(w, o, newRaw + rawSize, &wr, NULL);
    if (w != INVALID_HANDLE_VALUE) CloseHandle(w);
    free(b); free(o);
    if (!ok) swprintf(err, errLen, L"cannot write %ls", out);
    return ok;
}

// ------------------------------------------------------------ starting the game
#include "build_id.h"   // TY_BUILD_ID, written by build.ps1
#define TY_VERSION L"v0.3.5-rc"
#define TY_WINDOW_TITLE L"TY HD Renderer " TY_VERSION L"  build " TY_BUILD_ID
static void Fail(const wchar_t* what) { wchar_t m[600]; swprintf(m, 600, L"%ls (error %lu)", what, GetLastError()); MessageBoxW(NULL, m, L"TYLauncher", MB_ICONERROR); }
// ------------------------------------------------------------ embedded payload (tyrender.dll + default ini)
// build.ps1 links the matching tyrender.dll (resource 1) and default_tyrender.ini (resource 2) into this exe, so TYLauncher.exe alone
// is the release: it installs the DLL next to itself and creates tyrender.ini when there is none. Builds without a payload skip this.
static const BYTE* PayloadRes(int id, DWORD* size) {
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA); if (!r) return NULL;
    HGLOBAL g = LoadResource(NULL, r); if (!g) return NULL;
    *size = SizeofResource(NULL, r); return (const BYTE*)LockResource(g);
}
// Writes data to path when the file is missing (or, unless onlyIfMissing, when its bytes differ). Returns 1 when the file is current afterwards.
static int PayloadWrite(const wchar_t* path, const BYTE* data, DWORD size, int onlyIfMissing) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        if (onlyIfMissing) { CloseHandle(f); return 1; }
        DWORD have = GetFileSize(f, NULL), rd = 0; int same = 0;
        if (have == size) { BYTE* b = (BYTE*)malloc(size); if (b && ReadFile(f, b, size, &rd, NULL) && rd == size) same = !memcmp(b, data, size); free(b); }
        CloseHandle(f); if (same) return 1;
    }
    HANDLE w = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (w == INVALID_HANDLE_VALUE) return 0;
    DWORD wr = 0; int ok = WriteFile(w, data, size, &wr, NULL) && wr == size; CloseHandle(w); return ok;
}
static void EnsurePayload(void) {
    DWORD n = 0; const BYTE* d; wchar_t p[MAX_PATH]; swprintf(p, MAX_PATH, L"%lstyrender.dll", g_dir);
    if ((d = PayloadRes(1, &n)) && !PayloadWrite(p, d, n, 0))
        MessageBoxW(NULL, L"Could not update tyrender.dll next to the launcher (is the game running?). The existing DLL stays in use.", L"TYLauncher", MB_ICONWARNING);
    if ((d = PayloadRes(2, &n))) PayloadWrite(g_ini, d, n, 1);   // never overwrite the user's settings
}
static int StartGame(int inject) {
    wchar_t exe[MAX_PATH], dll[MAX_PATH], cl[4400];
    swprintf(dll, MAX_PATH, L"%lstyrender.dll", g_dir);
    swprintf(exe, MAX_PATH, L"%lsTY_tyrender.exe", g_dir);
    int patched = inject && GetFileAttributesW(exe) != INVALID_FILE_ATTRIBUTES;
    if (!patched) swprintf(exe, MAX_PATH, L"%lsTY.exe", g_dir);
    swprintf(cl, 4400, L"\"%ls\"%ls", exe, g_args);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi;
    int doInject = inject && !patched;
    if (!CreateProcessW(exe, cl, NULL, NULL, FALSE, doInject ? CREATE_SUSPENDED : 0, NULL, g_dir, &si, &pi)) { Fail(L"Could not start the game"); return 1; }
    if (doInject) {
        if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) { TerminateProcess(pi.hProcess, 1); Fail(L"tyrender.dll not found next to TYLauncher.exe"); return 1; }
        SIZE_T n = (wcslen(dll) + 1) * sizeof(wchar_t);
        void* mem = VirtualAllocEx(pi.hProcess, NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!mem || !WriteProcessMemory(pi.hProcess, mem, dll, n, NULL)) { TerminateProcess(pi.hProcess, 1); Fail(L"Could not inject (use the Patch button instead)"); return 1; }
        HANDLE th = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"), mem, 0, NULL);
        if (!th) { TerminateProcess(pi.hProcess, 1); Fail(L"CreateRemoteThread failed (use the Patch button instead)"); return 1; }
        WaitForSingleObject(th, 30000); DWORD mod = 0; GetExitCodeThread(th, &mod); CloseHandle(th); VirtualFreeEx(pi.hProcess, mem, 0, MEM_RELEASE);
        if (!mod) { TerminateProcess(pi.hProcess, 1); Fail(L"tyrender.dll failed to load"); return 1; }
        ResumeThread(pi.hThread);
    }
    DWORD code = 0;
    if (g_wait) { WaitForSingleObject(pi.hProcess, INFINITE); GetExitCodeProcess(pi.hProcess, &code); }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return (int)code;
}


#include "launcher_ui.inc"
