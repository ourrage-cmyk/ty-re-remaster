# A/B captures and comparisons

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (testers and modders) for the F11 pair; A (contributors) for the harness A/B.

## Purpose

An A/B capture shows the same frame with an effect on and off. The repository uses three kinds:

1. **The F11 pair, in game.** One key press writes two PNG files, the current frame and the frame with the new look switched off. Players use this for bug reports and comparison images.
2. **Harness A/B, by script.** Developers compare settings in one game session (`tools/suite.ps1`) or one session per preset (`tools/abshots.ps1`). Only pairs from the same session can be compared pixel by pixel.
3. **Comparison helpers.** `tools/imgdiff.ps1`, `tools/cropcmp.ps1`, `tools/imgmetrics.ps1` and `tools/match_ab.ps1` turn two images into a difference map, zoomed crops, numbers or matched cutscene frames.

The comparison images in `docs/ab/` were made with the F11 pair (see below).

## How it works

### The F11 pair (in game)

1. Each frame, the code checks the key state with `GetAsyncKeyState` and fires on the press edge (`src/abshot.inl:114`). It acts only when the game window is the foreground window (`src/abshot.inl:116`).
2. The first capture reads the frame just drawn, before the overlay is drawn and before `SwapBuffers` (`src/tyrender.cpp:607-608`, `src/abshot.inl:86-87`). It is labelled `shaders-on` or `shaders-off`, from the live flag `g_tsLive` (`src/abshot.inl:121`).
3. The code flips two flags: the live flag `g_tsLive` for relief and PBR extras, and `g_abOff`, which tells the passes to skip (`src/abshot.inl:120-123`). It pushes the shader values again so that a program already bound keeps the right values (`src/abshot.inl:102-108`).
4. The next frame is drawn in the other state. The second capture happens at the next call and gets the other label (`src/abshot.inl:127-128`). The flags are then restored (`src/abshot.inl:129-130`).
5. Each PNG is written by a small encoder inside the DLL, using stored deflate blocks, so there is no compression step (`src/abshot.inl:48-84`). The author notes the files are large (`src/abshot.inl:48`).
6. File name: `ty_<build>_<preset>_<shaders-on|shaders-off>_<yyyymmdd-hhmmss>.png`, in `<game folder>\Screenshots\` (`src/abshot.inl:4`, `:93`). The build comes from `build_id.h` (`src/abshot.inl:7`). The preset comes from `[shots] Preset` and defaults to `custom` (`src/abshot.inl:89-91`).
7. A title-bar message confirms the save for about 2.5 s (`src/abshot.inl:97`).

What the off frame turns off. These passes check `g_abOff` (each line is a gate):

- Ambient occlusion: `src/ao.inl:567`.
- Atmosphere (fog, haze, shafts): `src/atmos.inl:362`.
- Emitter light: `src/emit.inl:139`, `:227`.
- Post chain, including TAA jitter and grade: `src/fx.inl:162`, `:178`.
- Bounce light (Hi-Z and legacy): `src/gi.inl:105`, `src/gi2.inl:39`.
- Natural-light features are read as off: `src/light.inl:109`.
- Distance LOD in PBR: `src/pbr.inl:185`.
- Shadow strength uses the stock literal: `src/tyrender.cpp:558`.
- Relief and PBR extras flip with `g_tsLive`: `src/abshot.inl:122`, `src/tess.inl:20`.

The README says the off frame is "not proven to be fully vanilla" because the shadow filter and the tessellated draw path still run (`README.md:35`).

### Harness A/B

- **`tools/suite.ps1`**: one game session. It boots with the first variant's ini and swaps the other variants in with a reload trigger (`tools/suite.ps1:10-11`, `tools/agent_test.ps1:47-50`). For each variant and each standard view it takes a snapshot (`tools/agent_test.ps1:62`). Output: a contact sheet with views as rows and variants as columns, a Ty strip, a manifest with rock metrics, and a line in the results ledger (`tools/suite.ps1:18-30`). Only live keys can differ between variants (`tools/suite.ps1:5`).
- **`tools/abshots.ps1`**: one fresh session per preset, with the same views. Restart-only keys therefore differ correctly (`tools/abshots.ps1:2-3`, `:11-16`). The "vanilla" variant writes an ini that turns effect switches off (`tools/abshots.ps1:7`). The script sets `TY_SLOT` (`tools/abshots.ps1:10`). Its default DLL path is under the workstation root (`tools/abshots.ps1:5`; see [test-harness.md](test-harness.md) Limits).
- **`tools/ab2_run.ps1`**: a wrapper that sets short settle and camera waits and runs `abshots.ps1` on slot 2 with 44 views (`tools/ab2_run.ps1:1-3`).
- **`tools/match_ab.ps1`**: for cutscene frames, picks reference frames from the vanilla pool and finds the most similar frame in each other preset. Similarity is edge correlation on a subtitle band, plus 0.35 times the whole-frame value (`tools/match_ab.ps1:7`, `:17-18`). Frame timing shifts between presets, so the script matches content, not time.
- **Comparison helpers.** `tools/imgdiff.ps1` writes an amplified difference map, red where B is darker and cyan where B is brighter, and prints mean values and the share of darker pixels (`tools/imgdiff.ps1:2-4`, `:25`, `:31`). `tools/cropcmp.ps1` puts zoomed crops of one region side by side (`tools/cropcmp.ps1:2`). `tools/imgmetrics.ps1` offers `Mad`, `GradCorr`, `MeanLuma` and `MeanSat` (`tools/imgmetrics.ps1:3-6`).

### The `docs/ab/` image set

The folder holds 32 JPEG files: eight scenes, each as a full frame and a 2x crop, each with an on and an off frame. Names follow `<fog|nofog>_<n>_<scene>[_crop]_<on|off>.jpg`, for example `fog_1_canyon_gate_crop_off.jpg`. Scene slugs: `canyon_gate`, `lily_pond`, `waterfall_reeds`, `koala_lvl2`, `house_gate`, `house_waterfall`, `station_sign`, `shade_ao`. The README embeds them with captions (`README.md:31-149`).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Default key | `src/abshot.inl:8`, `:22` | `VK_F11` (`0x7A`); `[shots] Key` overrides it. |
| Key parse | `src/abshot.inl:16-19` | `F1`..`F24` or a virtual-key number. `0` gives 0, which is off. |
| Edge detect and foreground gate | `src/abshot.inl:114-116` | Fires once per press. Ignored when the game is not foreground. |
| Read the frame | `src/abshot.inl:25-41` | Reads the default framebuffer with `glReadPixels`, restoring the game's bindings after. |
| Pair logic | `src/abshot.inl:110-132` | Phase 0 captures and flips; phase 1 captures the second frame and restores. |
| Preset in file name | `src/abshot.inl:88-91` | Default `custom`; non-alphanumeric characters become `-`. |
| Log line | `src/abshot.inl:94` | `[shot] <path> <w>x<h> frame=<n>`. |
| Off-frame gates | see the list above | `g_abOff` declared at `src/tyrender.cpp:78`. |
| Harness pair | `tools/playtest/common.ps1:237-256` | Presses the key (F11 by default), waits up to 8 s, copies the two new PNGs as `<prefix>_a.png` and `_b.png`. |
| Foreground before a pair | `tools/playtest/common.ps1:240` | Counts a miss and marks the shot invalid. |

## Settings

| Key | Default in code | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[shots] Key` | `F11` (`src/abshot.inl:22`) | `F11` (`src/default_tyrender.ini:99`) | Live: `LoadShotConfig` runs on reload (`src/tyrender.cpp:598`) | Capture key. `0` turns it off. |
| `[shots] Preset` | `custom` (`src/abshot.inl:89`) | Written by the launcher (`src/default_tyrender.ini:98`) | Live | Names the files only. |
| `[tess] ABToggle` | `0` (`src/tess.inl:69`) | Not in shipped ini | Live | Harness aid. Flips relief and PBR extras when `tyrender\cam.txt` changes (`src/tess.inl:20`). |

