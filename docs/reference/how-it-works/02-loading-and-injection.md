# Loading and injection

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester) for the file list.

## Purpose

This page explains how `tyrender.dll` gets into the game process, and why the mod uses a patched copy of `TY.exe` instead of a replacement DLL. It also lists every file the mod writes into the game folder, and how the launcher and installer put them there. The short version: `TY.exe` is never changed. A copy named `TY_tyrender.exe` imports `tyrender.dll`, so Windows loads the DLL when the copy starts.

## How it works

### Why a patched copy, not a proxy DLL

1. The project docs state the choice: "our DLL is injected via a patched exe copy, not a proxy DLL" (`docs/START_HERE.md:91`, section 7). The full reasoning is in a design note the code cites (`src/tyrender.cpp:3`, `RESEARCH\sessions\...\D2_dll_loading_and_hooking.md`). That note is not in the repo (plan Appendix B), so the reasons are NEEDS INVESTIGATION (see NI-C2-2 below).
2. The DLL has one export, `tyrender_init`, which does nothing (`src/tyrender.cpp:673-674`). A proxy DLL would need to forward the game's imports to the real library, and no such forwarding code exists in the repo (inferred from the export list).
3. The export exists so that the copy has a named import from `tyrender.dll`. Without a named import, Windows would not load the DLL at start (inferred, standard PE loading). The comment says so: "so a patched TY.exe can import tyrender.dll" (`src/tyrender.cpp:673`).

### The import-table patch

The patch is the same algorithm in two places: `src/patch_exe.inc` (used by the launcher and the installer) and `src/patch_imports.ps1` (a PowerShell version for manual use). Steps, with the C version:

```
TY.exe  (opened read-only, GENERIC_READ only: patch_exe.inc:7)
   |  read whole file into memory                       (patch_exe.inc:9)
   |  checks: PE32, not already patched, room for one more
   |  section header                                    (patch_exe.inc:11, 13, 16)
   v
output bytes = copy of TY.exe
   |  + new section ".tyimp" appended after the last section      (:23, :35-36)
   |       [ original import descriptors ][ new descriptor ][ zero descriptor ]
   |       [ OriginalFirstThunk: "tyrender_init" ]
   |       [ FirstThunk (IAT slot): "tyrender_init" ]
   |       [ hint/name "tyrender_init" ][ "tyrender.dll" ]
   |  + header edits:
   |       NumberOfSections + 1                          (:37)
   |       Large Address Aware flag 0x0020               (:38)
   |       SizeOfImage recomputed, CheckSum = 0          (:39)
   |       DataDirectory[1] (imports) -> new table       (:40)
   |       DataDirectory[11] (bound imports) zeroed      (:41)
   v
written to TY_tyrender.exe, CREATE_ALWAYS                (:42-43)
```

Facts from the code:

- The original import descriptors are copied unchanged. The new section only adds one descriptor and its tables (`src/patch_exe.inc:29-33`).
- The new section has the flags "initialized data, read, write" (`src/patch_exe.inc:36`, value `0xC0000040`). The PowerShell version uses the same value (`src/patch_imports.ps1:69`).
- Bound imports are cleared, so the loader does not use stale addresses (`src/patch_exe.inc:41`; `src/patch_imports.ps1:78`).
- The copy is 4 GB addressable instead of 2 GB (Large Address Aware). The comment gives the reason: the HD textures and driver exhaust 2 GB, which shows as `CMKHeap - Out of memory` (`src/patch_exe.inc:38`; `src/patch_imports.ps1:72-73`). README "Memory" gives the same reason.
- The patch refuses a file that is already patched (a section named `.tyimp` exists; `src/patch_exe.inc:13`, `src/patch_imports.ps1:26`) and a file without room for one more section header (`src/patch_exe.inc:16`, `src/patch_imports.ps1:25`).
- Any file overlay after the last section is kept, and the new section is placed after it (`src/patch_imports.ps1:45`, warning only).

### The launcher's start path

