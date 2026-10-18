# Tester guide

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (testers and modders).

## Purpose

This guide is for people who install the renderer, play with it, and send reports. It covers the first session, the checks worth making, how to take a comparison pair, where the logs are, a symptom table, and what to send. It does not cover building or code. Those are in [contributor-onboarding.md](contributor-onboarding.md).

The build is a community beta. The README says the latest release is v0.3.5 and that changes on `main` after it are listed in the changelog (`README.md:9`). Untested features are marked untested in the changelog (`CHANGELOG.md:3`). The renderer was tested on one graphics card, an AMD Radeon RX 7600 XT on Windows 11 (`README.md:11`).

## How it works

### Install

Two ways (`README.md:180-200`):

1. **Setup.** Run the Setup program from the release. It finds the game in the Steam libraries, or asks for the folder that contains `TY.exe`. It adds an entry to Apps and Features. Files it replaces are backed up in `tyrender\backup` (`README.md:184-185`). The installer code is in `src/installer/setup.c` and `src/installer/common.inc`; Steam lookup is `src/installer/common.inc:145`.
2. **By hand.** Put `TYLauncher.exe` in the game folder, pick a preset, and press **Play with HD renderer**. The first Play installs the renderer files and creates `TY_tyrender.exe`, a copy of `TY.exe` that loads the DLL. `TY.exe` is not changed (`README.md:187-194`, `src/patch_exe.inc:2`).

**Play original (no mod)** starts the untouched game (`README.md:198`). Uninstall with the uninstaller in the game folder, or by hand (`README.md:202-207`).

### First session

1. Start with the **Medium** preset. It is the default and the one the author plays (`README.md:163`, `src/default_tyrender.ini:1`).
2. Press Play. Check the title bar for the build id (`README.md:196`).
3. Play for a few minutes in a level. Look at the scenes in the checklist below.
4. Try one change at a time with the launcher sliders. Most apply live (`README.md:193`). If a slider seems to do nothing, change it again. The renderer reads the reload file every 8 frames (`src/tyrender.cpp:594-597`).
5. Use **F11** for a comparison pair when something looks wrong (see the next section).

### What to look at

The README lists what the mod changes (`README.md:15-30`). A short checklist:

- **Shadows.** Softer edges, a larger map and a cooler shade colour.
- **Surface relief.** Cracks, grain, wood and rope get bumps. Characters are not changed.
- **Contact shadows (AO).** Darker crevices and corners, steady from frame to frame.
- **Haze.** Fog and light shafts. Distant rock loses a little colour.
- **Bounce light.** High and Ultra only (`README.md:25`).
- **Edges.** TAA or FXAA smoothing, with gentle sharpening and grading.
- **Water and the portal.** Known issues; see Known issues below.

### F11 comparison pair

