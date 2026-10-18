# Settings: atmosphere, fog, emitter light

> Status: documented at commit 4db1d7d (`main`, Merge PR #122); last verified 2026-10-18. Audience: B (tuning and bug reports), C (review).

## Purpose

This page lists the keys in `[atmos]`, `[fog]`, `[emit]`, `[spill]` and `[decal]`. The `[atmos]` section tunes the volumetric haze and fog pass (`LoadAtmosConfig`, `src/atmos.inl:290`). The `[fog]` section holds multipliers on the game's own fog and the water and foliage rules. Three of its keys are read in `src/tyrender.cpp` (`Distance`, `Amount`, `LateBoost`, lines 601-602 and 722-723) and `WaterNames` is read in `src/tex.inl:77`. `[emit]` tunes emitter light (`LoadEmitConfig`, `src/emit.inl:32`). `[spill]` and `[decal]` are small texture-side switches read in `src/tex.inl`.

For the passes, read [atmosphere fog](../shaders/atmosphere-fog.md), [froxel fog](../shaders/atmosphere-froxel-compute.md), [sky and haze colour](../shaders/sky-estimate-and-haze-color.md), [emitter light](../shaders/emitter-light.md) and [water, fog and decals](../shaders/water-fog-and-decals.md).

## How to read the tables

- **Key (type, range).** The section and key as the loader reads them. Keys are case-sensitive.
- **Code default (file:line).** The second argument of the `GetPrivateProfile*` call, which is used when the key is missing from `tyrender.ini`. The file:line is the first read in the code that is not a log line.
- **Shipped.** The value in `src/default_tyrender.ini` (`ini:N` is that file's line). "not in file" means the code default is used.
- **Launcher and presets.** What `src/launcher_ui.inc` writes or reads for the key. Preset values are in the order Low / Medium / High / Ultra.
- **Live or restart.** Definitions are in [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md). "Live" means the reload path re-reads the key and the value is used per frame or per draw. "(inferred)" means the use site was not traced line by line.
- **Effect.** One line. "(inferred from name)" means the text comes from the key name only. Mechanisms are on the shader pages linked under See also.

Every key in the code appears in exactly one table on the four settings pages (227 keys in all, checked against a grep of `GetPrivateProfile*` and the `F`/`LF`/`PF` macros in `src/*.inl` and `src/tyrender.cpp`).

## [atmos]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (atmos.inl:292) | `1` (ini:32) | Presets: 0 / 1 / 1 / 1 | Live | Master switch for the volumetric fog pass (atmos.inl:362, per frame). |
| `HalfRes` (int 0/1) | `1` (atmos.inl:292) | not in file | - | Live | 1 = fog pass runs at half resolution and is upsampled (atmos.inl:481). |
| `Steps` (int) | `24` (atmos.inl:292) | `24` (ini:34) | Presets: 12 / 24 / 24 / 32 | Live | Ray-march samples per pixel (u_steps, atmos.inl:19 declares it). |
| `Froxel` (int 0/1) | `1` (atmos.inl:304) | `1` (ini:33) | Launcher always writes 1 | Live | 1 = compute froxel volume path (kAtmosCS), 0 = per-pixel ray march (atmos.inl:453; forced failure at atmos.inl:257). |
| `FroxelPixels` (int, clamped 4 to 64) | `12` (atmos.inl:304) | not in file | - | Live (inferred) | Size in pixels of one froxel cell (atmos.inl:304; inferred from name). |
| `ContactStable` (int 0/1) | `1` (atmos.inl:298) | not in file | - | Live (inferred) | 1 = stable contact shadows (inferred from name; atmos.inl:298). |
| `ContactShadows` (float) | `0.35` (atmos.inl:296) | `0` (ini:35) | Presets: 0 / 0 / 0.35 / 0.35; slider 'Contact shadows' 0 to 1, default 0.00 (src/launcher_ui.inc:30) | Live | Strength of sharp contact shadows toward the sun, 0 = off (u_contact, atmos.inl:447). Costs GPU time (PERFORMANCE.md:76). |
| `ContactLength` (float) | `25` (atmos.inl:296) | not in file | - | Live (inferred) | Length of the contact-shadow march (u_contactLen, atmos.inl:447; inferred from name). |
| `ContactThickness` (float) | `6` (atmos.inl:296) | not in file | - | Live (inferred) | Thickness of the contact-shadow march (u_contactThick, atmos.inl:447; inferred from name). |
| `Density` (float) | `0.000015` (atmos.inl:294) | not in file | - | Live | Fog density. Enters u_density as Density x Intensity x FogAmount x [fog] Amount / [fog] Distance (atmos.inl:446). |
| `HeightFalloff` (float) | `0.0015` (atmos.inl:294) | not in file | - | Live | How fast fog thins with height (u_falloff; also read by gpuprof.inl:49; inferred from name). |
| `MaxDist` (float, world units) | `40000` (atmos.inl:294) | not in file | - | Live (inferred) | Distance at which the fog march stops or saturates (inferred from name; u_maxDist). |
| `SunIntensity` (float) | `1.4` (atmos.inl:295) | not in file | Slider 'Light shafts' 0 to 8, default 2.00 (src/launcher_ui.inc:29) | Live (inferred) | Brightness of the sun term in the scattering (inferred from name). Code default is 1.4. |
| `Anisotropy` (float) | `0.6` (atmos.inl:295) | not in file | - | Live (inferred) | Henyey-Greenstein g of the phase function (u_g, atmos.inl:19; inferred from name). |
| `AmbientScale` (float) | `1.0` (atmos.inl:295) | not in file | - | Live (inferred) | Scale of ambient light inside the fog (u_ambScale, atmos.inl:449). |
| `Intensity` (float) | `2.5` (atmos.inl:297) | `1.2` (ini:36) | Presets: 2.5 / 1.2 / 1.2 / 1.0 (src/launcher_ui.inc:7); slider 'Distance haze' 0 to 4, default 1.20 (src/launcher_ui.inc:28) | Live | Overall haze amount; it multiplies fog density (atmos.inl:446). Code default is 2.5, shipped file says 1.2 (conflict). |
| `FogAmount` (float) | `0.8` (atmos.inl:296) | not in file | - | Live | Fog multiplier inside the atmos section. It is a different key from [fog] Amount and both multiply the density (atmos.inl:446). |
| `Desaturation` (float) | `0.7` (atmos.inl:297) | `0.3` (ini:37) | Presets: 0.8 (Low) / 0.3 (others) (src/launcher_ui.inc:493) | Live (inferred) | How much far colour moves toward haze colour (inferred from name; README: far rocks lose colour). |
| `HazeLift` (float) | `0.35` (atmos.inl:297) | `0.15` (ini:38) | Presets: 0.35 (Low) / 0.15 (others) (src/launcher_ui.inc:493) | Live (inferred) | How much far haze is lifted toward light (README: get lighter). |
| `SkyHaze` (float) | `0.8` (atmos.inl:297) | not in file | - | Live (inferred) | Haze strength on the sky and horizon (inferred from name). README says 0.5; code says 0.8 (conflict). |
| `SkyHazeHeight` (float) | `0.45` (atmos.inl:297) | not in file | - | Live (inferred) | Height band of the sky haze (inferred from name). README says 0.25; code says 0.45 (conflict). |
| `FogSaturation` (float) | `0.5` (atmos.inl:297) | not in file | - | Live (inferred) | Saturation of the fog colour (inferred from name). |
| `ShaftContrast` (float) | `0.5` (atmos.inl:297) | not in file | - | Live (inferred) | Contrast of the light shafts (inferred from name; atmos.inl:297). |
| `WaterFog` (float) | `1.0` (atmos.inl:297) | not in file | - | Live (inferred) | Multiplier of the fog that thickens over the GI water mask (inferred; pbr.inl:23 DebugWater bit 4 switches this term off). |
| `HueLock` (float) | `1.0` (atmos.inl:297) | not in file | - | Live (inferred) | Keeps haze hue from drifting with view direction (inferred from name). |
| `SkyFogDist` (float (fraction of MaxDist)) | `0.45` (atmos.inl:297) | `0.45` (ini:40) | - | Live (inferred) | Fog depth for sky and painted rock as a fraction of MaxDist (ini:39-40). |
| `HazeHue` (float) | `0.15` (atmos.inl:297) | not in file | - | Live (inferred) | How much haze takes the sky hue (inferred from name). README says 0.25; code says 0.15 (conflict). |
| `HazeSky` (float) | `0.0` (atmos.inl:297) | not in file | - | Live (inferred) | How much haze colour follows the sky estimate (u_hazeSky, atmos.inl:449; 0 when the sky term is off). README says 0.6; code says 0.0 (conflict). |
| `DepthBlur` (float, pixels) | `0` (atmos.inl:297) | not in file | - | Live (inferred) | Blur of distant scenery, 0 = off (inferred from name; README: try 2 to 3). |
| `BlurStart` (float, world units) | `4000` (atmos.inl:297) | not in file | - | Live (inferred) | View distance where DepthBlur starts (inferred from name). |
| `BlurRange` (float, world units) | `12000` (atmos.inl:297) | not in file | - | Live (inferred) | View distance range over which DepthBlur ramps (inferred from name). |
| `SunColor` (string r,g,b) | `1.0,0.86,0.66` (atmos.inl:306) | not in file | - | Live (inferred) | Tint of the sun term, parsed with sscanf (atmos.inl:306-307). |

## [fog]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Amount` (float) | `1.0` (tyrender.cpp:602) | not in file | Slider 'Fog amount' 0 to 3, default 1.00 (src/launcher_ui.inc:26) | Live | Fog strength multiplier in the density (atmos.inl:446). Reloaded with the rest (tyrender.cpp:602). |
| `Distance` (float (divisor, min 0.05 used)) | `1.0` (tyrender.cpp:601) | not in file | Slider 'Fog distance' 0.30 to 4, default 1.00 (src/launcher_ui.inc:27) | Live | Divides the fog density (atmos.inl:446): a larger value gives thinner fog. Reloaded with the rest (tyrender.cpp:601). |
| `LateBoost` (float, 1 = off) | `1.0` (tyrender.cpp:602) | not in file | - | Live | Multiplier on the engine fog of see-through draws that come after the fog pass (atmos.inl:326 comment). Only acts above 1 (ini:75). Reloaded (tyrender.cpp:602). |
| `SeeThruDepth` (int 0/1) | `0` (atmos.inl:299) | `0` (ini:77) | - | Live (inferred) | 1 = write depth for non-emissive see-through draws before the fog pass (ini:74-75). Default off; the ini says playtest first. |
| `LateEmitters` (int 0/1) | `0` (atmos.inl:299) | `0` (ini:78) | - | Live (inferred) | 1 = LateBoost also hazes emissive draws that come after the fog pass (ini:75, 78). |
| `LateEmitBoost` (float, 1 = off) | `1.0` (atmos.inl:301) | `1.0` (ini:83) | - | Live (inferred) | Fog multiplier for emissive draws after the fog pass, for fire and glow (ini:82; atmos.inl:301). |
| `WaterBoost` (float, 1 = off) | `1.0` (atmos.inl:300) | `1.0` (ini:81) | - | Live (inferred) | Fog multiplier for late water draws whose texture name contains 'water' or matches WaterNames (ini:79-81; atmos.inl:300). |
| `FoliageBoost` (float, 1 = off) | `1.0` (atmos.inl:302) | `1.0` (ini:85) | - | Live (inferred) | Fog multiplier for late see-through draws that are not water, for example tree cards (ini:84-85; atmos.inl:302). |
| `WaterMaskSoft` (int 0/1) | `0` (atmos.inl:303) | `0` (ini:87) | - | Live (inferred) | 0 = hard cut on the GI water mask, 1 = smoothed 3x3 mask (ini:86-87; atmos.inl:303). Untested per ini. |
| `WaterNames` (string, comma-separated substrings) | `` (tex.inl:77) | `` (ini:89) | - | Restart (startup only) | Extra texture-name substrings that count as water for WaterBoost (ini:88-89). Read only at startup in LoadTexTags (tex.inl:77). |

## [emit]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `0` (emit.inl:34) | `0` (ini:107) | - | Mixed (per-frame gate, emit.inl:139, 175, 227; rewrite at shader compile, emit.inl:45) | Master switch for emitter light. Off in code and in the shipped file (ini:107). The output rewrite only happens when this is 1 at compile time (emit.inl:45). |
| `Strength` (float) | `1.0` (emit.inl:35) | `1.0` (ini:108) | - | Live (inferred) | Composite gain of emitter light (ini:108; emit.inl:35). |
| `Radius` (float, half-res texels) | `12` (emit.inl:36) | `12` (ini:109) | - | Live (inferred) | Blur step for all emitters (ini:109; emit.inl:36). |
| `Spread` (float, clamped 0.25 to 4) | `1.0` (emit.inl:37) | not in file | - | Live (inferred) | Spread of the emitter splat (emit.inl:37; inferred from name). |
| `MaxEmitters` (int) | `16` (emit.inl:34) | `16` (ini:110) | - | Live (inferred) | Cap on emitter draws splatted per frame (ini:110; emit.inl:34). |
| `Compensate` (int 0/1) | `0` (emit.inl:38) | not in file | - | Live (inferred) | Compensation switch for emitters (inferred from name; no comment in code). |

## [spill] and [decal]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `LogEmitters` (int 0/1) | `0` (tex.inl:69) | `0` (ini:66) | - | Restart (startup only) | 1 = log additive draws per albedo texture every 600 frames (ini:64-66; tex.inl:69). Logging only. Read only at startup. |

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Clamp` (int 0/1) | `0` (tex.inl:72) | `0` (ini:69) | - | Restart (startup only) | 1 = cap the luminance of decal draws at Max before output (ini:67; tex.inl:72). Read only at startup. |
| `Max` (float) | `0.9` (tex.inl:73) | `0.9` (ini:70) | - | Restart (startup only) | Luminance cap for decal draws when Clamp is 1 (ini:70; tex.inl:73). |
| `Names` (string, comma-separated) | `ty_footprint.dds,ty_footprint2.dds` (tex.inl:74) | `ty_footprint.dds,ty_footprint2.dds` (ini:71) | - | Restart (startup only) | Texture names treated as decals (ini:71; tex.inl:74). |

## Cost

- Volumetric fog and light shafts off saves about 2.1 ms against Medium (cost_table.md line 17; PERFORMANCE.md line 51).
- `ContactShadows` above 0 adds about 1.7 ms (cost_table.md line 12; PERFORMANCE.md line 54).
- Measured on the merged v0.3 build, RX 7600 XT, 2560x1440. Not re-timed for v0.3.2.

## Limits

- The fog density is one formula (`src/atmos.inl:446`): the haze keys and the `[fog]` multipliers only scale it. The keys cannot make fog that depends on world-space data, because the passes are screen-space (see [constraints and limits](../concepts/constraints-and-limits.md)).
- `[emit]` Enable changes the shader text only when it is 1 at shader compile time (`src/emit.inl:45`), so switching it on in a running game may not show the emitter light until the next start (see NEEDS INVESTIGATION).
- The `[fog]` water and foliage keys are marked untested in the shipped file (`ini:86-87`, `ini:89`).

## Discrepancies

1. **[atmos] haze keys versus README.** The README table gives `HazeSky` 0.6, `SkyHaze` 0.5, `SkyHazeHeight` 0.25, `HazeHue` 0.25 and `ContactShadows` 0 as defaults. Code defaults are `HazeSky` 0.0 (`src/atmos.inl:297`), `SkyHaze` 0.8 (`src/atmos.inl:297`), `SkyHazeHeight` 0.45 (`src/atmos.inl:297`), `HazeHue` 0.15 (`src/atmos.inl:297`) and `ContactShadows` 0.35 (`src/atmos.inl:296`). The code values are used on this page.
2. **[atmos] Intensity and the presets.** Code default 2.5 (`src/atmos.inl:297`). Shipped file 1.2 (`ini:36`). The launcher table writes 2.5 / 1.2 / 1.2 / 1.0 (`src/launcher_ui.inc:7`). The README says presets write 2.2. `src/ui_part_a.inc:7` gives 2.5 / 2.2 / 2.2 / 2.1, but no file includes `ui_part_a.inc` (see [presets.md](presets.md)). `docs/STATUS_2026-10-13.md` line 12 gives Ultra haze 2.1. The launcher value is used.
3. **[atmos] Desaturation and HazeLift for Low.** The launcher writes 0.8 and 0.35 for Low and 0.3 and 0.15 for the others (`src/launcher_ui.inc:493`). `tools/presets.ps1` line 7 writes 0.3 and 0.15 for every preset.
4. **[atmos] SunIntensity.** Code default 1.4 (`src/atmos.inl:295`). The launcher slider default is 2.00 (`src/launcher_ui.inc:29`) and every launcher save writes all slider keys (`src/launcher_ui.inc:246`), so after the first save the file holds 2.00.
5. **[atmos] ContactShadows.** Code default 0.35 (`src/atmos.inl:296`), shipped 0 (`ini:35`), slider default 0.00 (`src/launcher_ui.inc:30`). The README says 0.
6. **Two `Amount` keys.** `[atmos] FogAmount` (default 0.8, `src/atmos.inl:296`) and `[fog] Amount` (default 1.0, `src/tyrender.cpp:722`) both multiply the density (`src/atmos.inl:446`). Not a conflict, but easy to confuse.
7. **Comment placement.** `ini:74-75` (the `[fog]` see-through notes) sit under the `[light]` heading (`ini:72-73`). Cosmetic, but misleading.
8. **[fog] WaterNames** is read only at startup (`src/tex.inl:77`, called from `src/tyrender.cpp:725`). A reload does not pick up a change.

## NEEDS INVESTIGATION

- **Froxel=0 path.** Not traced: whether the ray-march path reads the same haze keys as the froxel path. Method: diff the uniform lists in `src/atmos.inl` for both paths, or set `Froxel=0` and compare a haze view.
- **[emit] Enable at run time.** Method: in a running game set `Enable=1`, write the reload file (or use the launcher), and check whether emitter light appears without a restart. The shader rewrite is compile-time (`src/emit.inl:45`).
- **[fog] WaterMaskSoft.** The ini says "untested" (`ini:86-87`). Method: playtest with 0 and 1.
- **[atmos] Froxel and FroxelPixels live behaviour.** Not traced. Method: change `FroxelPixels` through the reload and watch the volume size in the log.

## See also

- [settings-ao-gi-lod.md](settings-ao-gi-lod.md), [settings-post-shadows-light-pbr-tess.md](settings-post-shadows-light-pbr-tess.md), [settings-system-debug.md](settings-system-debug.md)
- [presets.md](presets.md)
- [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md)
- [water, fog and decals](../shaders/water-fog-and-decals.md), [emitter light](../shaders/emitter-light.md)