1. `wWinMain` sets the folder to the folder of `TYLauncher.exe` (`src/launcher_ui.inc:543`). The launcher must sit in the game folder: `TY.exe`, `TY_tyrender.exe` and `tyrender.dll` are all looked up there.
2. `EnsurePayload` writes `tyrender.dll` from resource 1 if the file is missing or different (`src/TYLauncher.c:47-48`). It writes `tyrender.ini` from resource 2 only if the file is missing (`src/TYLauncher.c:49`). If the DLL is locked because the game runs, a warning box appears and the old DLL stays (`src/TYLauncher.c:47-48`).
3. Command-line arguments (`src/launcher_ui.inc:546-554`):
   - `--wait`: wait for the game to exit and return its exit code (`:548`, `src/TYLauncher.c:73`).
   - `--noinject`: start the stock `TY.exe`, no DLL (`:549`).
   - `--play`: skip the window and start with the saved settings (`:550`).
   - Any argument that ends in `TY.exe` is dropped. This is how Steam's `%command%` is ignored (`:552`).
   - Everything else is passed to the game.
4. Play (`src/launcher_ui.inc:497`):
   - `SaveAll` writes the settings to `tyrender.ini` (`:246`, writers at `:57-64`), and writes `tyrender\reload` if the game window is found (`:246`, `:166-169`).
   - `EnsurePatched` creates `TY_tyrender.exe` if it is missing (`:255-260`). It does nothing if the file exists (`:257`), so a copy that is in use is never rewritten. It does not check whether the copy matches the current `TY.exe` (see Limits).
   - If the copy is missing and cannot be written (for example, the game folder is read-only), `PatchExe` fails and the launcher shows "could not create TY_tyrender.exe (will try DLL injection)" (`:258-259`, `src/patch_exe.inc:42-43`).
   - `StartGame(1)` runs (`src/TYLauncher.c:51-76`).
5. Stock (`src/launcher_ui.inc:498`): `SaveAll` then `StartGame(0)`, which runs the untouched `TY.exe` (`src/TYLauncher.c:55-56`).
6. Injection fallback (`src/TYLauncher.c:55-71`). It runs only when `inject` is set and `TY_tyrender.exe` does not exist at `StartGame` time. That happens only if `PatchExe` failed, because `EnsurePatched` runs first. The steps:
   - start `TY.exe` suspended (`:59-60`);
   - allocate memory in the game and write the DLL path (`:64-65`);
   - start a remote thread that calls `LoadLibraryW` (`:66-67`);
   - wait up to 30 seconds and read the module handle (`:68-69`);
   - resume the game (`:70`).
   The launcher header calls this the fallback and notes that "some security tools/sandboxes block this" (`src/TYLauncher.c:3-4`).

### The Steam launch option

`Steam setup...` writes a launch option of the form `"<game folder>TYLauncher.exe" --play %command%` (`src/launcher_ui.inc:505`), and shows the user where to paste it (`:508-510`). The option does not include `--wait`. So the process Steam starts is the launcher, and the launcher exits right after it starts the copy (inferred from the code path; `src/TYLauncher.c:73` waits only with `--wait`). Whether Steam keeps tracking the game after that is NEEDS INVESTIGATION (N-9).

### What the installer does

`TY HD Renderer Setup.exe` (`src/installer/setup.c`) runs the same steps in this order (`DoInstall`, `:77-87`):

