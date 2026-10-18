# Architecture overview

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), then B and C.

## Purpose

This page is the map. It names every piece of the mod, says which process runs it, and shows the order in which things start. Read it before any other page in `how-it-works/`.

The mod never changes the game's executable. A patched copy of the game (`TY_tyrender.exe`) loads a DLL (`tyrender.dll`). The DLL patches the copy's OpenGL calls in memory and adds its own render passes at fixed points in the frame. A separate launcher program writes the settings file and starts the copy.

Details: loading and the exe patch are on [02-loading-and-injection.md](02-loading-and-injection.md). The hook layer is on [03-gl-hooking.md](03-gl-hooking.md). The frame order is on [04-frame-anatomy-and-pass-order.md](04-frame-anatomy-and-pass-order.md).

## How it works

### 1. The pieces

| Piece | Runs in | Built from | Role |
|---|---|---|---|
| `TYLauncher.exe` | its own process (settings UI) | `src/TYLauncher.c:1-6`, `src/launcher_ui.inc:542-566` | Writes `tyrender.ini`, starts the game, creates `TY_tyrender.exe` on first Play. Also carries `tyrender.dll` and the default ini as resources (`src/build.ps1:17`). |
| `TY HD Renderer Setup.exe` | its own process (installer) | `src/installer/setup.c:89-110` | Copies the launcher, DLL and uninstaller into the game folder, creates `TY_tyrender.exe`, writes a manifest. |
| `TY HD Renderer Uninstall.exe` | its own process | `src/installer/uninstall.c:63-91` | Restores backups and removes what the install added. |
| `TY_tyrender.exe` | the game process | `src/patch_exe.inc:6-43` | A copy of `TY.exe` whose import table names `tyrender.dll`, with the Large Address Aware flag set. |
| `TY.exe` | not run by the mod on Play | none | The stock game. Opened read-only by the patch step (`src/patch_exe.inc:7`). |
| `tyrender.dll` | loaded into the game process | `src/tyrender.cpp` and every `.inl` it includes (`src/build.ps1:13`) | The hook layer and all render passes. |
| `tyrender.ini` | read by the DLL, written by the launcher and installer | `src/default_tyrender.ini` | All settings. Read at start, and again after a reload request. |
| `tyrender\reload` | empty flag file | `src/launcher_ui.inc:166-169` | Written by the launcher after a settings change while the game runs. The DLL polls for it (`src/tyrender.cpp:594-598`). |
| `tools/`, `src/*.ps1` | not part of the game | `src/build.ps1`, `src/patch_imports.ps1` and others | Build, packaging, testing and bookkeeping scripts. See [tools-reference](../reference/tools-reference.md). |

### 2. Data flow

```
 player
   |
   v
 TYLauncher.exe (own process, game folder)
   |  writes tyrender.ini            (launcher_ui.inc:246)
   |  writes tyrender\reload         (launcher_ui.inc:166-169), only if game window found
   |  writes tyrender.dll            (TYLauncher.c:45-48), when out of date
   |  creates TY_tyrender.exe        (launcher_ui.inc:255-260), first Play only
   |  CreateProcess                  (TYLauncher.c:60)
   v
 TY_tyrender.exe (game process)
   |  import table names tyrender.dll!tyrender_init  (patch_exe.inc:24, 33)
   |  loader maps tyrender.dll (inferred: standard PE import loading)
   v
 tyrender.dll
   |  DllMain: log, dirs, configs, Install   (tyrender.cpp:701-734)
   |  IAT patches on OPENGL32 / GDI32 / KERNEL32  (tyrender.cpp:662-671, tex.inl:166-171)
   v
 hooked game calls ----> render passes (AO, GI, emit, atmos, post) ----> original GL
   |
   +--> SwapBuffers hook: reload check, A/B shot, overlay, session report, timers
   |
   +--> writes: tyrender.log, tyrender\dump, tyrender\logs, Screenshots\
```