The full settings are in [reference/settings-system-debug.md](../reference/settings-system-debug.md). The F11 key is listed with the other hotkeys in [reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md).

## Cost

Not measured.

Each press reads the back buffer twice, and writes two uncompressed PNG files of full window size (`src/abshot.inl:25-41`, `:48-84`). The cost depends on disk and window size. No timing has been recorded in the repo.

## Limits

- **Foreground only.** The key does nothing when the game window is not in front (`src/abshot.inl:116`).
- **The off frame is not vanilla.** Shadow filter and tessellated geometry still run (`README.md:35`).
- **Two frames, one apart.** Moving objects and particles differ between the two halves of a pair (inferred from the one-frame flip, `src/abshot.inl:122-128`).
- **Pixels compare only within a session.** Images from different sessions are not pixel-comparable (`docs/START_HERE.md:76`). Run an A/A check first to measure noise (`docs/START_HERE.md:95`).
- **Harness restrictions.** `suite.ps1` can change only live keys (`tools/suite.ps1:5`). Restart-only keys need one session per preset, which is what `abshots.ps1` does.
- **Edge-correlation threshold.** The value 0.55 for "suspect" is the author's calibration (`tools/imgmetrics.ps1:4`). This page did not reproduce it.

## Known issues

- **#14** added the F11 A/B hotkey (`CHANGELOG.md:58`, `src/abshot.inl:1`).
- **#96** changed the off frame to skip more passes. Marked untested (`CHANGELOG.md:30-32`).
- **Pair build id.** The README caption says the pairs come from build `3ee6ea7` (`README.md:33`), but the changelog says "three build 4a2ef95 pairs" (`CHANGELOG.md:28`). See Discrepancies.

