# Settings: system, debug, reports, hotkeys and checks

> Status: documented at commit 4db1d7d (`main`, Merge PR #122); last verified 2026-10-18. Audience: B (tuning and bug reports), C (review).

## Purpose

This page lists `[debug]`, `[report]`, `[shots]`, `[overlay]` and `[clash]`, the environment variables that change the DLL, the bit values of `ForceFallback` and the debug-view values (questions Q133 and Q134). `[debug]` keys are test switches. None of them should be needed for normal play. The `[report]` keys set the session report that testers send with bug reports. `[shots]` and `[overlay]` hold the two hotkeys whose behaviour is on the [hotkeys page](../reference/hotkeys-and-files.md) and the [overlay page](../how-it-works/09-overlay-input-and-hotkeys.md).

## How to read the tables

- **Key (type, range).** The section and key as the loader reads them. Keys are case-sensitive.
- **Code default (file:line).** The second argument of the `GetPrivateProfile*` call, which is used when the key is missing from `tyrender.ini`. The file:line is the first read in the code that is not a log line.
- **Shipped.** The value in `src/default_tyrender.ini` (`ini:N` is that file's line). "not in file" means the code default is used.
- **Launcher and presets.** What `src/launcher_ui.inc` writes or reads for the key. Preset values are in the order Low / Medium / High / Ultra.
- **Live or restart.** Definitions are in [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md). "Live" means the reload path re-reads the key and the value is used per frame or per draw. "(inferred)" means the use site was not traced line by line.
- **Effect.** One line. "(inferred from name)" means the text comes from the key name only. Mechanisms are on the shader pages linked under See also.

Every key in the code appears in exactly one table on the four settings pages (227 keys in all, checked against a grep of `GetPrivateProfile*` and the `F`/`LF`/`PF` macros in `src/*.inl` and `src/tyrender.cpp`).

## [debug]

| Key (type, range) | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `CaptureFrame` (int frame number) | `0` (tyrender.cpp:58) | not in file | Restart (startup only, tyrender.cpp:714) | Frame number to capture into `tyrender.log`; `0` = off. The env var `TYRENDER_CAPTURE` overrides it (tyrender.cpp:63). |
| `CaptureFrame2` (int frame number) | `0` (tyrender.cpp:59) | not in file | Restart | A second capture frame (tyrender.cpp:622). |
| `DumpShaders` (int 0/1) | `1` (tyrender.cpp:60) | not in file | Restart | 1 = write each compiled shader source to `tyrender\dump` once per hash (tyrender.cpp:369-372). **On by default.** |
| `OverrideShaders` (int 0/1) | `1` (tyrender.cpp:61) | not in file | Restart | 1 = a file `tyrender\override\<hash>.glsl` replaces the source at compile time (tyrender.cpp:375). |
| `ForceFallback` (int bit mask) | `0` (ao.inl:491) | not in file | Restart (inferred: read at compile or init) | Test switch that makes one effect fail on purpose. Bits in the next table. |
| `ShadowView` (int 0/1) | `0` (light.inl:24) | not in file | Live in code (reload re-reads it); comment says restart | 1 = shadow term alone, white lit, black shadow (light.inl:24, 117). |
| `StateGuard` (int 0/1) | `0` (stateguard.inl:32) | not in file | Live (re-read every 300 frames, stateguard.inl:32) | 1 = GL state leak detector; logs `[state]` lines. Opt-in (stateguard.inl:1). |
| `GpuProf` | **not read** (gpuprof.inl:3 comment only; `g_gpEnable` is fixed at 1, gpuprof.inl:5) | - | - | The comment says `[debug] GpuProf=0` turns GPU timers off. No code reads it. See Discrepancies. |

## ForceFallback bits (Q133)

| Bit | Forces | Where | What the frame does |
|---|---|---|---|
| 1 | Froxel fog failure | `src/atmos.inl:257` | Falls back to the ray-march path and marks the froxel as failed. |
| 2 | Compile failure of GI programs (names starting `gi_`) | `src/ao.inl:441` | GI programs fail to build; GI is off. |
| 4 | Compile failure of AO v2 programs (`pre0`, `preN`, `gtao2`) | `src/ao.inl:440` | AO v2 fails; with `Legacy=0` AO is turned off (`src/ao.inl:546`). |
| 8 | Tessellation stage compile failure | `src/tess.inl:382` | Programs relink without tessellation. |
| 16 | Invalid light shader rewrite | `src/light.inl:36` | Inserts a deliberate `#error`, so the rewrite must fall back. |

The bits add up: 31 forces all five (`docs/STATUS_2026-10-13.md` line 25 used `ForceFallback=31` for the G1 gate). The README row ("bits 1 fog, 2 GI, 4 AO, 8 relief, 16 light shaders") and `docs/PERFORMANCE.md` line 65 agree with the code. The forced run renders normally at 7.44 ms (PERFORMANCE.md line 40, v0.3.2 gate).

## Debug-view values (Q134)

| Setting | Values | Where | Notes |
|---|---|---|---|
| `[ao] DebugView` or flag file `ao_debug` | 1 = AO only; 2 = normals; 3 = view depth | `src/ao.inl:5` (header), `src/ao.inl:663`, `src/ao.inl:691` | The flag file wins when present (`src/ao.inl:521`); with no file the ini value is used. Debug views are exclusive: while `ao_debug` is non-zero, GI and emitter passes are skipped (`src/tyrender.cpp:291-293`). |
| `[gi] DebugView` or flag file `gi_debug` | 1 = GI only; 2 = character mask | `src/gi.inl:82`, `src/gi2.inl:336`, `src/gi2.inl:341`; flag read at `src/gi.inl:101` | The flag file wins when present. |
| flag file `atmos_debug` (no ini key) | 1 = fog colour only; 2 = sun visibility | `src/atmos.inl:85-86`, `src/atmos.inl:154-155`; read at `src/atmos.inl:312` | Non-zero also skips the half-res path (`src/atmos.inl:481`). There is no `[atmos]` key for it. |
| `[pbr] Debug` | 1 = normals view | `src/pbr.inl:165` | Compile-time: the value inserts a line into the rewritten shader. Other values not traced (Ch04). |
| `[tess] Debug` | non-zero = dark triangle edges | `src/tess.inl:248`, `src/tess.inl:374` | Compile-time insert. |
| `[debug] ShadowView` | 1 = shadow term alone | `src/light.inl:24`, `src/light.inl:117` | See the Discrepancies on live versus restart. |

## [report]

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[report] Enable` (int 0/1) | `1` (sesslog.inl:372) | `1` (ini:93) | Restart (read in `SessInit`, sesslog.inl:370; called once from DllMain, tyrender.cpp:731) | Turns the session report on. The report is written locally to `tyrender\logs` and is never sent (ini:91-92). |
| `[report] CaptureSeconds` (int, clamped 0 to 3600) | `300` (sesslog.inl:373) | `300` (ini:94) | Restart (inferred, as above) | Length of the frame-time window at the start of the session. `0` = none. |
| `[report] MaxBytes` (int, minimum 65536) | `1048576` (sesslog.inl:374) | `1048576` (ini:95) | Restart (inferred) | Size cap per report file. |
| `[report] KeepFiles` (int, minimum 1) | `10` (sesslog.inl:375) | `10` (ini:96) | Restart (inferred) | Number of newest reports kept. |

Settings changes made by the launcher or the overlay are logged into the report as `setting changed (<why>): section.key = value`, at most 20 lines per write (`src/sesslog.inl:131-145`).

## [shots] and [overlay]

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[shots] Key` (F1 to F24, or a virtual-key number; 0 = off) | `F11` (abshot.inl:22) | `F11` (ini:99) | Live (inferred; the reload path calls `LoadShotConfig` at tyrender.cpp:598) | Hotkey for the A/B capture. Parsed by `ShotParseVk` (abshot.inl:16). Hotkey behaviour: [hotkeys and files](../reference/hotkeys-and-files.md). |
| `[shots] Preset` (string) | `custom` (abshot.inl:89) | not in file | Live (read on each capture, abshot.inl:89; also sesslog.inl:283) | Name used in screenshot file names. The launcher writes `Low`, `Medium`, `High`, `Ultra` or `Custom` (`src/launcher_ui.inc:242`). |

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[overlay] Key` (F1 to F24, VK number, 0 = off) | `0` (overlay.inl:43) | `F10` (ini:115) | Live (reload path calls `LoadOverlayConfig`, tyrender.cpp:598) | Key that opens the settings panel. The installer changes a stored `0` to `F10` (installer/setup.c:60-62). The panel itself: [overlay and hotkeys](../how-it-works/09-overlay-input-and-hotkeys.md). |

The panel exposes eight items (overlay.inl:22-30): `[post] Brightness`, `Saturation`, `Sharpen`, `Vignette`; `[atmos] Intensity`; `[ao] Enable`; `[tess] MaxLevel`; `[shadows] Taps`. Each change is written to `tyrender.ini` and then the owning section is reloaded (overlay.inl:64-72).

## [clash]

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[clash] Check` (int 0/1) | `1` (tyrender.cpp:639) | not in file | Restart (read in `CheckGameSettingsClash` at startup, tyrender.cpp:715) | 1 = compare the game's own `settings.ini` with the DLL settings and warn; 0 = no check. |

What the check does (Q121): at startup `CheckGameSettingsClash` reads `[Display]` from the game's `settings.ini` in the user's Documents folder (`<Documents>\TY the Tasmanian Tiger\settings.ini`). It logs a `[clash]` line if `SSAO` is `true` (the DLL's own AO would run too) or if `MSAA` is 2x or more while `[post] AA` is not 0 (`src/tyrender.cpp:637-661`). It changes nothing. The launcher runs the same rules on every status tick and shows the text in its status line (`src/launcher_ui.inc:102-125`; the comment says the check is re-read on each timer tick, so changes show within about 2 seconds). The DLL reads `[post] AA` at startup only; the launcher reads it live.

## Environment variables

| Variable | Read by | Effect |
|---|---|---|
| `TYRENDER_CAPTURE` | `src/tyrender.cpp:63` | Same as `[debug] CaptureFrame`; wins over the ini value. |
| `TY_SLOT` | `tools/ty_play.ps1:33`, `tools/abshots.ps1:10`, `tools/playtest/boot_spot.ps1:25` | Chooses the save slot in harness runs (`2` = the test spot). Harness only; the game does not read it in `src/`. |
| `TY_SETTLE` | `tools/agent_test.ps1:45` | Seconds to wait before a harness capture. Harness only. |
| `TY_CAMWAIT` | `tools/agent_test.ps1:44` | Camera wait for the harness. Harness only. |

These variables are for the test tools. Players do not need them.

## Cost

Not applicable to the switches above, except that `DumpShaders=1` writes a file for every new shader hash the first time it is compiled (`src/tyrender.cpp:369-372`), so a fresh install does disk work at start-up.

## Limits

- `ForceFallback` and the debug views are for tests. Several of them change shader text, so they need a restart to take full effect (see the live or restart column).
- The `[clash]` check only warns. It never edits the game's `settings.ini`.

## Discrepancies

1. **[debug] GpuProf.** `src/gpuprof.inl:3` says `[debug] GpuProf=0` disables the GPU timers. No code reads that key. `g_gpEnable` is set to 1 unconditionally (`src/gpuprof.inl:5`). The plan's Appendix A lists `GpuProf=1` as a key. The first pass also flagged this. The timers cannot be turned off by ini.
2. **[debug] DumpShaders default.** Code default is 1 (`src/tyrender.cpp:60`), so a fresh install writes shader dumps to `tyrender\dump`. The README settings table does not list `DumpShaders` at all. Not a conflict, but users may not expect it.
3. **[debug] ShadowView.** Comment says restart-only (`src/light.inl:24`). Code re-reads it on reload (`src/tyrender.cpp:598` calls `LoadLightConfig`) and writes it into the block (`src/light.inl:117`). See the post and shadows page.
4. **[overlay] Key default.** Code default 0 (`src/overlay.inl:43`), shipped F10 (`ini:115`). The installer changes a stored 0 to F10 (`installer/setup.c:60-62`). Consistent with the first pass, and the first pass's wording is correct.
5. **[shots] Preset case.** Code default `custom` (`src/abshot.inl:89`); the launcher writes `Custom` (`src/launcher_ui.inc:242`) when no preset matches. File names therefore differ in case for the same mix.
6. **README table.** The `[debug]` rows in the README are `ForceFallback` and `StateGuard` only. They match the code. `DumpShaders`, `OverrideShaders`, `CaptureFrame` and `ShadowView` are not in the README.

## NEEDS INVESTIGATION

- **GpuProf owner decision.** Remove the comment (`src/gpuprof.inl:3`) or add the key. Method: owner decision; a code change is out of scope for the docs task.
- **Debug-view values above 1 for [pbr] Debug and [tess] Debug.** Only the first value was traced here. Method: read the rewrite functions in `src/pbr.inl` and `src/tess.inl` (Ch04 and Ch07).
- **ForceFallback bit 1 behaviour in the froxel path.** Method: set `ForceFallback=1`, start the game, and check the `[fallback] FORCED froxel failure` line in `tyrender.log`.

## See also

- [settings-ao-gi-lod.md](settings-ao-gi-lod.md), [settings-atmos-fog-emit.md](settings-atmos-fog-emit.md), [settings-post-shadows-light-pbr-tess.md](settings-post-shadows-light-pbr-tess.md)
- [hotkeys and files](../reference/hotkeys-and-files.md), [log and error messages](../reference/log-and-error-messages.md)
- [debugging guide](../guides/debugging.md), [session report](../guides/session-report.md)
- [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md)
