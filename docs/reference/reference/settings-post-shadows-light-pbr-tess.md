# Settings: post, shadows, natural light, PBR, tessellation

> Status: documented at commit 4db1d7d (`main`, Merge PR #122); last verified 2026-10-18. Audience: B (tuning and bug reports), C (review).

## Purpose

This page lists the keys in `[post]`, `[shadows]`, `[light]`, `[pbr]`, `[tess]`, `[tex]` and `[portability]`. `[post]` sets the grade, anti-aliasing and sharpening (`LoadFXConfig`, `src/fx.inl:146`). `[shadows]` sets the shadow filter patch and its map size (`LoadShadowConfig`, `src/tyrender.cpp:150`; the light keys are read in `src/light.inl`). `[light]` sets the natural-light rewrite (`src/light.inl:16`). `[pbr]` sets the PBR surface rewrite (`src/pbr.inl:16`). `[tess]` sets tessellation and relief (`src/tess.inl:33`). `[tex]` and `[portability]` are small switches in `src/tex.inl:65` and `src/tess.inl:31`.

For the passes, read [post](../shaders/taa-fxaa.md) and [grade and sharpen](../shaders/grade-and-sharpen.md), [shadow filter](../shaders/shadow-filter.md), [natural light](../shaders/natural-light.md), [PBR surface detail](../shaders/pbr-surface-detail.md) and [tessellation and relief](../shaders/tessellation-and-relief.md).

## How to read the tables

- **Key (type, range).** The section and key as the loader reads them. Keys are case-sensitive.
- **Code default (file:line).** The second argument of the `GetPrivateProfile*` call, which is used when the key is missing from `tyrender.ini`. The file:line is the first read in the code that is not a log line.
- **Shipped.** The value in `src/default_tyrender.ini` (`ini:N` is that file's line). "not in file" means the code default is used.
- **Launcher and presets.** What `src/launcher_ui.inc` writes or reads for the key. Preset values are in the order Low / Medium / High / Ultra.
- **Live or restart.** Definitions are in [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md). "Live" means the reload path re-reads the key and the value is used per frame or per draw. "(inferred)" means the use site was not traced line by line.
- **Effect.** One line. "(inferred from name)" means the text comes from the key name only. Mechanisms are on the shader pages linked under See also.

Every key in the code appears in exactly one table on the four settings pages (227 keys in all, checked against a grep of `GetPrivateProfile*` and the `F`/`LF`/`PF` macros in `src/*.inl` and `src/tyrender.cpp`).

## [post]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (fx.inl:148) | `1` (ini:50) | Presets: all 1 | Live | Master switch for the post chain (fx.inl:178). |
| `AA` (int 0 to 3) | `2` (fx.inl:149) | `2` (ini:51) | Presets: 1 (Low) / 2 / 2 / 2; combo box | Live | 0 = none, 1 = FXAA, 2 = TAA, 3 = TAA plus FXAA (fx.inl:149 comment). TAA also turns on sub-pixel jitter (fx.inl:162). |
| `FXAA` (int 0/1) | `1` (fx.inl:148) | not in file | - | Live (inferred) | FXAA pass switch (fx.inl:148; FXAA is used for AA 1 and 3). |
| `TAABlend` (float) | `0.85` (fx.inl:150) | not in file | - | Live (inferred) | History blend of TAA (inferred from name; fx.inl:150). |
| `TAAReject` (float) | `1.0` (fx.inl:151) | not in file | - | Live (inferred) | 0 = old history rejection. 1 = history far outside the colour neighbourhood blends at most 0.35 (fx.inl:151 comment). |
| `Tonemap` (int 0/1) | `1` (fx.inl:155) | not in file | - | Live (inferred) | Tone curve switch (fx.inl:155; inferred from name). |
| `TonemapKnee` (float) | `0.72` (fx.inl:154) | not in file | - | Live (inferred) | Knee of the shoulder tone curve (inferred from name; fx.inl:154). |
| `TonemapRoll` (float) | `0.15` (fx.inl:154) | not in file | - | Live (inferred) | Roll-off of the tone curve (inferred from name; fx.inl:154). |
| `SplitTone` (float) | `0.12` (fx.inl:154) | not in file | - | Live (inferred) | Split-tone amount (inferred from name; fx.inl:154). |
| `Brightness` (float) | `1.0` (fx.inl:154) | `0.94` (ini:52) | Presets: 1.00 (Low) / 0.94; slider 'Brightness' 0.70 to 1.50, default 0.94 (src/launcher_ui.inc:18) | Live | Overall gain of the grade. Shipped file says 0.94; code default is 1.0 (conflict). |
| `Contrast` (float) | `1.0` (fx.inl:153) | not in file | Slider 0.80 to 1.30, default 1.00 (src/launcher_ui.inc:19) | Live | Contrast of the grade (inferred from name; fx.inl:153). |
| `Saturation` (float) | `1.0` (fx.inl:153) | `1.18` (ini:53) | Presets: 1.03 (Low) / 1.18; slider 0.60 to 1.60, default 1.18 (src/launcher_ui.inc:20) | Live | Colour saturation of the grade (fx.inl:153). Shipped file says 1.18; code default is 1.0 (conflict). |
| `Vibrance` (float) | `0.04` (fx.inl:153) | not in file | - | Live (inferred) | Saturation weighted toward dull colours (inferred from name). |
| `Sharpen` (float) | `0.25` (fx.inl:153) | not in file | Slider 'Sharpening' 0 to 1, default 0.35 (src/launcher_ui.inc:32) | Live | Sharpen strength (fx.inl:153). Code default is 0.25 and the slider default is 0.35 (conflict). |
| `SharpenHaloClamp` (float) | `1.0` (fx.inl:153) | not in file | - | Live (inferred) | Clamp on sharpen halos (fx.inl:153; inferred from name). |
| `Shoulder` (float) | `0.12` (fx.inl:154) | not in file | - | Live (inferred) | Highlight shoulder of the grade (inferred from name; fx.inl:154). |
| `Vignette` (float) | `0.0` (fx.inl:154) | not in file | - | Live (inferred) | Corner darkening (inferred from name; fx.inl:154). |
| `Gamma` (float) | `1.0` (fx.inl:154) | not in file | - | Live (inferred) | Gamma of the grade (inferred from name; fx.inl:154). |

## [shadows]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Patch` (int 0/1) | `1` (tyrender.cpp:152) | `1` (ini:46) | Launcher always writes 1 | Restart (inferred: shader rewrite at compile, tyrender.cpp:388, 393) | 1 = use the patched shadow filter (PatchShadowShader, tyrender.cpp:174). Applied when the engine shader is compiled. |
| `Temporal` (int 0/1) | `0` (tyrender.cpp:152) | not in file | - | Live (inferred) | 0 = stable per-pixel rotation, 1 = 8-frame cycle matching the TAA jitter (tyrender.cpp:144 comment; light.inl:108). |
| `Grid` (int 0/1) | `1` (tyrender.cpp:152) | not in file | - | Live (inferred) | 1 = fixed grid filter, no per-pixel noise (value sent in tyLt[4].z, light.inl:115). README says the grid is 3x3 at Medium and 4x4 at Ultra; not traced here. |
| `MapScale` (int, clamped 1 to 4) | `2` (tyrender.cpp:153) | `2` (ini:47) | Presets: 1 / 2 / 2 / 4 (combo box) | Restart (startup only) (inferred) | Multiplies the 2048 stock shadow map (tyrender.cpp:255, 274). 1 = 2048, 2 = 4096, 4 = 8192. The clamp also allows 3 (6144 by the formula); not tested. |
| `Taps` (int, clamped 4 to 32) | `16` (tyrender.cpp:154) | `16` (ini:48) | Presets: 8 / 16 / 16 / 32 | Docs say restart; code writes it into the light block each frame (conflict, NI) | Filter taps per shadow sample; sent in tyLt[0].z (light.inl:108-110). Docs (README, PERFORMANCE.md:85) say restart. |
| `Radius` (float) | `1.4` (tyrender.cpp:155) | not in file | - | Live | Filter radius in shadow-map texels (tyLt[0].x; tyrender.cpp:155, light.inl:110). |
| `Strength` (float) | `0.5` (tyrender.cpp:156) | not in file | Slider 'Shadow darkness' 0 to 1, default 0.50 (src/launcher_ui.inc:23) | Live | Shadow darkness. Re-applied to every program by the reload path (tyrender.cpp:599). |
| `GrassStrength` (float) | `0.35` (tyrender.cpp:157) | not in file | Slider 'Shadows on grass' 0 to 1, default 0.35 (src/launcher_ui.inc:24) | Live (inferred) | Shadow strength on grass blades (tyrender.cpp:157; grass template g_grassVS, tyrender.cpp:146; inferred). |
| `PlaneBias` (int 0/1) | `0` (light.inl:25) | not in file | - | Live | Plane-based shadow bias switch (tyLt[6].x, light.inl:117). |
| `PlaneClamp` (float) | `0.004` (light.inl:26) | not in file | - | Live | Clamp used by the plane bias (tyLt[6].z, light.inl:117). |
| `SlopeBias` (float) | `0` (light.inl:28) | not in file | - | Live | Slope-scaled shadow bias (tyLt[6].w, light.inl:117). |
| `SkinBias` (float) | `0` (light.inl:29) | not in file | - | Live | Shadow bias for skinned characters (tyLt[7].x, light.inl:117). |
| `CloseCracks` (float) | `0` (light.inl:30) | not in file | - | Live | 1 = fill 1 to 3 texel gaps between casters, grid filter only (tyLt[4].w, light.inl:115, 30 comment). |

## [light]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (light.inl:18) | `1` (ini:73) | - | Mixed (per frame, light.inl:109; rewrite at compile, light.inl:35) | Master switch for the natural-light shader rewrite (light.inl:35). Off means no tint, hemisphere or foliage terms. |
| `ShadowTint` (int 0/1) | `1` (light.inl:18) | not in file | - | Mixed (inferred) | Tint shadows toward the fog and sky blue (inferred from name; tyLt[1], tyrender.cpp:167). |
| `SkipCharacters` (int 0/1) | `1` (light.inl:19) | not in file | - | Mixed (inferred) | Characters skip the natural-light term (inferred from name). |
| `Hemisphere` (int 0/1) | `1` (light.inl:19) | not in file | - | Mixed (inferred) | Hemisphere ambient term on or off (inferred from name; tyLt[2], tyrender.cpp:167). |
| `Foliage` (int 0/1) | `1` (light.inl:19) | not in file | - | Mixed (inferred) | Foliage translucency term on or off (tyLt[4].x, light.inl:115). |
| `ShadowTintAmount` (float) | `1.0` (light.inl:21) | not in file | - | Live | Strength of the shadow tint (tyLt[1].w; light.inl:21). |
| `TintFromFog` (float) | `0.15` (light.inl:21) | not in file | - | Live (inferred) | How much fog colour mixes into the shadow tint (inferred from name). |
| `ShadowSaturation` (float) | `0.0` (light.inl:21) | not in file | - | Live | Resaturation of shadowed pixels (tyLt[3].w per tyrender.cpp:167 comment). |
| `ShadeCool` (float) | `0.22` (light.inl:21) | not in file | - | Live (inferred) | Cool shift of shaded areas (inferred from name). |
| `ShadowDepth` (float) | `0.12` (light.inl:21) | not in file | - | Live (inferred) | Depth of the shadow darkening (inferred from name). |
| `HemisphereAmount` (float) | `0.10` (light.inl:22) | not in file | - | Live | Strength of the hemisphere light (tyLt[2].w per tyrender.cpp:167 comment). |
| `HemisphereStatic` (float) | `0.5` (light.inl:22) | not in file | - | Live | Hemisphere scale for static meshes (tyLt[4].y per tyrender.cpp:167 comment). |
| `FoliageAmount` (float) | `0.10` (light.inl:22) | not in file | - | Live | Strength of foliage translucency (tyLt[4].x, light.inl:115). |
| `ShadowFill` (float) | `0.015` (light.inl:22) | not in file | - | Live (inferred) | Fill light inside shadows (inferred from name). |

## [pbr]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (pbr.inl:18) | `1` (ini:55) | - | Restart (rewrite at compile, pbr.inl:39, 46) | Master switch for the PBR surface rewrite (pbr.inl:39, 46; README: restart). |
| `LitProps` (int 0/1) | `1` (pbr.inl:18) | not in file | - | Restart (inferred: rewrite at compile) | 0 = dynamically lit non-skinned props keep the stock look (pbr.inl:18 comment). |
| `Debug` (int 0/1) | `0` (pbr.inl:18) | not in file | - | Restart (inferred: source insert at compile, pbr.inl:165) | 1 = normals view (pbr.inl:165). |
| `WaterStable` (int 0/1) | `1` (pbr.inl:20) | `1` (ini:59) | - | Live (inferred) | 1 = water-tagged draws (emissive or transparent tags) drop bump, relief, specular and sky Fresnel (pbr.inl:19-20 comment; issue #5). |
| `SkyFresnelSkipWater` (int 0/1) | `0` (pbr.inl:20) | `0` (ini:63) | - | Live (inferred) | 1 = water-tagged draws skip only the sky Fresnel term (pbr.inl:20 comment). |
| `DebugWater` (int bit mask) | `0` (pbr.inl:23) | `0` (ini:61) | - | Live (inferred) | Bisect bits for water flicker: 1 spec, Fresnel and sheen off; 2 bump and relief off; 4 atmos water fog off; 8 hemisphere and shadow tint off (pbr.inl:21-25). Logs [dbgwater] lines at load. |
| `NormalStrength` (float) | `1.0` (pbr.inl:27) | not in file | - | Live | Strength of the albedo-derived bump (pbr.inl:27; inferred from name). |
| `Specular` (float) | `0.5` (pbr.inl:27) | not in file | - | Live | Specular amount (pbr.inl:27; inferred from name). |
| `Roughness` (float) | `0.65` (pbr.inl:27) | not in file | - | Live | Base roughness (pbr.inl:27; inferred from name). |
| `HeightScale` (float) | `3.0` (pbr.inl:27) | not in file | - | Live | Height scale of the albedo height field (pbr.inl:27; inferred from name). |
| `LowFreqBump` (float) | `0.6` (pbr.inl:28) | not in file | - | Live | Weight of the low-frequency bump (pbr.inl:28; inferred from name). |
| `Cavity` (float) | `0.30` (pbr.inl:28) | not in file | - | Live | Cavity darkening (pbr.inl:28; inferred from name). |
| `ValleyRough` (float) | `0.30` (pbr.inl:28) | not in file | - | Live | Extra roughness in valleys (pbr.inl:28; inferred from name). |
| `Sheen` (float) | `0.12` (pbr.inl:28) | not in file | - | Live | Fresnel sheen amount (pbr.inl:28; inferred from name). |
| `DetailStrength` (float) | `1.0` (pbr.inl:29) | `1.0` (ini:56) | - | Live | Strength of the detail layer (pbr.inl:29; first pass left this unverified). |
| `DetailScale` (float) | `1.0` (pbr.inl:29) | `1.0` (ini:57) | - | Live | UV scale of the detail layer (pbr.inl:29; inferred from name). |
| `DetailTexture` (float) | `1.0` (pbr.inl:29) | not in file | - | Live | Weight of the detail texture (pbr.inl:29; inferred from name). |
| `DetailDisp` (float) | `0.8` (pbr.inl:29) | not in file | - | Live | Inward depth of the detail layer (pbr.inl:29; README: inward depth). |
| `FadeStart` (float, world units) | `1500` (pbr.inl:30) | not in file | - | Live | Distance where PBR detail starts to fade (pbr.inl:30). |
| `FadeEnd` (float, world units) | `3500` (pbr.inl:30) | not in file | - | Live | Distance where PBR detail has faded out (pbr.inl:30). |

## [tess]

`Scope` decides whether geometry is tessellated. `all` and `env` tessellate environment geometry; every other value does not (`src/tess.inl:36`, `src/tess.inl:107`). Characters are never tessellated (`src/tess.inl:6`, `src/tess.inl:107`).

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (tess.inl:35) | `1` (ini:42) | - | Restart (inferred: tessellation shaders are rewritten and linked at load, tess.inl:99) | Master switch for tessellation and relief (tess.inl:99). The relief term in pbr.inl also checks this key (pbr.inl:214). |
| `Debug` (int 0/1) | `0` (tess.inl:35) | not in file | - | Restart (inferred: shader text, tess.inl:248, 374) | Non-zero darkens triangle edges so the patch layout shows (tess.inl:248, 374). |
| `Scope` (string: env-detail, skin, off, env, all) | `env-detail` (tess.inl:36) | `skin` (ini:43) | Presets: skin / skin / skin / all (launcher default when missing: skin, launcher_ui.inc:74) | Restart (inferred: geometry gate, tess.inl:107) | all or env = environment geometry is tessellated; any other value = no geometry tessellation (tess.inl:36, 107). Fragment relief still runs (inferred, see Limits on the settings page). Code default env-detail. |
| `Alpha` (float) | `0.7` (tess.inl:37) | not in file | - | Restart per README (conflict; path not traced) | Tessellation detail weight (inferred from name; README lists it under restart). |
| `TargetPx` (float, pixels) | `12` (tess.inl:38) | not in file | - | Restart per README (conflict; path not traced) | Target triangle edge length in pixels (inferred from name). |
| `AlphaWorld` (float) | `0.45` (tess.inl:39) | not in file | - | Live (inferred) | World-space variant of Alpha (inferred from name). |
| `EdgeLength` (float, world units) | `25` (tess.inl:40) | not in file | - | Live (inferred) | World edge length for tessellation (inferred from name). |
| `FadeStart` (float, world units) | `1500` (tess.inl:41) | not in file | - | Live (inferred) | Distance where geometry tessellation starts to fade (inferred from name). |
| `FadeEnd` (float, world units) | `4000` (tess.inl:42) | not in file | - | Live (inferred) | Distance where geometry tessellation has faded out (inferred from name). |
| `MaxSmoothing` (float, world units) | `3` (tess.inl:43) | not in file | - | Live (inferred) | Maximum Phong smoothing offset (tess.inl:43 comment: world units). |
| `Displacement` (float, world units) | `2.5` (tess.inl:44) | not in file | - | Live (inferred) | Maximum height from the texture, plus or minus (tess.inl:44 comment). |
| `DisplacementLod` (float) | `1.5` (tess.inl:45) | not in file | - | Live (inferred) | Mip bias for displacement sampling (inferred from name). |
| `TargetEdge` (float, world units, min 0.5) | `4` (tess.inl:47) | not in file | - | Live (inferred) | Edge size per sub-triangle near surfaces (tess.inl:47 comment). |
| `ReliefDepth` (float, world units) | `3` (tess.inl:48) | not in file | Slider 'Surface relief' 0 to 10, default 3.00 (src/launcher_ui.inc:31) | Live | Maximum inward depth of cracks and valleys; 0 = old plus or minus path (tess.inl:48 comment). |
| `ReliefCurve` (float, clamped 0.2 to 4) | `1.0` (tess.inl:52) | not in file | - | Live (inferred) | Shape of the relief curve; below 1 lifts weak relief (tess.inl:49-52 comment). |
| `ReliefKnee` (float, world units, 0 = off) | `0` (tess.inl:53) | not in file | - | Live (inferred) | Above this depth, relief grows only at ReliefKneeSlope of its rate (tess.inl:49-53 comment). |
| `ReliefKneeSlope` (float, clamped 0 to 1) | `0.35` (tess.inl:54) | not in file | - | Live (inferred) | Rate above the knee (tess.inl:54). |
| `GroundRelief` (float) | `1.0` (tess.inl:55) | not in file | - | Live (inferred) | Multiplier on up-facing surfaces such as sand and paths (tess.inl:55 comment). |
| `PropRelief` (float) | `0.7` (tess.inl:56) | not in file | - | Live (inferred) | Multiplier on walls, fences, rope and rock (tess.inl:56 comment). |
| `ReliefFadeStart` (float, world units) | `700` (tess.inl:57) | not in file | - | Live (inferred) | Distance where relief starts to fade (tess.inl:57; inferred from name). |
| `ReliefFadeEnd` (float, world units) | `1600` (tess.inl:58) | not in file | - | Live (inferred) | Distance where relief is gone (tess.inl:58; inferred from name). |
| `ReliefLods` (float) | `0` (tess.inl:59) | not in file | - | Live (inferred) | Extra mip bias on height sampling; positive = smoother (tess.inl:59 comment). |
| `EdgeFade` (float, world units) | `2.5` (tess.inl:60) | not in file | - | Live (inferred) | Fades relief to zero within this distance of a patch border (tess.inl:60 comment). |
| `ReliefSteps` (int, clamped 3 to 12) | `8` (tess.inl:66) | not in file | - | Live (inferred) | Parallax steps in the fragment shader (tess.inl:66 comment). |
| `ReliefFeature` (float, min 0.5) | `1.0` (tess.inl:67) | not in file | - | Live (inferred) | Mip bias for features; above 1 gives coarser features (tess.inl:67 comment). |
| `WorldMaxLevel` (int, clamped 2 to 63) | `48` (tess.inl:68) | not in file | - | Restart (inferred: tessellation level, tess.inl:68) | Maximum tessellation level for world geometry (inferred from name). |
| `ABToggle` (int 0/1) | `0` (tess.inl:69) | not in file | - | Live (inferred) | A/B switch for the tessellation state (tess.inl:69; pbr.inl:210 toggles the live flag). |
| `Cull` (int 0/1) | `1` (tess.inl:70) | not in file | - | Restart (inferred: TCS code, tess.inl:70) | Frustum and back-face culling of patches in the TCS (tess.inl:70 comment). |
| `MaxLevel` (int, clamped 1 to 32) | `8` (tess.inl:71) | `8` (ini:44) | Presets: 4 / 8 / 8 / 24 | Restart per README and PERFORMANCE.md:85 (inferred) | Maximum tessellation level (tess.inl:71). Launcher writes it on every save. |

## [tex] and [portability]

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `Enable` (int 0/1) | `1` (tex.inl:67) | not in file | - | Restart (startup only) | Texture-name tagging on or off (tex.inl:67). Read only at startup in LoadTexTags. |
| `Debug` (int 0/1) | `0` (tex.inl:67) | not in file | - | Restart (startup only) | Texture debug logging (tex.inl:67; inferred from name). |
| `LogUnknown` (int 0/1) | `0` (tex.inl:68) | not in file | - | Restart (startup only) | 1 = log texture names with no tag (tex.inl:68; plan item N-27). |
| `SkinnedByTag` (int 0/1) | `1` (tex.inl:70) | not in file | - | Restart (startup only) | Skinned shaders get the detail code too; the tags switch it per draw (tex.inl:70 comment). |

| Key (type, range) | Code default (file:line) | Shipped `src/default_tyrender.ini` | Launcher and presets | Live or restart | Effect |
|---|---|---|---|---|---|
| `TessVersionGate` (int 0/1) | `1` (tess.inl:72) | `1` (ini:102) | - | Restart (inferred: used when tessellation programs link, tess.inl:31) | 1 = tessellation stages are not compiled on GL contexts below 4.0 (tess.inl:31; ini:101-102). |

## Cost

- Tessellation: `Scope=all` costs about +6.2 ms over Medium (cost_table.md line 15; PERFORMANCE.md line 57). Turning relief off saves about 0.05 ms at Medium (PERFORMANCE.md line 52).
- Shadow map 8192 with 32 taps: about +5.1 ms (cost_table.md line 13). Shadow Grid v0.3.2 against the old filter: about +0.7 ms (PERFORMANCE.md line 58).
- Switching TAA off (to FXAA or none) measured about +0.4 ms, so TAA itself is not the expensive part (PERFORMANCE.md line 53; cost_table.md line 18). The launcher cost model gives PBR 0.0 (`src/launcher_ui.inc:39-41` comment).
- All values from the merged v0.3 build on RX 7600 XT at 2560x1440 (see [presets.md](presets.md)).

## Limits

- The tessellation and relief keys act on textures, not on the mesh. Relief comes from the albedo texture as a height field. Characters are excluded (see [constraints and limits](../concepts/constraints-and-limits.md)).
- The shadow patch works on the engine's own shadow shader text. If the text is not found the patch is not applied (see [shadow filter](../shaders/shadow-filter.md)).

## Discrepancies

1. **[post] Brightness and Saturation.** Shipped file 0.94 and 1.18 (`ini:52-53`). Code defaults 1.0 and 1.0 (`src/fx.inl:153-154`). The README has the code values. The shipped file is what the launcher writes for Medium.
2. **[post] Sharpen.** Code default 0.25 (`src/fx.inl:153`), README 0.25, launcher slider default 0.35 (`src/launcher_ui.inc:32`). The slider value is written on every launcher save, so most users have 0.35 in their file.
3. **[tess] Scope.** Code default `env-detail` (`src/tess.inl:36`). Shipped file `skin` (`ini:43`). Launcher default when the key is missing `skin` (`src/launcher_ui.inc:74`). README says `env-detail` is the default and that `off` means "nothing". In code, `off` only avoids geometry tessellation (`src/tess.inl:107`); the fragment relief path is not gated by `Scope` (inferred from `src/pbr.inl:214`, which checks `[tess] Enable` and `[pbr] Enable`). `docs/STATUS_2026-10-13.md` line 10 calls the Medium relief "env-detail"; the shipped and launcher Medium value is `skin`. The two values give the same geometry behaviour (both are not `all`), but the labels differ.
4. **[shadows] MapScale range.** README lists 1, 2 and 4 (2048, 4096, 8192). Code clamps to 1 to 4 (`src/tyrender.cpp:153`), so 3 is accepted and would give 6144 by the formula in `src/tyrender.cpp:255` and `:274`. Not tested.
5. **[shadows] Taps restart.** README (Taps "(restart)") and `docs/PERFORMANCE.md` line 85 say restart. Code writes the tap count into the per-frame light block (`src/light.inl:108-110`) and the reload path re-reads it (`src/tyrender.cpp:598`). The docs may be right if the shader loop bound is compiled in. Not resolved here.
6. **[debug] ShadowView.** The comment at `src/light.inl:24` says restart-only. The reload path calls `LoadLightConfig` (`src/tyrender.cpp:598`) and `LightApply` writes the value each apply (`src/light.inl:117`). In code it behaves as live. The settings-system-debug page has the key.
7. **[tess] Alpha and TargetPx.** README lists them under restart. The code path was not traced (see NEEDS INVESTIGATION).
8. **[pbr] DetailStrength.** First pass said the code default was not verified. It is 1.0 (`src/pbr.inl:29`).
9. **[tess] Enable and Scope in the presets.** The Low preset writes `Scope=skin` even though relief is off there. This is harmless but means `Scope` alone does not show what is active.

## NEEDS INVESTIGATION

- **[shadows] Grid size.** README says the grid is 3x3 at Medium and 4x4 at Ultra. Not traced here. Method: the shadow filter code (Ch04 page), then compare a dumped shader under `tyrender\dump`.
- **[shadows] Taps live or restart.** Method: change `Taps` through the reload file (or the overlay, which exposes `Taps`, `src/overlay.inl:30`) and compare a shadow edge. Or grep the tap loop in the patched shader for a constant bound.
- **[shadows] MapScale 3.** Method: set `MapScale=3`, start the game, and read the log line `[shadows] shadow map enlarged to` (`src/tyrender.cpp:274`).
- **[tess] Scope=off and relief.** Not tested. Method: set `Scope=off` with `Enable=1` and compare relief on sand at a grazing angle.
- **[tess] Alpha, TargetPx, MaxLevel restart status.** Method: change each through the reload path and check the tessellated edge length in a debug view (`[tess] Debug=1`, `src/tess.inl:248`).
- **[light] and [emit] mixed status.** The rewrite happens at compile time (`src/light.inl:35`), the uniform part per frame. Method: toggle `[light] Enable` at run time and look for the tint.

## See also

- [settings-ao-gi-lod.md](settings-ao-gi-lod.md), [settings-atmos-fog-emit.md](settings-atmos-fog-emit.md), [settings-system-debug.md](settings-system-debug.md)
- [presets.md](presets.md)
- [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md)
- [shadow filter](../shaders/shadow-filter.md), [PBR surface detail](../shaders/pbr-surface-detail.md), [tessellation and relief](../shaders/tessellation-and-relief.md)