The launcher and the DLL never talk directly. They share files in the game folder. The launcher writes `tyrender.ini` and the flag file. The DLL reads them and deletes the flag file (`src/tyrender.cpp:597-598`).

### 3. Lifecycle

1. **Launcher start.** `wWinMain` sets the folder to the launcher's own folder (`src/launcher_ui.inc:543`). It writes the bundled DLL if the file is missing or different, and writes `tyrender.ini` only if it is missing (`src/TYLauncher.c:45-50`).
2. **Play.** `EnsurePatched` creates `TY_tyrender.exe` if it is not there (`src/launcher_ui.inc:255-260`). `StartGame(1)` starts the copy (`src/TYLauncher.c:51-76`). Injection is used only when the copy does not exist at that point (`src/TYLauncher.c:55`).
3. **Process start.** The loader maps the copy and loads `tyrender.dll` through its import entry. The DLL's only export is `tyrender_init` (`src/tyrender.cpp:673-674`).
4. **DllMain, attach.** Order in `src/tyrender.cpp`:
   - game folder and log file (`:705-708`);
   - `tyrender`, `dump` and `override` folders (`:709-711`);
   - timer frequency (`:712`) and the crash handler (`:713`);
   - all `tyrender.ini` readers: `LoadConfig` and the `Load*Config` family (`:714-725`);
   - the "loaded" log line (`:726`);
   - `Install` (`:727`), then three sanity logs of the saved originals (`:728-730`, one `[iat] ... orig=` line each);
   - session report setup (`:731`) and "hooks installed" (`:732`).
5. **Install.** Patches 17 OpenGL imports and `SwapBuffers` through the IAT (`src/tyrender.cpp:664-669`), and four file functions in KERNEL32 (`src/tex.inl:167-170`). Also replaces `wglGetProcAddress` through the IAT (`:668`), so later calls to it return hooked pointers.
6. **First frame.** The first `SwapBuffers` opens the session report file (`src/sesslog.inl:329-330`, `:258-262`). It is not opened in DllMain.
7. **Each frame.** GL calls run through the hooks (see [03](03-gl-hooking.md)). The `SwapBuffers` hook runs once per frame (`src/tyrender.cpp:591-630`).
8. **Exit.** On `DLL_PROCESS_DETACH` the DLL writes a "clean exit" line to the session report (`src/tyrender.cpp:702`, `src/sesslog.inl:346-361`). On a fatal exception, `CrashVEH` logs the code, address and stack, then writes a crash line (`src/tyrender.cpp:685-699`).

### 4. Module map of the DLL

`src/tyrender.cpp` is the only translation unit. It includes the other modules as text (`src/tyrender.cpp:434` and `:540-552`). The include order matters because the modules share file-level statics.

| Included file | Line in tyrender.cpp | Role (short) | Own includes |
|---|---|---|---|
| `tex.inl` | 434 | Texture names, material classes, kernel32 file hooks, upload hook | `tex_tags.inc` (`src/tex.inl:11`) |
| `ao.inl` | 540 | GTAO and the GL entry-point table (`GLProc`) | none |
| `fx.inl` | 541 | TAA, FXAA, grade, jitter | none |
| `gi.inl` | 542 | Legacy GI | `gi2.inl` (`src/gi.inl:10`) |
| `atmos.inl` | 543 | Atmosphere, fog, sky estimate | none |
| `emit.inl` | 544 | Emitter light | none |
| `tess.inl` | 545 | Tessellation and relief | none |
| `pbr.inl` | 546 | PBR rewrite | none |
| `light.inl` | 547 | Light model rewrite | none |
| `debugcam.inl` | 548 | Debug camera, motion test | none |
| `sesslog.inl` | 549 | Session report | `build_id.h` (`src/sesslog.inl:13`) |
| `gpuprof.inl` | 550 | GPU timers | `stateguard.inl` (`src/gpuprof.inl:4`) |
| `abshot.inl` | 551 | F11 A/B screenshots | `build_id.h` (`src/abshot.inl:7`) |
| `overlay.inl` | 552 | In-game settings overlay | `build_id.h` (`src/overlay.inl:8`) |

