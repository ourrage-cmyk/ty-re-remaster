# FAQ

**Status:** community beta. Answers are checked against the repo clone at HEAD `4db1d7d` (2026-10-18). Each answer cites `path:line` in the repo. Items marked "(inferred)" or "NEEDS INVESTIGATION" are not confirmed by the code.

**Audience:** testers (T1-T20) and contributors (C1-C20). Testers can skip the second half. Contributors should read the first half for the user-facing behaviour they are changing.

## For testers

### T1. Which preset should I pick?
Start with **Medium**. It is the default and the one that held up best in playtests. High and Ultra cost more GPU time, and High and Ultra flickered in playtest 4 (README.md:163; README.md:155-162 for the table). Pick Low if the frame rate is too low. See [tester-guide](guides/tester-guide.md).

### T2. Why does F10 do nothing?
The overlay key is off by default in code (`[overlay] Key` defaults to `0`, which means off; `src/overlay.inl:43`, `src/overlay.inl:1`). The shipped ini sets `Key=F10` (`src/default_tyrender.ini:115`). Older `tyrender.ini` files keep `Key=0`, so set `Key=F10` by hand in the game folder (CHANGELOG.md:22; README.md:225). See [debugging](guides/debugging.md).

### T3. Why is my frame rate lower than the table says?
The table is the author's GPU, at 2560x1440 windowed, on one AMD Radeon RX 7600 XT (README.md:151-178). Your GPU, resolution and the Ultra AO and bounce light settings change the result. To find the cost of one effect, turn it off in the launcher and compare (README.md:171-172 give rough per-effect costs). The rough costs are the author's numbers, not measured on your card.

### T4. How do I uninstall?
Run `Uninstall.exe` in the game folder, or use Apps and Features > TY HD Renderer (mod) (README.md:202-207; `src/installer/uninstall.c:1-3`). The uninstaller removes only what the install added, puts back the originals it saved from `tyrender\backup`, and asks whether to keep your `tyrender.ini` (README.md:205). Your game files are not touched (`src/installer/uninstall.c:2-3`).

### T5. Does it work on NVIDIA, Intel or Linux/Proton?
Not tested yet. The only tested setup is one AMD card on Windows 11 (README.md:11). Reports from other setups are welcome. The code avoids vendor-specific calls and probes features at runtime (docs/START_HERE.md:80). Proton behaviour is unknown. NEEDS INVESTIGATION: run the game under Proton and send a report (see T10). See [known-issues](decisions/known-issues.md).

### T6. Does the mod change `TY.exe` or my game files?
`TY.exe` is never modified (README.md:7 and README.md:192; `src/patch_exe.inc:2`). The first Play creates `TY_tyrender.exe`, a copy of `TY.exe` that loads `tyrender.dll`. The copy is marked Large Address Aware (`src/patch_exe.inc:38`). "Play original" starts the untouched game (README.md:198).

### T7. Does the mod touch my save files?
NEEDS INVESTIGATION. The docs do not describe save files. A limited search of `src` and `tools` found no code that writes to save paths (inferred: the mod does not write saves). To check, watch the game folder and the user profile with a file-system monitor during a save, and compare. Until then, do not assume either way.

