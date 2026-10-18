# What is this

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B, C.

## Purpose

TY HD Renderer is a community graphics mod for *TY the Tasmanian Tiger* on PC. It adds shadows, surface relief, ambient occlusion, haze, bounce light and a final anti-aliasing and grade pass to the game's own renderer. It is a fan project. It ships no game files, so you need your own copy of the game (`README.md:5-7`).

This page says what the mod changes, what it leaves alone, how it is shipped, and what state it is in. For how the mod gets into the game, see [how-it-works/02-loading-and-injection.md](../how-it-works/02-loading-and-injection.md). For the honest list of things it cannot do, see [constraints-and-limits.md](constraints-and-limits.md).

## How it works

The mod is a renderer layer, not a replacement engine. In outline:

1. The launcher (`TYLauncher.exe`) or the installer makes a copy of the game executable, `TY_tyrender.exe`, that imports `tyrender.dll` and is marked Large Address Aware. The original `TY.exe` is opened read-only and never written (`src/patch_exe.inc:7`, `src/patch_exe.inc:38`, `README.md:29`).
2. `tyrender.dll` hooks the game's OpenGL calls at load time (`src/tyrender.cpp:664-668`, the `Install` function).
3. Shader text from the game is rewritten as it is compiled (`src/tyrender.cpp` shader hooks; `src/pbr.inl:1-6`, `src/tess.inl:1-5`).
4. At the point where the game copies its depth buffer, the mod runs its own full-screen passes: AO, bounce light, emitter light and atmosphere (`src/tyrender.cpp:280-293`). Anti-aliasing and grading run later, on the final image (`src/fx.inl:1-6`).
5. Settings come from `tyrender.ini` in the game folder. The launcher writes it through presets and sliders (`README.md:270`, `src/launcher_ui.inc:6`).

## What changes, and what does not

Changes, as the README describes them (`README.md:19-27`):

- **Shadows.** Softer edges, a larger shadow map, and a cooler shade colour (`src/tyrender.cpp:255` and `:274` enlarge the 2048 cascade maps by `MapScale`; `README.md:300`).
- **Surface relief.** Bumps and roughness derived from the texture's own colour (`src/pbr.inl:1-5`, `src/tess.inl:9`).
- **Contact shadows and ambient occlusion.** Crevices and corners darken (`src/ao.inl:1-4`).
- **Haze.** Volumetric fog with light shafts (`src/atmos.inl:1-4`).
- **Bounce light** (High and Ultra; `README.md:25`, `src/gi2.inl:1-5`).
- **Anti-aliasing and grade.** TAA or FXAA, sharpening, contrast, saturation (`src/fx.inl:1-6`).
- **Launcher.** Presets, sliders, a rough cost per option, and saved settings (`README.md:27`).

Not changed (as the README and code describe it):

- The game executable `TY.exe` (`README.md:29`, `src/patch_exe.inc:7`).
- Game assets. The mod has no textures or models of its own. It reads the game's textures and derives relief from their colour (`src/pbr.inl:2-3`).
- Characters are not given surface relief or tessellation (`src/tess.inl:6-7`, `src/tex.inl:202`; see [material-model.md](material-model.md)).
- Gameplay. The mod only changes how the frame is drawn (`docs/GOAL.md:21-22` lists game logic as a non-goal).

## Presets and cost

Four presets are offered, named Low, Medium, High and Ultra (`src/launcher_ui.inc:6`). Medium is the default and the recommended one (`README.md:159`, `src/launcher_ui.inc:218`). The shipped `tyrender.ini` is the Medium preset (`src/default_tyrender.ini:1`).

Frame costs in the README and in `docs/PERFORMANCE.md` are GPU milliseconds per frame, measured by the author on one card (AMD Radeon RX 7600 XT, 2560x1440 windowed):

| Preset | README | PERFORMANCE.md |
|---|---|---|
| Stock game (no mod) | 5.7 ms | 5.7 ms |
| Low | about 6.0 ms | about 6.0 ms |
| Medium | about 12 ms | about 12 ms |
| High | about 18 ms | about 18 ms |
| Ultra | about 31 ms | about 31 ms |

