// Import-table patch shared by TYLauncher.exe and the installer (src/installer/setup.c):
// makes TY_tyrender.exe, a copy of TY.exe that imports tyrender.dll and is Large Address Aware. TY.exe is never modified.
// Same algorithm as patch_imports.ps1.
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

