# Settings: AO, GI and distance LOD

> Status: documented at commit 4db1d7d (`main`, Merge PR #122); last verified 2026-10-18. Audience: B (tuning and bug reports), C (review).

## Purpose

This page lists every key in `[ao]`, `[gi]` and `[lod]` in `tyrender.ini`. These keys switch and tune screen-space ambient occlusion (AO), screen-space bounce light (GI, the `gi.inl` and `gi2.inl` passes) and the distance LOD switches. The AO loader is `LoadAOConfig` (`src/ao.inl:487`); it also calls `LoadLodConfig` (`src/ao.inl:478`, called at `src/ao.inl:490`). The GI loader is `LoadGIConfig` (`src/gi.inl:89`). The file is `<game folder>\tyrender.ini`; the loaders build it from the game folder (`src/ao.inl:488`).

For what the passes do, read [AO](../shaders/ao-gtao.md), [GI legacy](../shaders/gi-legacy.md), [GI Hi-Z](../shaders/gi-hiz.md) and [distance LOD](../shaders/distance-lod.md).

## How to read the tables

- **Key (type, range).** The section and key as the loader reads them. Keys are case-sensitive.
- **Code default (file:line).** The second argument of the `GetPrivateProfile*` call, which is used when the key is missing from `tyrender.ini`. The file:line is the first read in the code that is not a log line.
- **Shipped.** The value in `src/default_tyrender.ini` (`ini:N` is that file's line). "not in file" means the code default is used.
- **Launcher and presets.** What `src/launcher_ui.inc` writes or reads for the key. Preset values are in the order Low / Medium / High / Ultra.
- **Live or restart.** Definitions are in [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md). "Live" means the reload path re-reads the key and the value is used per frame or per draw. "(inferred)" means the use site was not traced line by line.
- **Effect.** One line. "(inferred from name)" means the text comes from the key name only. Mechanisms are on the shader pages linked under See also.

Every key in the code appears in exactly one table on the four settings pages (227 keys in all, checked against a grep of `GetPrivateProfile*` and the `F`/`LF`/`PF` macros in `src/*.inl` and `src/tyrender.cpp`).

## [ao]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (ao.inl:492) | `1` (ini:3) | Presets: 0 / 1 / 1 / 1 (AO switch) | Live | Master switch for AO. Checked every frame (ao.inl:567). |
| `Legacy` (int 0/1) | `0` (ao.inl:506) | `0` (ini:4) | Launcher always writes 0 | Live | 1 = legacy GTAO path. With 0 the v2 path runs; if v2 cannot compile AO is turned off, there is no legacy fallback (ao.inl:546, 609). Not recommended (README). |
| `Slices` (int) | `3` (ao.inl:502) | `3` (ini:5) | Presets: 2 / 3 / 3 / 4 | Live (inferred) | Directions per pixel in the horizon search (u_slices, ao.inl:655). |
| `Steps` (int) | `6` (ao.inl:502) | `6` (ini:6) | Presets: 4 / 6 / 6 / 8 | Live (inferred) | Samples per direction (u_steps, ao.inl:655). Cost scales with Slices x Steps (src/launcher_ui.inc:47). |
| `Radius` (float, world units) | `60` (ao.inl:495) | not in file | - | Live (inferred) | Search radius of the crevice term (u_radius, ao.inl:655). ao.inl:6 header says 1.5; code and README say 60 (conflict). |
| `Strength` (float) | `0.85` (ao.inl:500) | not in file | Slider 'Crevice shadows' 0 to 1.5, default 0.60 (src/launcher_ui.inc:21) | Live | Strength of the composite (u_strength, ao.inl:703). Launcher slider default differs from code. |
| `Power` (float) | `1.0` (ao.inl:501) | not in file | - | Live | Exponent applied to the AO term in the composite (u_power, ao.inl:703). |
| `Thin` (float) | `0.5` (ao.inl:496) | not in file | - | Live (inferred) | Thin-object term, for grass and fence-like geometry (inferred from name; README 'thin objects'). |
| `MultiBounce` (float) | `0.6` (ao.inl:498) | not in file | - | Live (inferred) | Brightens AO in bright areas to imitate bounce (inferred from name; README 'bounce darkening'). Consumer: ao.inl:703 (u_mbScale). |
| `Temporal` (int 0/1) | `1` (ao.inl:499) | not in file | - | Live (inferred) | 1 = accumulate AO over frames (ao.inl:650 uses the frame index with NoiseFrames). |
| `TemporalBlend` (float) | `0.92` (ao.inl:497) | not in file | - | Live (inferred) | History weight of the temporal accumulation (inferred from name). README says 0.85; code says 0.92 (conflict). |
| `TemporalMode` (int 0/1) | `1` (ao.inl:513) | not in file | - | Live (inferred) | 0 = v0.3.1 nearest-history accumulation, 1 = bilinear history with length and variance clamp (ao.inl:513 comment). |
| `NoiseFrames` (int (1 or more used)) | `64` (ao.inl:512) | not in file | - | Live (inferred) | Length of the noise cycle in frames for the temporal sample pattern (ao.inl:650, clamped to at least 1). |
| `BlurRadius` (int 1 to 5 (clamped)) | `2` (ao.inl:512) | not in file | - | Live (inferred) | Radius in texels of the AO denoise blur (ao.inl:512). |
| `BlurFalloff` (float) | `0.15` (ao.inl:512) | not in file | - | Live (inferred) | Edge-stop strength of the AO denoise blur (inferred from name). |
| `WideRadius` (float, world units) | `400` (ao.inl:493) | not in file | - | Live (inferred) | Search radius of the wide, corner-darkening term (inferred from name). |
| `WideStrength` (float) | `1.0` (ao.inl:494) | not in file | Slider 'Corner darkness' 0 to 2, default 1.00 (src/launcher_ui.inc:22) | Live | Strength of the wide term (inferred from name). |
| `WideSlices` (int (0 = same as Slices)) | `0` (ao.inl:511) | not in file | - | Live (inferred) | Directions for the wide term; 0 uses Slices (ao.inl:511). |
| `FadeStart` (float, view distance) | `0` (ao.inl:507) | not in file | - | Live (inferred) | View distance where AO starts to fade out (ao.inl:470 comment). With FadeEnd 0 the fade is off. |
| `FadeEnd` (float, view distance (0 = off)) | `0` (ao.inl:508) | not in file | - | Live (inferred) | View distance where AO has faded out (ao.inl:470 comment). |
| `CharRadius` (float (0 = off)) | `40` (ao.inl:509) | not in file | - | Live (inferred) | Radius of the character capsule used as an AO occluder mask (ao.inl:471 comment). |
| `CharHeight` (float) | `120` (ao.inl:510) | not in file | - | Live (inferred) | Height of that character capsule (ao.inl:471 comment). |
| `FogFade` (float) | `1.0` (ao.inl:514) | not in file | - | Live | 0 = AO ignores fog (old behaviour), 1 = AO fades with fog transmittance (ao.inl:514 comment). |
| `FogFadePower` (float (min 0.1)) | `2.0` (ao.inl:514) | not in file | - | Live | Exponent for the fog fade of AO (ao.inl:514). |
| `ZMode` (int 0/1) | `0` (ao.inl:503) | not in file | - | Live (inferred) | Depth convention: 0 = GL depth range (ndc z = 2d - 1), 1 = ndc z = d (ao.inl:6 header). |
| `Exp` (int bit mask) | `0` (ao.inl:504) | not in file | - | Live | Experiment bits, not in README: 1 skips the colour re-copy after AO (tyrender.cpp:294), 2 skips the AO composite draw (ao.inl:708), 8 skips the AO draw call (ao.inl:565). |
| `DepthMipOffset` (float) | `3.15` (ao.inl:505) | not in file | - | Live (inferred) | Mip level bias of the depth pyramid used by the horizon search (u_mipOff, ao.inl:650; inferred from name). |
| `DebugView` (int 0 to 3) | `0` (ao.inl:515) | not in file | - | Live | 1 = AO only, 2 = normals, 3 = view depth (ao.inl:5, 663, 691). The flag file ao_debug wins when present (ao.inl:521). Test aid. |

## [lod]

The master switch `Enable` and one switch per effect. An effect is reduced with distance only when `Enable` and its own key are both 1 (`src/ao.inl:473-475` comment). `Start` and `End` are view depth in world units; `MinScale` is the share of the work kept at `End`, ramped smoothly.

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `0` (ao.inl:480) | `0` (ini:21) | - | Live | Master switch for distance LOD. Each effect needs its own key as well (ini:15-19; ao.inl:473-475 comment). |
| `AO` (int 0/1) | `0` (ao.inl:481) | `0` (ini:23) | - | Live | 1 = AO horizon steps fall with distance when Enable is 1 (ao.inl:473-475 comment; ini:23). |
| `GI` (int 0/1) | `0` (ao.inl:481) | `0` (ini:22) | - | Live | 1 = GI rays fall with distance when Enable is 1 (ao.inl:473-475 comment; used in gi2.inl trace path, inferred). |
| `Fog` (int 0/1) | `0` (ao.inl:481) | `0` (ini:24) | - | Live | 1 = froxel fog samples fall with distance when Enable is 1 (ao.inl:473-475 comment; inferred consumer). |
| `Shadow` (int 0/1) | `0` (tyrender.cpp:158) | `0` (ini:25) | - | Live (inferred) | 1 = shadow cascades 2 and 3 use the cheaper filter when Enable is 1 and [shadows] Patch is 1 (tyrender.cpp:158; ini:18). Consumer not traced. |
| `Start` (float, view depth in world units) | `1500` (ao.inl:482) | `1500` (ini:26) | - | Live | Full quality closer than this (ao.inl:482; ini:26). |
| `End` (float, world units (raised to Start + 1 if smaller)) | `6000` (ao.inl:483) | `6000` (ini:27) | - | Live | Distance where the kept share reaches MinScale (ao.inl:483). |
| `MinScale` (float 0 to 1 (clamped)) | `0.35` (ao.inl:484) | `0.35` (ini:28) | - | Live | Share of the work kept at End, ramped smoothly (ao.inl:484). |
| `TessEnable` (int 0/1) | `0` (tess.inl:61) | `0` (ini:30) | - | Restart (startup only) (inferred) | 1 = tessellation edge levels fall with distance (tess.inl:61; ini:29-30). The same ini file says tessellation/relief is not wired (ini:19): conflict. Consumer in pbr.inl:185. |

## [gi]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (gi.inl:91) | `0` (ini:8) | Presets: 0 / 0 / 1 / 1 (bounce light switch) | Live | Master switch for bounce light (gi.inl:105, per frame). |
| `Mode` (int) | `1` (gi.inl:95) | `1` (ini:9) | Launcher always writes 1 | Live (inferred) | 1 = Hi-Z GI (gi2.inl), 0 = legacy linear-march GI (gi.inl). README: 1 is recommended. |
| `Rays` (int (min 1 used)) | `4` (gi.inl:91) | `1` (ini:10) | Presets: 1 / 1 / 1 / 2 (launcher reads 1 when key missing) | Live | Rays per pixel for the Hi-Z trace (u_rays, gi2.inl:453). Code default is 4; shipped and presets use 1 (conflict with code default). |
| `Steps` (int) | `8` (gi.inl:91) | not in file | - | Live (inferred) | Step budget: when HiZIter is 0 the trace iterates Steps x 4 times (gi2.inl:453). |
| `Strength` (float) | `0.7` (gi.inl:93) | not in file | Slider 'Bounce light' 0 to 3, default 0.70 (src/launcher_ui.inc:25) | Live | Strength of the GI composite (u_strength, gi2.inl:504); GI is skipped at 0 (gi.inl:105). |
| `Radius` (float, world units) | `400` (gi.inl:93) | not in file | - | Live (inferred) | Search radius of the GI rays (inferred from name). |
| `Thickness` (float, world units) | `120` (gi.inl:93) | not in file | - | Live (inferred) | Thickness band a ray hit may land in (inferred from name). |
| `TemporalBlend` (float) | `0.85` (gi.inl:93) | not in file | - | Live (inferred) | History weight of the GI temporal pass (inferred from name). README agrees with code (0.85). |
| `SigmaL` (float) | `4` (gi.inl:93) | not in file | - | Live (inferred) | Luminance edge stop of the a-trous denoise (inferred from name). |
| `SourceBlur` (float) | `0.15` (gi.inl:93) | not in file | - | Live (inferred) | Blur of the colour that GI takes from its sources (inferred from name). |
| `NoiseCycle` (int) | `8` (gi.inl:93) | not in file | - | Live (inferred) | Length in frames of the GI noise pattern (inferred from name). |
| `ExcludeSources` (int bit mask) | `7` (gi.inl:93) | not in file | - | Live (inferred) | Bit mask of draw classes left out of the GI source mask (gi.inl:93; default 7). Bit meanings are in the GI pages (Ch05). |
| `Characters` (int 0/1) | `1` (gi.inl:94) | not in file | - | Live (inferred) | 1 = characters take part in GI (README; inferred from name). |
| `CharacterColour` (float) | `0.0` (gi.inl:94) | not in file | - | Live (inferred) | How much of a character's colour is passed to GI (README). |
| `CharacterGI` (float) | `0.3` (gi.inl:94) | not in file | - | Live (inferred) | How much light a character receives from the scene (README). |
| `CharacterHeight` (float) | `160` (gi.inl:94) | not in file | - | Live (inferred) | Height of the mask around a character (README). |
| `CharacterRadius` (float) | `70` (gi.inl:94) | not in file | - | Live (inferred) | Radius of the mask around a character (README). |
| `Checker` (int 0/1) | `0` (gi.inl:95) | `0` (ini:12) | - | Live (inferred) | 1 = trace a checkerboard half of the pixels each frame; the rest keep last frame and temporal blend fills them (ini:11-12). |
| `AtrousFar` (int 0/1) | `0` (gi.inl:95) | `0` (ini:14) | - | Live (inferred) | 1 = 3x3 denoise ring beyond [lod] Start. The ini comment says untested (ini:13-14). |
| `Atrous` (int) | `3` (gi.inl:95) | not in file | - | Live (inferred) | Number of a-trous denoise iterations (g_giAtrous, gi2.inl:11). |
| `HiZIter` (int (0 = Steps x 4)) | `0` (gi.inl:95) | not in file | - | Live (inferred) | Fixed iteration count of the Hi-Z trace (gi2.inl:453). |
| `Prof` (int 0/1) | `0` (gi.inl:95) | not in file | - | Live (inferred) | 1 = GPU timer marks for GI stages (gi2.inl:71, 95). |
| `Sweep` (int 0/1) | `0` (gi.inl:95) | not in file | - | Live (inferred) | 1 = cycle a built-in table of GI variants for measurement; the table overwrites Mode, Rays, Atrous and HiZIter (gi2.inl:14, 27; gi.inl:107). Test harness only. |
| `DebugView` (int 0 to 2) | `0` (gi.inl:95) | not in file | - | Live | 1 = GI only, 2 = character mask (gi2.inl:336, 341). The flag file gi_debug wins when present (gi.inl:101). |

## Cost

Measured values are GPU milliseconds per frame at 2560x1440 on an AMD Radeon RX 7600 XT (the only GPU tested so far). They come from the merged v0.3 build, not from v0.3.2 or later, so treat them as rough.

- AO off saves about 1.15 ms ([docs/results/cost_table.md](../../results/cost_table.md) line 16, relative to the C1 reference; [docs/PERFORMANCE.md](../../PERFORMANCE.md) line 50, relative to Medium).
- Bounce light at 1 ray adds about 4.4 ms (`gi_hiz1`, cost_table.md line 14; PERFORMANCE.md line 55). The Ultra preset uses 2 rays.
- The AO cost scales with `Slices` x `Steps`: the launcher's fitted model uses 0.051 ms per slice-step product on top of 0.5 ms (`src/launcher_ui.inc:48`).
- `[lod]` is **not measured** (PERFORMANCE.md line 81: "not yet measured (untested on hardware)").

## Limits

- The keys change the parameters of the passes. They cannot add data the passes do not have: the passes are screen-space and see only what the depth and colour copies show (see [constraints and limits](../concepts/constraints-and-limits.md)).
- `Legacy=1` is the older AO path and is not recommended (README "For modders"). With `Legacy=0` there is no fallback to the legacy path: if the v2 AO program cannot compile, AO is turned off (`src/ao.inl:546`).

## Discrepancies

1. **[ao] TemporalBlend.** README table says 0.85. Code default is 0.92 (`src/ao.inl:497`). The first-pass SETTINGS_REFERENCE.md has the same conflict. The code value is used here.
2. **[ao] Radius and Power in the header.** The header comment of `src/ao.inl` (line 6) says `Radius=1.5` and `Power=1.5`. Code defaults are `Radius` 60 (`src/ao.inl:495`) and `Power` 1.0 (`src/ao.inl:501`), and the README agrees with the code. The header is stale (plan item N-26).
3. **[gi] Enable.** Code default 1 (`src/gi.inl:91`). Shipped file 0 (`ini:8`). If the key is missing, the launcher reads 0 (`src/launcher_ui.inc:77`), so the launcher and the DLL disagree for a missing key. The README row is correct for the code default.
4. **[gi] Rays.** Code default 4 (`src/gi.inl:91`). Shipped file 1 (`ini:10`). Launcher reads 1 when the key is missing (`src/launcher_ui.inc:77`). The README row (4) matches the code.
5. **[lod] tessellation.** `ini:19` says tessellation and relief distance LOD "is not wired". `ini:29-30` and `src/tess.inl:61` define `TessEnable`, which sets the edge levels. The comment and the key disagree.
6. **First-pass notes.** SETTINGS_REFERENCE.md lists `[lod] Shadow` and `[ao] Legacy` as "not verified". They are now verified at `src/tyrender.cpp:158` and `src/ao.inl:506`. Its `[ao] Legacy` code default is 0.
7. **Undocumented key.** `[ao] Exp` is a bit mask used for experiments (`src/tyrender.cpp:294`, `src/ao.inl:565`, `src/ao.inl:708`). It is not in the README or the first pass.

## NEEDS INVESTIGATION

- **[lod] Shadow and TessEnable consumers.** The loader reads them (`src/tyrender.cpp:158`, `src/tess.inl:61`) but the consuming code was not traced for this page. Method: `grep -n "g_shLod\|g_tsLodOn" src/*.inl src/*.cpp`, then run the game with `[lod] Enable=1` and the key set, and compare a far view (plan N-22).
- **[lod] cost.** Not measured. Method: GPU timer run with `tools/perf_matrix.ps1` or `tools/g1_gate.ps1` (plan N-20).
- **[gi] ExcludeSources bits.** The default is 7 (`src/gi.inl:93`). Bit meanings are on the GI shader pages (Ch05). Method: read the mask code in `src/gi2.inl`.
- **[ao] TemporalBlend and Radius intended values.** Owner decision: header comment versus code (plan N-26).

## See also

- [settings-atmos-fog-emit.md](settings-atmos-fog-emit.md), [settings-post-shadows-light-pbr-tess.md](settings-post-shadows-light-pbr-tess.md), [settings-system-debug.md](settings-system-debug.md)
- [presets.md](presets.md): what each preset writes into these keys
- [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md): when each key is read
- [AO](../shaders/ao-gtao.md), [GI Hi-Z](../shaders/gi-hiz.md), [distance LOD](../shaders/distance-lod.md)