The build is one 32-bit shared library (`src/build.ps1:13`, `-shared -static`, i686 toolchain). The launcher is a second build (`src/build.ps1:20`).

### 5. Where state lives

- File-level `static` variables in `tyrender.cpp` and the included modules. There are no classes. Examples: frame counter `g_frame` (`src/tyrender.cpp:39`), the current program and FBO (`:67`), the scene depth texture (`:79`).
- Per-program maps keyed by GL program id: shader source ids (`src/tyrender.cpp:84`, `g_skinProgs`), named uniform locations (`:80`), per-program fog values (`:91`).
- Per-texture arrays keyed by GL texture id: material flags and classes in `tex.inl`.
- Nothing is saved between runs except the files in the game folder.

### 6. Threads

The mod creates no threads. A search of `src/` finds no `CreateThread`, `_beginthread` or `std::thread` (inferred from grep, see the NEEDS INVESTIGATION section). Every hook runs on whichever thread calls the GL or file function. One mutex exists, in `src/tex.inl:24`, and it guards the file-tracking state used by the KERNEL32 hooks (`src/tex.inl:145`, `:149`, `:178`). That suggests the authors expected file reads from more than one thread. Nothing in the repo shows that happening.

## Code pointers

| What | file:line | Note |
|---|---|---|
| Launcher entry | `src/launcher_ui.inc:542-566` | `wWinMain`, argument handling, Play and Stock paths |
| Launcher start of the game | `src/TYLauncher.c:51-76` | Starts the copy, or injects into a suspended `TY.exe` |
| Payload writer | `src/TYLauncher.c:45-50` | DLL and default ini from resources |
| Exe patch | `src/patch_exe.inc:6-43` | Same algorithm as `src/patch_imports.ps1` |
| DLL entry | `src/tyrender.cpp:701-734` | `DllMain` |
| IAT install | `src/tyrender.cpp:662-671` | Calls `PatchIAT` for each hooked import |
| Frame hook | `src/tyrender.cpp:591-630` | `h_SwapBuffers` |
| Crash handler | `src/tyrender.cpp:685-699` | Vectored exception handler |
| Session report open | `src/sesslog.inl:258-262`, `:329-330` | Lazy, on first frame |
| Installer file placement | `src/installer/setup.c:77-87` | `DoInstall` |

## Settings

| Key | Default in code | Shipped in `default_tyrender.ini` | Live or restart | Effect |
|---|---|---|---|---|
| `[debug] CaptureFrame` | 0 (`src/tyrender.cpp:58`) | absent | restart | Logs every call in one frame |
| `[debug] DumpShaders` | 1 (`src/tyrender.cpp:60`) | absent | restart | Writes shader sources to `tyrender\dump` |
| `[debug] OverrideShaders` | 1 (`src/tyrender.cpp:61`) | absent | restart | Reads replacement shaders from `tyrender\override` |
| `[report] Enable` | 1 (`src/sesslog.inl:372`) | 1 (`src/default_tyrender.ini:93`) | restart | Writes the session report file |
| `[clash] Check` | 1 (`src/tyrender.cpp:639`) | absent | restart | Logs a warning if the game's own SSAO or MSAA is on |

The full tables are in the settings reference pages (see [settings-system-debug](../reference/settings-system-debug.md)). Keys in this table are listed only because this page's mechanism reads them.

## Cost

Not measured. Start-up work is the ini reads, the tag table load and one pass over the game's import table. The per-frame work is in the pass pages. This page adds one `GetFileAttributes` call for each of the flag files every 8 frames (`src/tyrender.cpp:594-598`, `:624-626`).

## Limits

