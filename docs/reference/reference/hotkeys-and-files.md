# Hotkeys, flag files, folders and environment variables

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B, C.

## Purpose

This is the single list of every key, trigger file, output folder, environment variable and game switch that the renderer or its test tools respond to. Use it to find out what a file in `<game folder>\tyrender\` does, or which key to press. The mechanics behind each entry are on [09-overlay-input-and-hotkeys.md](../how-it-works/09-overlay-input-and-hotkeys.md) and [10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md).

Conventions used here:

- `<game folder>` is the folder that holds `TY_tyrender.exe` (the patched copy of `TY.exe`). `tyrender.ini` and `tyrender.log` sit directly in it.
- `<game>\tyrender\` below means `<game folder>\tyrender\`.
- Trigger files are polled every 8 frames unless a row says otherwise. The poll is in `h_SwapBuffers` (`src/tyrender.cpp:594`, `if ((g_frame & 7) == 0)`).

## Hotkeys

| Key | When | Setting | Action | Code |
|---|---|---|---|---|
| F10 (shipped) | Game running | `[overlay] Key`, code default 0 (off) | Opens and closes the settings panel. Works without window focus. | `src/overlay.inl:239` |
| Up / Down | Panel open | none | Select a row. Works only while the game window has focus. | `src/overlay.inl:246-247` |
| Left / Right | Panel open | none | Change the selected value by its step. Writes `tyrender.ini` at once. | `src/overlay.inl:249-250` |
| Enter | Panel open | none | Keep the values and close. | `src/overlay.inl:244`, `95` |
| Esc | Panel open | none | Discard changes and close. | `src/overlay.inl:243` |
| F10 | Panel open | as above | Discard and close (same as Esc). | `src/overlay.inl:243` |
| F11 (shipped and code default) | Game window in front | `[shots] Key` | Writes two screenshots: one with the renderer's extras on, one with them off. | `src/abshot.inl:8`, `22`, `114-116` |

Rules for key values in `tyrender.ini`:

- `F1` to `F24`, or a decimal virtual-key code from 1 to 255. `0` turns the key off (`src/overlay.inl:37-40`, `src/abshot.inl:16-19`).
- Other text, including `F25` and hex codes such as `0x79`, is read as 0 (off).
- The shipped ini sets `[shots] Key=F11` (`src/default_tyrender.ini:99`) and `[overlay] Key=F10` (`:115`). An existing ini keeps its old `[overlay] Key=0`; the installer only converts 0 to F10 at install time (`src/installer/setup.c:60-62`).

### Launcher controls

The launcher is a normal Win32 window. Keyboard use goes through the standard dialog keys (`GetMessageW` and `IsDialogMessageW`, `src/launcher_ui.inc:564`). Gamepad support is loaded at run time from XInput. The DLL has no gamepad code.

| Input | Action | Code |
|---|---|---|
| Pad D-pad or left stick | Move focus between controls (repeats while held) | `src/launcher_ui.inc:376-392` |
| Pad A button | Activate the focused control | `src/launcher_ui.inc:392` |
| First pad press | Shows focus outlines (before that they are hidden) | `src/launcher_ui.inc:298-299`, `386` |

The launcher ignores pad input unless it is the foreground window (`src/launcher_ui.inc:378`).

## Flag and trigger files in `<game>\tyrender\`

| File | Read by | Effect | Deleted by the DLL? | Code |
|---|---|---|---|---|
| `reload` | every 8 frames | Re-reads all `Load*Config` functions, the fog values and the shot and overlay keys. Logs `[cfg] reloaded all` and a settings diff in the report. The launcher creates it through `LiveReload` when it saves, moves a slider or resets, and the game window is running (`src/launcher_ui.inc:166-169`; callers at `246`, `488`, `502`). | yes | `src/tyrender.cpp:597-603` |
| `post_off` | every 8 frames | While the file exists, the post chain (sharpen, grade and related) and TAA jitter are off. | no | `src/tyrender.cpp:596`, `src/fx.inl:162`, `178` |
| `ao_off` | every 8 frames | AO is off while the file exists. | no | `src/ao.inl:520`, `567` |
| `ao_debug` | every 8 frames | Integer AO debug view. Overrides `[ao] DebugView` while present. Values 2 and 3 show the raw AO buffer. Logs `[ao] debug view a -> b`. | no | `src/ao.inl:521`, `663`, `691` |
| `ao_reload` | every 8 frames | Re-reads the AO config and logs `[ao] config reloaded`. | yes | `src/ao.inl:522-523` |
| `atmos_off` | every 8 frames | Atmosphere is off while the file exists. | no | `src/atmos.inl:311`, `362` |
| `atmos_debug` | every 8 frames | Integer atmosphere debug view, default 0. Also turns off the half-resolution path (`src/atmos.inl:481`). | no | `src/atmos.inl:312` |
| `gi_off` | every 8 frames | GI is off while the file exists. | no | `src/gi.inl:100`, `105`, `src/gi2.inl:39` |
| `gi_debug` | every 8 frames | Integer GI debug view. Falls back to `[gi] DebugView` when the file is absent. | no | `src/gi.inl:101`, `182` |
| `cap.req` | every 8 frames | The next frame is logged call by call between `=== BEGIN CAPTURE` and `=== END CAPTURE`. | yes | `src/tyrender.cpp:624-628` |
| `cam.txt` | every 8 frames | Debug camera offset: `dx dy dz [yaw pitch]` in world units and degrees. Three numbers are the minimum. Delete the file to return to the game camera. | no | `src/debugcam.inl:8-14` |
| `motion.req` | every 8 frames | Starts a motion capture (format below). | yes | `src/debugcam.inl:47-52` |
| `snap.req` | every 8 frames | Writes one 24-bit BMP of the back buffer to `snaps\<name>.bmp`. The whole file content, with trailing spaces and line breaks removed, is the name. Empty content gives `snap_<frame>`. | yes | `src/tyrender.cpp:506-537` |
| `texture_tags.csv` | at start-up only | Override of the built-in texture tags. Read in `LoadTexTags`, which is not on the reload list. | no | `src/tex.inl:83`, `src/tyrender.cpp:725` |
| `dump\` (folder) | when a shader is compiled, if `[debug] DumpShaders` is 1 (default) | One file per shader source, named `<hash>.vs.glsl`, `.fs.glsl` or `.xs.glsl`. A file is written only if none with that name exists. | no | `src/tyrender.cpp:369-372` |
| `override\` (folder) | when a shader is compiled, if `[debug] OverrideShaders` is 1 (default) | A file named `<hash>.glsl` replaces that shader's source if it compiles. | no | `src/tyrender.cpp:374-375` |
| `logs\` (folder) | at start | Session reports, see [session-report.md](../guides/session-report.md). | no | `src/sesslog.inl:259-263` |
| `snaps\` (folder) | on snapshot or motion capture | Output of `snap.req` and `motion.req`. | no | `src/tyrender.cpp:524`, `src/debugcam.inl:72` |

Other files in the game folder that the renderer creates:

| Path | Created when | Code |
|---|---|---|
| `<game folder>\tyrender.log` | every start (replaces the old one) | `src/tyrender.cpp:708` |
| `<game folder>\tyrender\` | every start | `src/tyrender.cpp:709` |
| `<game folder>\tyrender\dump\`, `override\` | every start | `src/tyrender.cpp:710-711` |
| `<game folder>\Screenshots\` | first F11 press | `src/abshot.inl:92` |

### `motion.req` format

One line: `name yawStepDeg frames [x y z yaw pitch] [fwd side sleepMs]`. Defaults: step 0.1 degrees per frame, 14 frames (clamped to 4 to 40), camera unchanged. The DLL logs `[motion] request seen`, deletes the file, and runs for that many frames at 640 by 360 grey. The result is `<game>\tyrender\snaps\motion_<name>.raw`: three little-endian int32 values (width, height, frame count), then the frames (`src/debugcam.inl:42-76`). The DLL reads the file. In the repo, the request is written only by `tools/agent_test.ps1:55`.

### `cam.txt` and `[tess] ABToggle`

When `[tess] ABToggle=1` (default 0, `src/tess.inl:69`), the relief and PBR extras flip each time the content of `cam.txt` changes (`src/pbr.inl:206-211`). This is a test aid. Use F11 for screenshot pairs.

## Environment variables

| Name | Read by | Effect | Code |
|---|---|---|---|
| `TYRENDER_CAPTURE` | the DLL, at start-up | Frame number to capture call by call. Overrides `[debug] CaptureFrame`. | `src/tyrender.cpp:62-63` |
| `TY_SLOT` | `tools/ty_play.ps1`, `tools/abshots.ps1` | Save slot the boot script selects (`2` or other). | `tools/ty_play.ps1:33`, `tools/abshots.ps1:10` |
| `TY_SETTLE` | `tools/agent_test.ps1`, `tools/ab2_run.ps1` | Seconds to wait before the capture. | `tools/agent_test.ps1:45`, `tools/ab2_run.ps1:1` |
| `TY_CAMWAIT` | `tools/agent_test.ps1`, `tools/ab2_run.ps1` | Seconds to wait for the camera. | `tools/agent_test.ps1:44` |
| `TY_PERFCAM` | `tools/g1_gate.ps1`, `tools/agent_test.ps1` | Camera line written to `cam.txt` for a performance run. | `tools/g1_gate.ps1:9`, `tools/agent_test.ps1:64` |
| `TY_LOCK_TOKEN` | `tools/playtest/common.ps1` | Token for the global game lock. Set by the tool, not by the user. | `tools/playtest/common.ps1:105-122` |

The DLL reads only `TYRENDER_CAPTURE`. The `TY_*` variables are for the test scripts in `tools/`.

## Game command-line switches used by the tools

| Switch | Used by | Notes |
|---|---|---|
| `-skipvideo` | `tools/ty_play.ps1:5` (default), most test scripts | Skips the intro video. |
| `-novsync` | `tools/perf.ps1:15`, `tools/gpucmp.ps1:5`, `tools/agent_test.ps1:33` | Test runs only. The DLL does not change vsync. What the game does with the switch is not in the repo (see NI). |
| `-level z1` | none now | Stopped working in windowed mode (`tools/ty_play.ps1:3`). The tools load a save instead. |
| `-DebugModeCamera x,y,z,rx,ry,rz` | none in the repo | Mentioned as a test view in `src/pbr.inl:229` and `docs/START_HERE.md:87`. Not used by any script here. |

## Code pointers

| What | file:line |
|---|---|
| Key parsers | `src/overlay.inl:37-40`, `src/abshot.inl:16-19` |
| Shipped ini keys | `src/default_tyrender.ini:93-99`, `:114-115` |
| Flag polls | `src/tyrender.cpp:594-604`, `src/ao.inl:518-524`, `src/atmos.inl:309-313`, `src/gi.inl:98-102` |
| Trigger for capture | `src/tyrender.cpp:622-628` |
| Snapshot | `src/tyrender.cpp:506-537` |
| Motion capture | `src/debugcam.inl:42-76` |
| Shader dump and override | `src/tyrender.cpp:369-375` |
| Texture tag override | `src/tex.inl:83` |
| Launcher pad handling | `src/launcher_ui.inc:294-392` |

## Settings

| key | default in code | shipped | live or restart | effect |
|---|---|---|---|---|
| `[overlay] Key` | `"0"` (`overlay.inl:43`) | `F10` (`default_tyrender.ini:115`) | live (reload list) | panel toggle |
| `[shots] Key` | `"F11"` (`abshot.inl:22`) | `F11` (`:99`) | live (reload list) | A/B pair |
| `[debug] CaptureFrame` / `CaptureFrame2` | 0 (`tyrender.cpp:58-59`) | not shipped | restart | call-by-call capture frames |
| `[debug] DumpShaders` | 1 (`tyrender.cpp:60`) | not shipped | restart | write `dump\` files |
| `[debug] OverrideShaders` | 1 (`tyrender.cpp:61`) | not shipped | restart | read `override\` files |
| `[tess] ABToggle` | 0 (`tess.inl:69`) | not shipped | live (reload list) | harness toggle |

Full key lists: [settings-system-debug.md](settings-system-debug.md).

## Discrepancies

1. **`snap.req` name.** `src/tyrender.cpp:504` comments say "its first line, if any, is the output name". The code reads the whole file (`ReadFileStr`, `src/tyrender.cpp:134`), trims trailing whitespace, and uses the rest as the name. A multi-line file gives a name with line breaks. Code wins.
2. **`snap.req` name is not checked.** The name goes into the file path as written (`src/tyrender.cpp:530`). Characters that are invalid in Windows file names will make the write fail silently (the file is opened with `fopen` and no error is logged). Inferred from the code; not tested.
3. **`post_off` description.** The first-pass file says it "bypasses the post-processing chain". Code: it also turns off TAA jitter (`src/fx.inl:162`) and the whole chain (`fx.inl:178`). Both are covered above.
4. **`[overlay] Key` default.** The plan's Appendix A lists the code default as 0 and the shipped value as F10. Both are right; see the installer note above.
5. **First-pass coverage.** The first-pass `OVERLAY_AND_HOTKEYS.md` section 4 did not name `snap.req`, `ao_reload`, `atmos_off`, `atmos_debug`, `gi_off` or `gi_debug`. They are in the table above.
6. **Motion tool.** `tools/motion.ps1` describes the motion test in its header, but grep finds no line in that script that writes `motion.req`. The writer is `tools/agent_test.ps1:55`. Whether `motion.ps1` reaches it through another script was not checked (see NI-H5).

## NEEDS INVESTIGATION

- **NI-H1: what the game does with `-novsync`.** The DLL has no vsync code (search of `src/` for `SwapInterval` and `novsync` returns none). Method: compare frame pacing with and without the switch in a test run, or inspect the game binary in the reverse-engineering project (not in the repo; plan item N-17).
- **NI-H2: does `-DebugModeCamera` do anything in this build?** Method: start the game with the switch and watch whether the camera follows the given values. Not in the repo's scripts.
- **NI-H3: what other game switches exist (`-StopFrame`, `-fastLoad`, `-tydev`, `-disableEnemies`).** Only mentioned in `docs/START_HERE.md:87`. Method: test each switch with a clean copy of the game folder.
- **NI-H4: does `ao_debug` value 1 match `[ao] DebugView=1`?** The code path is `g_aoDebug` in `src/ao.inl`. Method: set each value and compare screenshots (see the AO page).
- **NI-H5: how `tools/motion.ps1` starts a motion capture.** Its header describes the test, but the only writer of `motion.req` in `tools/` is `tools/agent_test.ps1:55`. Method: read `tools/motion.ps1` in full and trace its calls.

## See also

- [09-overlay-input-and-hotkeys.md](../how-it-works/09-overlay-input-and-hotkeys.md): what the keys do, step by step.
- [10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md): the log and report that the triggers write to.
- [log-and-error-messages.md](log-and-error-messages.md): the log lines each trigger produces.
- [settings-system-debug.md](settings-system-debug.md): the `[debug]`, `[shots]`, `[overlay]` and `[report]` keys.
- [debugging.md](../guides/debugging.md): using these files in a bug hunt.