### T8. Where are the logs?
- `<game folder>\tyrender.log`, opened for writing at each start (`src/tyrender.cpp:708`).
- The engine's own output log, `TyOutputLog.txt`, shows shader compile errors (docs/START_HERE.md:84).
- Session reports are in `<game folder>\tyrender\logs\` (`src/sesslog.inl:259-262`; see T9).

See [debugging](guides/debugging.md) for what each line means.

### T9. How do I send a useful report?
The session report is on by default (`src/default_tyrender.ini:93`). Reproduce the problem, then send the newest `TYHD_report_<time>_<build>.txt` from `<game folder>\tyrender\logs\` with your F11 pair if you have one (README.md:236-248). The report stays on your computer and is never sent by the mod (`src/default_tyrender.ini:91`). Limits: `MaxBytes` and `KeepFiles` (`src/default_tyrender.ini:95-96`). See [tester-guide](guides/tester-guide.md) and [session-report](guides/session-report.md).

### T10. Why does the fog look different from before?
Fog changed in several builds. The colour no longer changes with camera direction, and the fog is 20% thinner by default and less tinted (CHANGELOG.md:49). The hue stays fixed as the camera turns (CHANGELOG.md:42), and that part was untested after the latest change. If the fog is too strong, lower it in the launcher or in `[fog]` (`src/default_tyrender.ini:76-89`).

### T11. Can I turn off one effect?
Yes. Each effect has its own `Enable` key as well as the master switch (`src/default_tyrender.ini:15`). In the launcher, set the effect's slider to off. For a quick A/B test, a flag file works: create `<game folder>\tyrender\ao_off`, `gi_off`, `atmos_off` or `post_off` (`src/ao.inl:5` and `:520`; `src/gi.inl:6` and `:100`; `src/atmos.inl:6` and `:311`; `src/fx.inl:6`). The flag is read every 8 frames.

### T12. What is F11?
F11 takes a pair of screenshots: one with the effects on, and one with the relief and PBR extras flipped. Both go to `<game folder>\Screenshots\`, named `ty_<build>_<preset>_<shaders-on|shaders-off>_<stamp>.png` (`src/abshot.inl:1-4`). The key is set by `[shots] Key` (`src/default_tyrender.ini:99`). See [ab-captures](guides/ab-captures.md).

### T13. Why does the game say "CMKHeap - Out of memory"?
The 32-bit game ran out of address space with the HD texture pack. `TY_tyrender.exe` is marked Large Address Aware, which gives 4 GB instead of 2 GB (README.md:200; `src/patch_exe.inc:38`). If you still see it, check whether `TY_tyrender.exe` is the one you launched. Use Play, not `TY.exe`.

### T14. Why does the water flicker?
Some builds had a blue flicker on Ty in water. Build `4a2ef95` changed the light on Ty to ramp up and down slowly in water instead of flickering (README.md:213; issue #5). The lily pads and rocks look brighter than the water under them, which is still open (README.md:213). For a bisect, use `[pbr] DebugWater` (`src/default_tyrender.ini:61`). See [debugging](guides/debugging.md).

### T15. The settings clash warning says the game's SSAO or MSAA is on. What now?
The game has its own SSAO and MSAA, and the mod's warning fires when either is on. Turn them off in the game's options. The check writes a log line (`src/tyrender.cpp:637`; CHANGELOG.md:20). The warning is controlled by `[clash] Check` (CHANGELOG.md:20). The launcher text is at `src/launcher_ui.inc:121`.

### T16. Why is "Custom mix" shown in the launcher?
You changed a slider after choosing a preset, so the settings no longer match any preset. Click a preset to go back to its values (`src/ui_part_c.inc:95`).

### T17. My sliders do not seem to apply. Why?
Some settings apply live and some need a restart. A setting marked "Live" applies while the game runs, through the launcher or the `tyrender\reload` file (README.md:270). Settings marked "(restart)" need a restart, such as `[shadows] MapScale` and `[tess] Enable` (README.md:300-314). The reload file is polled every 8 frames (`src/tyrender.cpp:594-603`).

### T18. Can I edit the settings by hand?
Yes. Edit `tyrender.ini` in the game folder, or open it from **Advanced (ini)...** in the launcher (README.md:263; README.md:198). Keep a copy of the original before you edit it.

### T19. How do I add the mod to Steam?
Right-click the game in Steam, open Properties > Launch options, and paste the text the installer shows. It has the form `"<game folder>\TYLauncher.exe" --play %command%` (`src/installer/setup.c:115`). The installer's own wording is in `src/installer/setup.c:115`.

### T20. Is this an official mod?
No. It is an unofficial, community-made fan project, not made by or connected to the game's developers or publishers (README.md:7). You need your own copy of the game. The mod includes no game files.

## For contributors

### C1. Where do I start?
Read [contributor-onboarding](guides/contributor-onboarding.md), then [what-is-this](concepts/what-is-this.md) and [source-map](reference/source-map.md). The repo layout and working rules are in docs/START_HERE.md:68-85 and `README.md`.

### C2. How do I build?
Run `src\tyrender\build.ps1` (usage at `src/build.ps1:2`). The build is 32-bit with llvm-mingw i686 and a static runtime (`src/build.ps1:1`; docs/START_HERE.md:80). Output goes to `src\tyrender\out\`. The `-Install` switch copies the result into a game folder. See [build-from-scratch](guides/build-from-scratch.md).

### C3. Why a forward renderer and not a deferred one?
The game draws into the default framebuffer, and the mod hooks into that flow (docs/START_HERE.md:85). The design notes for the world-space AO, GI and fog are in `docs/research/WORLD_SPACE_AO_GI_FOG.md`. The reason for the forward choice is not written as a single decision (inferred from the hook point). NEEDS INVESTIGATION: find the decision in [decisions-log](decisions/decisions-log.md).

### C4. Why only screen-space passes?
The passes run on the frame buffer and the copied depth (`src/ao.inl:1-3`; docs/START_HERE.md:85). Reading only screen data is the design (inferred from the hook point; the reason is not written in the repo). The limits are listed in [constraints-and-limits](concepts/constraints-and-limits.md).

### C5. How do I add a setting?
Add the key to `src/default_tyrender.ini`, read it in the matching `Load*Config` function, and add it to the reload list in `src/tyrender.cpp:597-603` if it should be live. Then add the slider and the README row. The full steps are in [add-a-setting](guides/add-a-setting.md).

### C6. Why do some settings need a restart?
Some values are used when resources are created, such as the shadow map size and the tessellation setup (README.md:300-314, "(restart)"). The reload handler re-reads the config and does not rebuild them (`src/tyrender.cpp:597-603`; inferred from the calls it makes). Mark a new key "(restart)" in README.md if it has this behaviour.

### C7. Why does the reload handler call only some config loaders?
The reload handler calls `LoadAOConfig`, `LoadFXConfig`, `LoadAtmosConfig`, `LoadGIConfig`, `LoadEmitConfig`, `LoadShadowConfig`, `LoadPbrConfig`, `LoadTessConfig`, `LoadLightConfig`, `LoadShotConfig` and `LoadOverlayConfig` (`src/tyrender.cpp:597-603`). `LoadTexTags` runs only at start (`src/tyrender.cpp:725`). A new loader is probably not reloaded unless it is added to the list (inferred).

### C8. Why are merges done through a staging tree?
The master tree is never touched by agents. Work is merged into a staging tree, built, and tested there first (docs/START_HERE.md:68). The stated reason is shared state (inferred from the rule's wording). The merge script is `tools/merge_agent.ps1:1`. Max three parallel agents is the stated limit (docs/START_HERE.md:68).

### C9. How do the tests run?
Game tests run with the playtest harness and the PowerShell scripts in `tools/`. The harness takes the lock, checks the phase file, boots the game and checks flicker, screenshots and logs. See [test-harness](guides/test-harness.md) and [ab-captures](guides/ab-captures.md).

### C10. What does "phase TEST" mean?
Game tests run only when the phase file reads `TEST`. `Assert-TestPhase` throws if it does not (`tools/playtest/common.ps1:96-100`). The assertion message says the phase is controlled by the plan thread, so do not edit the file by hand. The lock file is `game.lock` in the lock folder (`tools/playtest/common.ps1`, `Assert-TestPhase`).

### C11. What is the playtest variant list?
Each variant (for example `stock`, `mod`, `overlay`, `fog_seethru1`, `emit1`) is defined in `tools/playtest/autoplaytest.ps1:51-79`. A plan must use every variant it asks for, or the run refuses to start (`tools/playtest/autoplaytest.ps1:10`). See [test-harness](guides/test-harness.md).

### C12. How do I file a playtest request?
Add a `req-<id>.json` file to `tools/playtest/requests/`, using `TEMPLATE.json` in that folder. `build_plan.ps1` merges pending requests before each playtest (`tools/playtest/requests/README.md`). See [test-harness](guides/test-harness.md).

### C13. Is there CI?
Not in this repo. `.github` contains only issue templates and a pull request template (`.github/ISSUE_TEMPLATE/`, `.github/pull_request_template.md`). NEEDS INVESTIGATION: check the hosting settings for workflows that are not in the clone (NI-15).

### C14. What is the license?
NEEDS INVESTIGATION. The clone root has no LICENSE file (checked with `ls -a` at HEAD `4db1d7d`). Ask the maintainer before reusing code outside the project (NI-16).

### C15. Why are some paths hard-coded?
Some developer scripts show a development folder on the author's machine as their default, for example the usage line at `tools/playtest/autoplaytest.ps1:4`. That is a known limit (N-13), not a user setting. Pass your own game folder with the script's `-Game` parameter.

### C16. Where are the shaders?
Engine shaders are in the game's `Override_PC.rkv` archive, which holds 17 `.shader` files (docs/START_HERE.md:84). The mod's own shader text is in `src/` (for example `src/ao.inl:95` and `:184`, both GLSL 330). The mod rewrites shader text when the game links it (docs/START_HERE.md:84). See [shader-source-rewriting](how-it-works/05-shader-source-rewriting.md).

### C17. How do I get a shader dump or override?
Set `[debug] DumpShaders=1` to write the shader text to `tyrender\dump`, and `OverrideShaders=1` to load an edited copy from `tyrender\override`. Files are named by an FNV-1a hash (see [debugging](guides/debugging.md) for the hash and the file names).

### C18. What is the session report code?
`src/sesslog.inl` writes the report. It starts at `src/sesslog.inl:1-6`, writes the file at `:259-262`, and writes a CRASH line at `:362-365`. The `[report]` keys are in `src/default_tyrender.ini:90-96`. See [session-report](guides/session-report.md).

### C19. Why does the GL version matter?
The AO shaders are written in GLSL 330 (`src/ao.inl:95`, `:184`). Tessellation needs GL 4.0 and is skipped below it when `[portability] TessVersionGate=1` (`src/tess.inl:72`; `src/default_tyrender.ini:101-102`). The compute fog needs GL 4.3 (README.md:265). The lowest supported GL version is not written down. NEEDS INVESTIGATION: the repo has no `wglCreateContextAttribsARB` call under `src/` (checked by grep), so find where the GL context is created and where the 3.3 floor is set, and check [decisions-log](decisions/decisions-log.md).

### C20. Why can't the playtests run on Linux?
The harness drives the game through Windows APIs; for example `tools/key.ps1` uses Add-Type and user32 calls. The game and the mod are Windows 32-bit builds (docs/START_HERE.md:80). Linux is not a test target yet (inferred from the tools). See [test-harness](guides/test-harness.md).

## See also

- [tester-guide](guides/tester-guide.md)
- [debugging](guides/debugging.md)
- [test-harness](guides/test-harness.md)
- [add-a-setting](guides/add-a-setting.md)
- [known-issues](decisions/known-issues.md)
- [glossary](glossary.md)