## How to verify

Steps for a tester (no harness):

1. Set `[shots] Key=F11` (the default) in `tyrender.ini`, or check that the key line is there (`src/default_tyrender.ini:99`).
2. Start a level, stand still, and press F11 once with the game window in front.
3. In `<game folder>\Screenshots\`, two files appear. Their names contain `shaders-on` and `shaders-off`, with the same timestamp (`src/abshot.inl:93`).
4. In `tyrender.log`, two `[shot]` lines appear (`src/abshot.inl:94`). The log is recreated at each game start (`src/tyrender.cpp:708`), so copy it before starting the game again.
5. For a visual comparison, run `tools/imgdiff.ps1 -A <off.png> -B <on.png> -Out <diff.png>` (`tools/imgdiff.ps1:4`). Not run by the documenter.

Steps for the off-frame check (spec, not executed):

1. The spec in `tools/playtest/requests/f11_true_off.md` compares the off frame with a stock frame at a fixed camera. Its pass rule is a mean difference of at most 2.0 out of 255, outside the Ty box (`tools/playtest/requests/f11_true_off.md:15-19`).
2. The spec lists the passes still on as residuals and says shadows and light are not gated. The code disagrees; see Discrepancies.

## NEEDS INVESTIGATION

- **P12-6** Whether the current off frame meets the 2.0 limit in the spec. Method: run the spec on the owner's stock and current builds, in one TEST phase run (`tools/playtest/requests/f11_true_off.md:4`).
- **P12-7** Which is current for shadows and light in the off frame. The spec says "not gated" (`f11_true_off.md:21-23`), the code gates them (`src/tyrender.cpp:558`, `src/light.inl:109`). Method: `git log --format=%h -- tools/playtest/requests/f11_true_off.md` in the clone for its date, then run the spec.
- **P12-8** Build of the `docs/ab/` set (`3ee6ea7` or `4a2ef95`). Method: `git log --format='%h %s' -- docs/ab README.md` in the clone, and the owner's note.
- **P12-9** Whether `abshots.ps1` can run on the owner's data. Its preset ini files are read from a results folder that is not in the repo (`tools/abshots.ps1:6`, `:9`). Method: owner's results folder (see N-13).

## Discrepancies

1. `README.md:33` says the pairs come from build `3ee6ea7`, on the Ultra preset. `CHANGELOG.md:28` says "three build 4a2ef95 pairs". Both are in the repo.
2. `tools/playtest/requests/f11_true_off.md:21-23` says shadow strength and light uniforms are not gated by the hotkey. The code gates both (`src/tyrender.cpp:558`, `src/light.inl:109`). `README.md:35` and `CHANGELOG.md:32` agree with the code. The spec is out of date.
3. `tools/abshots.ps1:5` and `tools/abshots.ps1:6` use paths under the workstation root (N-13). Not a conflict; listed for the reviewer.

## See also

- [test-harness.md](test-harness.md): the playtest runner that reads these pairs.
- [tester-guide.md](tester-guide.md): how testers send a pair with a report.
- [debugging.md](debugging.md): debug views that change the picture.
- [../how-it-works/09-overlay-input-and-hotkeys.md](../how-it-works/09-overlay-input-and-hotkeys.md): input and the order of overlay and capture.
- [../reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md): every hotkey and trigger file.