1. Start a level, stand still, and press **F11** once with the game window in front. The key does nothing when the game is not in front (`src/abshot.inl:116`).
2. Two PNG files appear in `<game folder>\Screenshots\`. Their names contain `shaders-on` and `shaders-off` (`src/abshot.inl:4`, `:93`).
3. The "off" frame is not the stock game. The shadow filter and the tessellated draw path still run (`README.md:35`). Read it as "F11 off", not as vanilla.
4. Send both files for a visual comparison (see Send a report).

### Find the logs

- **Renderer log**: `<game folder>\tyrender.log`. The file is recreated at each start (`src/tyrender.cpp:708`), so copy it before you start the game again.
- **Session report**: `<game folder>\tyrender\logs\TYHD_report_<time>_<build>.txt`, one per start. Written locally and never sent (`src/sesslog.inl:1-2`, `:259-262`). Settings are in `[report]` (`src/default_tyrender.ini:90-96`): `Enable=1`, `CaptureSeconds=300`, `MaxBytes=1048576`, `KeepFiles=10`. The file has a header with build, hardware and software basics, frame-time rows for the first 300 seconds, and event lines. Events include GL errors, shader failures, settings changes, screenshots and window changes (`src/sesslog.inl:1-6`). It does not list usernames, computer names, serial numbers, addresses or personal folder paths (`README.md:244`). The full description is in [session-report.md](session-report.md).
- **Game log**: `<game folder>\TyOutputLog.txt`, written by the game itself (`docs/START_HERE.md:89`).
- **Report button**: the launcher's Advanced tab opens the logs folder (`README.md:234`).

### Symptom table

Each row gives the likely cause with its source, and the first thing to try. Defaults are shipped values unless marked "code".

| Symptom | Likely cause | Try first |
|---|---|---|
| Setup cannot find the game | Steam lookup failed, or the folder is not the game folder (`src/installer/common.inc:145`). | Pick the folder that contains `TY.exe` by hand (`README.md:184`). |
| Steam Play button does not start the mod | The Steam launch option is not set. Setup and the launcher write it (`src/installer/setup.c:115`, `src/ui_part_c.inc:210`). | Paste the launch option that Setup shows, or start `TYLauncher.exe`. |
| Log line `CMKHeap - Out of memory` | The 32-bit game ran out of address space. `TY_tyrender.exe` is marked Large Address Aware (`README.md:200`, `src/patch_exe.inc:38`). | Start the game with Play, not with `TY.exe`. |
| Sliders do nothing while the game runs | The reload file is checked every 8 frames (`src/tyrender.cpp:594-597`). | Move the slider again. Restart the game if nothing changes. |
| Preset button says "Custom mix" | The settings differ from the preset (`src/ui_part_c.inc:95`). | Click the preset again. |
| Warning about the game's own SSAO or MSAA | The game's options repeat the mod's AO or AA (`src/launcher_ui.inc:121`, `src/tyrender.cpp:637`). | Turn off the duplicate option in the game's settings. The check is `[clash] Check` (code default `1`, `src/tyrender.cpp:639`). |
| Blue flicker or slow brightness ramp on Ty in water (#5) | Water shading and light. `WaterStable` is on by default (`src/default_tyrender.ini:59`). README says the ramp is the current symptom, not a frame flicker (`README.md:213`). | Leave `WaterStable` on. Send a session report and a pair. |
| Lily pads and rocks brighter than the water under them (#4) | Water fog and water names. `WaterBoost` is `1.0`, which is off, and `WaterNames` is empty (`src/default_tyrender.ini:81`, `:89`). The shipped note says to try 2.0 to 3.0 in a playtest (`src/default_tyrender.ini:80`). | Try a higher `[fog] WaterBoost` in `tyrender.ini`, one step at a time. |
| Fire is not hazed | Glowing draws come after the fog pass. `LateEmitters` and `LateEmitBoost` are off by default (`src/default_tyrender.ini:78`, `:83`). The changelog lists fire as not fogged (`CHANGELOG.md:36`). | Leave the defaults. Report with a pair. |
| Trees look grey or dark in fog | See-through depth and foliage fog. `SeeThruDepth` is off by default (`src/default_tyrender.ini:77`), and `CHANGELOG.md:16` marks it untested. Trees can look dark on level 2 (`CHANGELOG.md:63`). | Test `[fog] SeeThruDepth=1` on its own. |
| Checkerboard or bright line on character or grass shadows | Shadow filter. Open at this commit (`CHANGELOG.md:63`, `README.md:215`). | Compare with `[shadows] Patch=0`. The shipped value is `1` (`src/default_tyrender.ini:46`). |
| Footprints glow | Bright decal draws. Decal clamp is off by default (`src/default_tyrender.ini:69-70`). | `[decal] Clamp=1` to test. |
| Portal does not light the ground | Emitter light is off by default (`src/default_tyrender.ini:107`). Enabling it drew a flat white band in one test (`CHANGELOG.md:39`). | Leave it off. Report with a pair. |
| Trails behind Ty when turning | TAA history. TAA is the default AA mode (`src/default_tyrender.ini:51`). Ghosting was reduced in v0.3.5 (`CHANGELOG.md:51`) and is issue #2. | Compare with `[post] AA=1` (FXAA). |
| Fog colour changes when the camera turns | Fixed in v0.3.5 (#3, `CHANGELOG.md:49`). The fix is untested after a later change (`CHANGELOG.md:42`). | Update to the current build. |
| Shimmer or flicker at High or Ultra | Reported in playtest 4 (`README.md:163`). | Use Medium. Report with logs. |
| Ultra is very slow | Cost of the preset (`README.md:165-178`). | Use Medium or High. |
| Black screen after a setting change | A shader failed to compile, or a rewrite was rejected. The effect switches off and the frame is drawn without it (`README.md:265`, `src/tyrender.cpp:380`). | Find `FAILED` or `does not compile` in `tyrender.log` (`docs/START_HERE.md:89`). Send the log. |
| Crash | The session report ends with a `CRASH exception ...` line (`src/sesslog.inl:362-365`). | Send the newest `TYHD_report_` file. |
| No atmosphere on an older card | The atmosphere's compute volume needs GL 4.3. Its fallback depends on `ContactShadows` (`src/atmos.inl:254`, `:371`). | Check `tyrender.log` for `[fallback]` lines. See [debugging.md](debugging.md) Discrepancies. |
| No relief on tessellated surfaces on an older card | Tessellation is skipped below GL 4.0 when `TessVersionGate=1` (`src/default_tyrender.ini:101-102`, `src/tess.inl:397`). | Expected on older GL. Report the GL version. |
| F10 overlay does nothing | The code default is `0`, which is off (`src/overlay.inl:43`). An existing ini keeps `Key=0` from an older build (`CHANGELOG.md:22`, `README.md:225`). The shipped ini says `F10` (`src/default_tyrender.ini:115`). | Set `[overlay] Key=F10`. |
| F10 overlay breaks the scene | Earlier playtest; the overlay is untested in a level (#12, #79; `README.md:225`). | Set `[overlay] Key=0` to turn it off (`src/overlay.inl:1`). Report. |

### Flicker or ramp

The README describes the water problem as a slow ramp in brightness, not frame-to-frame flicker (`README.md:213`). When you report it, say which one you see, and when it starts. Give a short clip with a timestamp if you can.

### Send a report

From the README (`README.md:236-248`):

1. The newest `TYHD_report_` file from `tyrender\logs\`.
2. A screenshot of the problem. An F11 pair is best.
3. The build id and the preset. Both are in the report file name and the launcher title.
4. Your GPU model and driver version. The report has them too.
5. For water or flicker problems, a short clip with a timestamp.
6. Relevant lines of `tyrender.log`, for example `[state]`, `[gpu]` or `[tex]`.

The renderer does not send anything. You choose what to send (`src/sesslog.inl:1`, `README.md:246`). Do not send `TY.exe` or game data. The mod ships none of the game's content (`README.md:258`).

Hardware reports are wanted. NVIDIA, Intel and Linux or Proton reports are especially welcome, because those are untested (`README.md:11`, `README.md:230`).

### Cost and presets

Frame times in the README are GPU milliseconds measured by the author on one card at 2560 by 1440 (`README.md:153`). They are not measured here. Use the presets table for the trade-off, and [reference/presets.md](../reference/presets.md) for the exact values.

## Cost

Not measured on this page. The preset costs and the per-option costs are the author's numbers on one card (`README.md:151-178`).

## Limits

- **One card tested**, RX 7600 XT on Windows 11 (`README.md:11`).
- **One game build tested**: the 2020-04-04 `TY.exe`. Other builds are untested (`README.md:226`).
- **Off frame is not vanilla** (`README.md:35`).
- **Screenshots from different sessions are not pixel-comparable** (`docs/START_HERE.md:76`). Compare pairs taken in the same session.
- **Water and portal** are known issues at this commit (`README.md:149`, `README.md:213`).

## Known issues

The README lists the open items with their issue numbers (`README.md:209-226`): water brightness and light (#5), lily pads and rock brightness and water fog (#4), the F10 overlay (#12, #79), water outline from above (#104), and the game version (`README.md:226`). The history is in [decisions/known-issues.md](../decisions/known-issues.md).

## How to verify

Steps that show the install works (tester):

1. The title bar shows a build id after Play (`README.md:198`).
2. Press F11 once. Two `ty_..._shaders-on_...png` and `..._shaders-off_...png` files appear in `Screenshots\` (`src/abshot.inl:93`).
3. Open `tyrender.log` and find `[shot]` lines (`src/abshot.inl:94`).
4. Turn `[overlay] Key` to `F10` and press F10 once. The panel opens (`src/overlay.inl:1-2`). Press Esc to close without changes.

## NEEDS INVESTIGATION

- **P12-14** Whether the F10 overlay is safe in a level. The changelog says untested (`CHANGELOG.md:22`). Method: owner playtest, or the in-level overlay check in [test-harness.md](test-harness.md) (N-24).
- **P12-15** Whether the Steam launch option works with Steam's overlay and achievements, given a renamed exe. Method: test in Steam (N-9).
- **P12-16** Linux and Proton behaviour. Method: a Proton run (N-10).
- **P12-17** Whether the gamepad menus work as documented. The launcher has XInput navigation (`CHANGELOG.md:27`). Method: a controller test on the launcher.

## Discrepancies

1. The README gives two short lists for reporting. `README.md:230` asks for `tyrender.log`, a screenshot, the GPU model and the driver. `README.md:236-239` asks for the newest `TYHD_report_` file first, then a screenshot. They do not conflict, but the session report is the better file. The guide above follows the second list.
2. First-pass `TROUBLESHOOTING.md` says the Steam launch option is `--play %command%`. The full value that Setup shows is `"<game folder>\TYLauncher.exe" --play %command%` (`src/installer/setup.c:115`). Testers should copy the value Setup shows.
3. First-pass `TROUBLESHOOTING.md` cites `src/atmos.inl 260` for the atmosphere volume init. The function starts at `src/atmos.inl:254`.
4. `README.md:33` gives build `3ee6ea7` for the screenshot pairs. `CHANGELOG.md:28` says "three build 4a2ef95 pairs". See [ab-captures.md](ab-captures.md).

## See also

- [ab-captures.md](ab-captures.md): how the F11 pair is made and read.
- [session-report.md](session-report.md): the report file, its sections and privacy rules.
- [debugging.md](debugging.md): what the developers look at.
- [../reference/presets.md](../reference/presets.md): preset values and costs.
- [../reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md): every key and file.
- [../decisions/known-issues.md](../decisions/known-issues.md): open issues with their history.