Source: `README.md:155-161`, `docs/PERFORMANCE.md:11-15`. The numbers are not reproduced on other hardware. A conflict in the older comments is recorded in the Discrepancies section below.

## Status

- Community beta (`README.md:9`). The README calls v0.3.5 the latest release. The CHANGELOG lists changes on `main` after v0.3.5 under "Unreleased" (`CHANGELOG.md:5-44`).
- The window title in the launcher still reads `v0.3.5-rc` (`src/TYLauncher.c:22`, `CHANGELOG.md:64`).
- Tested on one graphics card only: AMD Radeon RX 7600 XT on Windows 11 (`README.md:11`, `docs/GOAL.md:18`). NVIDIA, Intel and Linux/Proton are untested (`README.md:11`).
- Several unreleased features are marked "Untested" in the CHANGELOG, meaning the owner has not played them. The rule is in the CHANGELOG preamble (`CHANGELOG.md:3`).
- The mod is a hobby project. The README says it was made with Claude (`README.md:259`).

## Visual evidence

Before and after pairs are in `docs/ab/` (`README.md:33-37`). Each pair is an F11 capture. Frames are named `ty_<build>_<preset>_<shaders-on|shaders-off>_<stamp>.png` (`src/abshot.inl:4`, `src/abshot.inl:93`). The F11 "off" frame skips most of the effects, but it is not the stock game (`README.md:35`).

## Limits

See [constraints-and-limits.md](constraints-and-limits.md). The short version: the mod works only on screen-space data, it is tested on one card, and it cannot add shadow-casting lights or real reflections. None of these are implemented in the repo (no code or doc for them was found by search).

## Known issues

Summarised from `README.md:211-226` and `CHANGELOG.md:34-39`: blue flicker and slow brightness ramps on Ty in water (#5, #104), lily pads and rocks brighter than the water (#4), the portal light does not light the ground (#9), a sharp shadow line near the fire bridge (#103), the F10 overlay not confirmed in a level (#79). Full list: [decisions/known-issues.md](../decisions/known-issues.md).

## How to verify

- Check the status line: the launcher title shows the version and build id (`README.md:196`, `src/TYLauncher.c:23`).
- Check that `TY.exe` is unchanged: its SHA-256 should match the value recorded in `docs/START_HERE.md:6` (the prefix there is `7DF4EA56...21E0`; not checked by any code in the repo).
- Check the presets: the values are in `kPresetVals` in `src/launcher_ui.inc` (names at `:6`).

## NEEDS INVESTIGATION

- Which copy of the game and which Steam build the author used beyond the "2020-04-04" note. See [game-and-engine-facts.md](game-and-engine-facts.md) (N-7).
- Whether the frame costs hold on hardware other than the RX 7600 XT. Only the owner can measure (N-21).

## Discrepancies

- Stock frame time: `README.md:157` and `docs/PERFORMANCE.md:11` say 5.7 ms. `docs/GOAL.md:11` says the stock engine frame is 4.6 ms, and `docs/research/WORLD_SPACE_AO_GI_FOG.md:34` uses a "4.6 ms headroom" figure. `docs/RESUME.md:61` says to read GOAL as 5.7 ms.
- Build identity: `README.md:33` names build `3ee6ea7` for the A/B pairs, while `README.md:9` and `CHANGELOG.md:5` name `4a2ef95`. The repo HEAD is `4db1d7d`.
- Latest version: `README.md:9` says v0.3.5 is the latest release; the launcher title still says `v0.3.5-rc` (`src/TYLauncher.c:22`).
- Medium and High costs: `src/launcher_ui.inc:10` (a code comment) records Medium 11.1 ms, High 17.2 ms, Ultra 32.1 ms on a "merged build". The README and PERFORMANCE.md give about 12, 18 and 31 ms. The comment is older or from a different build; the README numbers are what the docs present.

## See also

- [game-and-engine-facts.md](game-and-engine-facts.md)
- [constraints-and-limits.md](constraints-and-limits.md)
- [material-model.md](material-model.md)
- [../how-it-works/01-architecture-overview.md](../how-it-works/01-architecture-overview.md)
- [../reference/settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md)
- [../glossary.md](../glossary.md)