- One process, one GL context. `h_wglMakeCurrent` only logs the first context it sees (`src/tyrender.cpp:489-500`). Nothing in the code handles a second context (inferred from the code).
- The launcher cannot see frame data. It knows only what the DLL writes to files (inferred from the file-based design, `src/launcher_ui.inc:166-169`).
- 32-bit only. The DLL and the launcher are built for i686 (`src/build.ps1:13`, `:20`).
- The DLL keeps its own hook state in statics. A crash in the DLL takes the game with it. The crash handler logs and continues the search, it does not recover (`src/tyrender.cpp:698`).

## Known issues

- Several first-pass architecture claims do not match the code (session report timing, injection timing, a line count, the UI file list). See Discrepancies below.
- The launcher source has two UI files that are not built. `src/ui_part_c.inc` has a second `wWinMain` (`src/ui_part_c.inc:230`) but no file includes it. See [launcher](../reference/launcher.md).
- The header of `src/tyrender.cpp` (lines 1-6) describes the file as "Stage 1": a GL logger and shader dump. The file now does much more (inferred from comparing the header with the module list above).

## How to verify

1. Start the game through the launcher (Play). Open `<game folder>\tyrender.log`. The first lines should be the DllMain log lines, starting with `tyrender stage1 loaded. game dir ...` (`src/tyrender.cpp:726`). Then one `[iat] <name> orig=... module=...` line each for `SwapBuffers`, `wglGetProcAddress` and `glDrawElements` (`src/tyrender.cpp:728-730`), and `hooks installed` (`src/tyrender.cpp:732`). The file is truncated on each start (`src/tyrender.cpp:708`).
2. After the first frame, a file named `TYHD_report_<time>_<build>.txt` appears in `<game folder>\tyrender\logs\` (`src/sesslog.inl:259`).
3. Press Play original in the launcher. `tyrender.log` should not be touched, because the DLL is not loaded in that path (`src/launcher_ui.inc:498`, `src/TYLauncher.c:55-56`).

## NEEDS INVESTIGATION

- **NI-8 (threading, see section 6 of the plan):** Do any hooked GL or file calls come from a thread other than the render thread? Method: add a thread-id log line to a test build of the DLL in a test folder, or set a breakpoint on `h_glDrawElements` in a debugger and read the thread id. The repo contains no RE notes on this.

## Discrepancies

- `ARCHITECTURE.md`, section 2, step 2 says DllMain "opens the session report (`SessOpen`)". Code: DllMain calls `SessInit` (`src/tyrender.cpp:731`). `SessOpen` runs on the first `SwapBuffers` (`src/sesslog.inl:329-330`).
- `ARCHITECTURE.md`, section 5 gives `src/tyrender.cpp` as 735 lines. The file has 734 lines (`wc -l`).
- `ARCHITECTURE.md`, section 1 says the launcher injects "into a running start". Code: the injected process is started suspended, before it runs (`src/TYLauncher.c:61-70`).
- `ARCHITECTURE.md`, section 5 lists `src/ui_part_a.inc` and `src/ui_part_c.inc` as launcher UI parts. Code: `src/TYLauncher.c` includes only `launcher_ui.inc` (`src/TYLauncher.c:79`). The two `ui_part` files are not compiled.
- `src/tyrender.cpp:3` cites `RESEARCH\sessions\...\D2_dll_loading_and_hooking.md`. That file is not in the repo (plan Appendix B). Cite the comment only.

## See also

- [02-loading-and-injection.md](02-loading-and-injection.md): how the copy and DLL are created and started.
- [03-gl-hooking.md](03-gl-hooking.md): how hooks are placed and what they do.
- [04-frame-anatomy-and-pass-order.md](04-frame-anatomy-and-pass-order.md): the frame order.
- [hooks-reference.md](../reference/hooks-reference.md): every hooked function.
- [source-map](../reference/source-map.md): every file in `src/`.
- [../concepts/game-and-engine-facts.md](../concepts/game-and-engine-facts.md): what is known about the game.