1. Create `<game>\tyrender\` and the manifest `install.ini` (`setup.c:78-79`, `common.inc:53-55`, `:89`).
2. Write `TYLauncher.exe` (`setup.c:80`, resource 1), `tyrender.dll` (`:81`, resource 3), and the uninstaller (`:82`, resource 2). Resource ids: `setup.c:10-13`.
3. Create `TY_tyrender.exe` with `PatchExe` (`setup.c:29-41`, `:83`).
4. Write `tyrender.ini` if none exists, or add only the missing keys if one exists (`setup.c:43-60`, `:84`).
5. Add the Apps and Features entry (`setup.c:85`).

Before any file is replaced, the original is copied into `<game>\tyrender\backup\` and recorded in the manifest (`setup.c:15-23`, `:32-36`, `:53-56`; `common.inc:186-190`). The game folder is found in this order (`FindSteamGame`, `src/installer/common.inc:143-170`): the Steam install path from the registry value `SteamPath` under `HKCU\Software\Valve\Steam`; the default `steamapps\common\TY the Tasmanian Tiger` folder; then every `"path"` entry of `steamapps\libraryfolders.vdf`. A folder counts only if it holds `TY.exe`. If nothing is found, the user picks the folder (`src/installer/setup.c:96-101`). Setup refuses to run while a process named `TY.exe` or `TY_tyrender.exe` is running (`src/installer/setup.c:106`, `src/installer/common.inc:95-101`). The launcher uses a different test, the game window title (`src/launcher_ui.inc:165`).

### Uninstall

`TY HD Renderer Uninstall.exe` (`src/installer/uninstall.c`) uses the manifest in `tyrender\install.ini`:

- every file in the manifest is restored from `tyrender\backup\` if it was replaced, or deleted if the install created it (`uninstall.c:9-20`, `:23-36`);
- the user is asked whether to keep `tyrender.ini`. Yes keeps the current file. No restores the file from before the install, or deletes it when there was none (`uninstall.c:79-80`, `:30-33`);
- `tyrender.log` and `tyrender\reload` are deleted if present (`uninstall.c:38-43`);
- the manifest and the Apps and Features entry are removed (`uninstall.c:82-85`), then the uninstaller removes itself and the install folder pieces that are empty, with `rmdir` (no `/s`) (`uninstall.c:47-60`).

Files the mod creates at run time and that are not in the manifest stay behind: `tyrender\dump\`, `tyrender\override\` (if the user added files), `tyrender\logs\`, `Screenshots\`, and any `cam.txt` the user created (inferred from `RemoveRuntimeFiles` and the non-recursive `rmdir` calls). The DLL deletes `cap.req` itself after use (`src/tyrender.cpp:625-626`).

### Going back to stock

- Uninstall (above), or
- the launcher's **Play original** button, which starts `TY.exe` directly (`src/launcher_ui.inc:498`, `src/TYLauncher.c:55-56`). Nothing is removed, so the mod stays installed and can be used again, or
- by hand: delete the files listed in README "Uninstall" (`README.md:207`). That list names `TY_tyrender.exe`.

See [installer-and-uninstaller](../reference/installer-and-uninstaller.md).

## Files the mod writes in the game folder

| Path (relative to the game folder) | Written by | When | file:line |
|---|---|---|---|
| `TYLauncher.exe` | installer, or the user by hand | install | `src/installer/setup.c:80` |
| `tyrender.dll` | launcher (from resource 1), installer | each launcher start if different; install | `src/TYLauncher.c:47-48`, `src/installer/setup.c:81` |
| `TY HD Renderer Uninstall.exe` | installer | install | `src/installer/setup.c:82` |
| `TY_tyrender.exe` | launcher, installer, `--play` path | first Play or install | `src/launcher_ui.inc:255-260`, `:555-556`, `src/installer/setup.c:31-41` |
| `tyrender.ini` | launcher (resource 2, only if missing), installer (adds missing keys), launcher Save (every change) | install, Play, slider or button change | `src/TYLauncher.c:49`, `src/installer/setup.c:43-60`, `src/launcher_ui.inc:246` |
| `tyrender\install.ini`, `tyrender\install.log` | installer | install | `src/installer/common.inc:55`, `:89`, `src/installer/setup.c:67-75` |
| `tyrender\backup\<file>` | installer | before a file is replaced | `src/installer/common.inc:186-190` |
| `tyrender\reload` (empty) | launcher | a setting changed while the game runs | `src/launcher_ui.inc:166-169` (deleted by the DLL, `src/tyrender.cpp:597-598`) |
| `tyrender.log` | DLL | each start that loads the DLL (truncated) | `src/tyrender.cpp:708` |
| `tyrender\dump\*.glsl` | DLL | shader dumps, when `DumpShaders` is on | `src/tyrender.cpp:370-372`, `:710` |
| `tyrender\override\` | DLL (folder only) | each start | `src/tyrender.cpp:711` |
| `tyrender\logs\TYHD_report_*.txt` | DLL | first frame of each start (session report) | `src/sesslog.inl:258-262`, `:329-330` |
| `Screenshots\*.png` | DLL | on an A/B screenshot | `src/abshot.inl:86-92` |

Files the mod reads, and that the user may create: `tyrender\cap.req`, `tyrender\post_off`, `tyrender\ao_off`, the other flag files, `tyrender\override\<hash>.glsl`, `tyrender\cam.txt`, `tyrender\motion.req`. Full list: [hotkeys-and-files](../reference/hotkeys-and-files.md).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Read-only open of TY.exe | `src/patch_exe.inc:7` | Only `GENERIC_READ` |
| Output write of the copy | `src/patch_exe.inc:42-43` | `CREATE_ALWAYS` on `TY_tyrender.exe` |
| New section and flags | `src/patch_exe.inc:35-36` | `0xC0000040` |
| LAA flag | `src/patch_exe.inc:38` | `0x0020` in the header |
| Import directory and bound imports | `src/patch_exe.inc:40-41` | DataDirectory[1] and [11] |
| PowerShell version | `src/patch_imports.ps1:5-6` | Hard-coded paths (see Limits) |
| Resource ids (launcher) | `src/build.ps1:17` | 1 = DLL, 2 = default ini |
| Resource ids (setup) | `src/build.ps1:27`, `src/installer/setup.c:10-13` | 1 launcher, 2 uninstaller, 3 DLL, 4 ini |
| Payload read and write | `src/TYLauncher.c:28-44` | `PayloadRes`, `PayloadWrite` |
| Start and inject | `src/TYLauncher.c:51-76` | `StartGame` |
| Launcher argument parsing | `src/launcher_ui.inc:542-556` | `wWinMain` |
| Play, Stock, Steam button | `src/launcher_ui.inc:497`, `:498`, `:503-511` | |
| Launcher reload flag | `src/launcher_ui.inc:166-169` | Empty file |
| DLL reads reload flag | `src/tyrender.cpp:594-598` | Every 8 frames, then deleted |
| DLL log file | `src/tyrender.cpp:705-708` | Truncated on each load |
| Session report folder | `src/sesslog.inl:259-260` | `tyrender\logs` |
| Screenshot folder | `src/abshot.inl:86-92` | `Screenshots`, created on first use |
| Installer manifest | `src/installer/common.inc:89` | `tyrender\install.ini` |

## Settings

| Key | Default in code | Shipped in `default_tyrender.ini` | Live or restart | Effect |
|---|---|---|---|---|
| `[debug] CaptureFrame` | 0 (`src/tyrender.cpp:58`) | absent | restart | One frame logged call by call |
| `[debug] CaptureFrame2` | 0 (`src/tyrender.cpp:59`) | absent | restart | A second captured frame |
| env `TYRENDER_CAPTURE` | unset (`src/tyrender.cpp:63`) | not applicable | restart | Overrides `CaptureFrame` when set |
| `[debug] DumpShaders` | 1 (`src/tyrender.cpp:60`) | absent | restart | Shader sources written to `tyrender\dump` (files are not overwritten, `:371`) |
| `[debug] OverrideShaders` | 1 (`src/tyrender.cpp:61`) | absent | restart | Replacement shaders read from `tyrender\override` |
| `[clash] Check` | 1 (`src/tyrender.cpp:639`) | absent | restart | Warns in the log when the game's own SSAO or MSAA is on |
| `[report] Enable` | 1 (`src/sesslog.inl:372`) | 1 (`src/default_tyrender.ini:93`) | restart | Session report on or off |
| `[report] CaptureSeconds` | 300 (`src/sesslog.inl:373`) | 300 (`:94`) | restart | Frame-time rows at the start |
| `[report] MaxBytes` | 1048576 (`src/sesslog.inl:374`) | 1048576 (`:95`) | restart | Size cap per report file |
| `[report] KeepFiles` | 10 (`src/sesslog.inl:375`) | 10 (`:96`) | restart | Number of report files kept |

The full tables are in [settings-system-debug](../reference/settings-system-debug.md). Keys here are the ones this page's code reads.

Note: the shipped ini has no `[debug]`, `[clash]` or `[tex]` section. The code defaults apply (`src/default_tyrender.ini`, checked by grep).

## Cost

Not measured.

- Start-up reads `tyrender.ini` about ten times (`src/tyrender.cpp:714-725`, one `GetPrivateProfile*` call per key) and loads the texture tag table (`src/tex.inl:65`, the list is in `src/tex_tags.inc`).
- The patch reads all of `TY.exe` into memory and writes a copy of about the same size (`src/patch_exe.inc:9`, `:42-43`). It runs at most once per copy: after `TY_tyrender.exe` exists, Play skips it (`src/launcher_ui.inc:257`).
- While `DumpShaders` is on, the DLL writes one dump file per distinct shader source the game compiles (`src/tyrender.cpp:369-372`). The count depends on the game's shader set (17 `.shader` files, see [game-and-engine-facts](../concepts/game-and-engine-facts.md)).

## Limits

- The copy is tied to the `TY.exe` it was made from. `EnsurePatched` and the Play path check only whether `TY_tyrender.exe` exists (`src/launcher_ui.inc:257`, `src/launcher_ui.inc:555-556`). After a game update, the old copy keeps being used (inferred from the code; not tested). The fix is to delete `TY_tyrender.exe`. Only the installer and the launcher create it.
- The patch needs a PE32 file with the import directory in a section and room for one more section header (`src/patch_exe.inc:11`, `:16`, `:18-19`). Other builds are not tested (see NI-7).
- The DLL build is not checked against the `TY.exe` build (no version check in `src/TYLauncher.c` or the installer, inferred from the code).
- The injection fallback depends on `LoadLibraryW` in the game and on a remote thread, which some security tools block (`src/TYLauncher.c:3-4`).
- The PowerShell patch has hard-coded paths in its parameter defaults (`src/patch_imports.ps1:5-6`), and the build scripts do too (`src/build.ps1:5`, `:34`). Those are the owner's workstation paths. Use `<game folder>` and `<tools folder>` instead. Tracked as NEEDS INVESTIGATION item N-13.
- The launcher's Stock button and `--noinject` run the stock game with no DLL, so no hook, no log and no reload (`src/launcher_ui.inc:498`, `:549`).

## Known issues

- Out-of-memory crash (`CMKHeap - Out of memory`) with the HD texture pack. Fixed in the copy by the Large Address Aware flag (`src/patch_exe.inc:38`). README "Memory" describes it.
- The settings clash warning (issue #13) is in `src/tyrender.cpp:632-659` and in the launcher status line (`src/launcher_ui.inc:102`).
- Steam with the launch option: untested interaction with Steam tracking (NI-9).
- Linux, Proton and Steam Deck: untested (README "Tested on"; NI-10).

## How to verify

1. Hash check: compute the SHA-256 of `TY.exe` before installing and after Play. The hashes must match, because the patch opens `TY.exe` read-only (`src/patch_exe.inc:7`). In PowerShell, `Get-FileHash -Algorithm SHA256 "<game folder>\TY.exe"`.
2. Large Address Aware: the copy's PE header has the flag `0x0020` at the characteristics field (file offset of the PE header plus 22). With the MSVC tools installed, `dumpbin /headers "<game folder>\TY_tyrender.exe"` prints "Application can handle large (>2GB) addresses". The `TY.exe` file should not.
3. Import entry: `dumpbin /imports "<game folder>\TY_tyrender.exe"` should show `tyrender.dll` with `tyrender_init` (inferred from `src/patch_exe.inc:24`).
4. Files in the game folder after a normal install: `TYLauncher.exe`, `tyrender.dll`, `TY HD Renderer Uninstall.exe`, `TY_tyrender.exe`, `tyrender.ini`, and the folder `tyrender\` with `install.ini`, `install.log`, `backup\`, `dump\`, `override\`. `tyrender.log` appears on the first start that loads the DLL. `tyrender\logs\` appears with the first report. `Screenshots\` appears with the first screenshot.
5. Stock check: after Play original, `tyrender.log` should keep the time it had before (the DLL is not loaded, see `src/launcher_ui.inc:498`).
6. Injection path: reachable only when `PatchExe` fails (`src/launcher_ui.inc:258-259`). Testing it needs a `TY.exe` copy that the patch refuses, in a test folder. Not done in this pass.

## NEEDS INVESTIGATION

- **N-9 (Steam):** Does Steam keep the overlay, achievements and playtime tracking when its launch option starts the launcher, which starts `TY_tyrender.exe`? Method: set the launch option in a test library, start the game from Steam, and watch the process list. No Steam API use was found: a grep for `steam_api`, `SteamAPI` and `steam_appid` in `src/` and `tools/` returns nothing. The only Steam code is the library lookup in the installer (`src/installer/common.inc:143-170`) and the launch option text (`src/launcher_ui.inc:505`).
- **N-10 (Linux, Proton, Steam Deck):** Does the import patch and the IAT hooks work under Proton? Method: a Proton run with `tyrender.log`. Web search `Proton OpenGL wglGetProcAddress IAT hook` (allowed for this item). Cite results.
- **N-6 (community tools, load order):** Does `TygerFramework`, a PC_External loader or the ty2k mod conflict with `tyrender.dll` or the copy? The README says "Works with the base game" and "Gets more from the ty2k mod" (`README.md:254-255`), with no test evidence in the repo. Method: install the other mod with the renderer, run once, and read `tyrender.log` for `[iat] NOT FOUND`.
- **N-7 (supported TY.exe builds):** Is a hash check present? Not in `src/TYLauncher.c` or the installer (searched). Method: test other builds in a test folder; see the README "Game version" line.
- **NI-C2-2 (new item, proposed for section 6):** Why a patched copy and not a proxy DLL (the design note cited at `src/tyrender.cpp:3` is not in the repo). Method: ask the owner to export `D2_dll_loading_and_hooking.md`, or write the reasons from the git history of `src/tyrender.cpp` if the owner agrees.
- **NI-C2-1 (new item, proposed for section 6):** Does Windows snap the copy's import table before `tyrender.dll`'s `DllMain` runs? `CheckOrig` (`src/tyrender.cpp:677-681`) exists to detect a failure here. Method: read the three `[iat] ... orig=` lines in `tyrender.log` after a run. Each `module=` should name a loaded system DLL (gdi32 or opengl32), not an empty value.

## Discrepancies

- `docs/LAUNCHER.md` (first pass, line 23) says `--noinject` starts the game "without injecting the DLL (the patched executable is used instead)". Code: `--noinject` starts the stock `TY.exe` (`src/launcher_ui.inc:549`, `src/TYLauncher.c:55-56`).
- `docs/LAUNCHER.md` (line 25) says the injection path "starts suspended". Code: only when `TY_tyrender.exe` is missing at start (`src/TYLauncher.c:55`, `:59`).
- `ARCHITECTURE.md`, section 1, says the launcher injects "into a running start". Code: the process is suspended (`src/TYLauncher.c:59-70`).
- `src/build.ps1:36` lists what to delete by hand. It names `tyrender.dll`, `TYLauncher.exe`, `tyrender\`, `tyrender.log` and `tyrender.ini`, and omits `TY_tyrender.exe`. `README.md:207` and `src/installer/uninstall.c:71` both name `TY_tyrender.exe`.
- `ARCHITECTURE.md`, section 5, lists `src/ui_part_a.inc` and `src/ui_part_c.inc` as launcher parts. No file includes them. `src/TYLauncher.c:79` includes only `launcher_ui.inc`.

## See also

- [01-architecture-overview.md](01-architecture-overview.md): the whole picture.
- [03-gl-hooking.md](03-gl-hooking.md): what the DLL does once it is loaded.
- [launcher](../reference/launcher.md): the launcher's controls and start paths.
- [installer-and-uninstaller](../reference/installer-and-uninstaller.md): setup and uninstall in detail.
- [hotkeys-and-files](../reference/hotkeys-and-files.md): every flag and trigger file.
- [how-it-works/08 settings pipeline](08-settings-pipeline-and-live-reload.md): where the ini values come from.
