# Natural light (shadow tint, hemisphere, foliage)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (tester, tunes `[light]`) and A (contributor).

## Purpose

The engine darkens shadowed pixels with a neutral factor, so shadows read as grey. The light rewrite changes the colour of lit and shaded pixels so that:

- shadows read as cool, sky-lit shade instead of grey (shadow tint and fill);
- surfaces facing up pick up a cool sky tone and surfaces facing down pick up a warm ground bounce (hemisphere term);
- grass keeps its saturation in shade and is lifted a little, as if light passes through leaves (foliage term).

The rewrite is applied to every fragment shader that has the right text (`src/light.inl:2`). The values come from a shared uniform block, so they can be changed live through the reload file.

Shadow edges and filter quality are covered in [shadow-filter.md](shadow-filter.md). This page covers the colour of the result.

## How it works

1. **Master gate.** `LightRewriteFS` returns at once if `[light] Enable` is 0 (`src/light.inl:35`). A forced-fallback bit (16 in `[debug] ForceFallback`) makes it insert a compile error on purpose to test the fallback path (`:36`).
2. **Shadow tint.** The tint needs the engine text `diffuseColour.xyz *= lightReceive;` and a `float shadowAmount` variable (`src/light.inl:38-43`). The shadow-only debug view is added at the same place (`:40-42`). The rewrite replaces the receive line with a call to `TyShade`, or to `TyShadeFoliage` on grass (`:73-74`).
3. **`TyShade` (the shadow colour).** Using the engine's receive term and shadow amount (`src/light.inl:51-56`):
   - the colour is darkened a little more in shadow (`ShadowDepth`, `tyLt[5].w`);
   - it is multiplied by a cool shade colour, weighted by the shadow amount and `ShadowTintAmount` (`tyLt[1]`);
   - saturation is raised in shadow by `ShadowSaturation` (`tyLt[3].w`);
   - a small fill in the sky colour is added in shadow (`ShadowFill`, `tyLt[5].rgb`).
4. **`TyShadeFoliage` (grass).** Grass programs are found by text: they have `uniform float alphaRef;`, no `lightDirMatrix`, and `diffuseColour.w = 1.0;` (`src/light.inl:37`). The foliage version lifts the shade by `FoliageAmount` (`tyLt[4].x`) and tints it less (`:57-63`).
5. **Hemisphere term.** `TyHemi` blends a warm ground colour and a cool sky colour by the vertical component of the normal (`src/light.inl:66-70`). It is applied to the light term just before `diffuseColour *= light;` (`:75-79`). It is applied only where the text has `#if (LIT == 1) || (SHADOWNORMS == 1)`. Static meshes use `HemisphereStatic` as their strength, lit variants use 1 (`:77`). When the PBR rewrite has run, the bumped normal `tyN` is used (`:76`).
6. **Skin and characters.** With `[light] SkipCharacters=1`, programs with `#define SKIN 1` get neither tint nor hemisphere (`src/light.inl:45`). Characters keep the engine shading.
7. **Per-frame values.** `LightFrame` runs once per frame (`src/light.inl:88-119`):
   - it takes the engine fog colour (`fogColour`, captured in `src/atmos.inl:518`) and mixes it into a default sky blue by `TintFromFog` (`src/light.inl:92-99`);
   - it gives the sky colour a cool shift controlled by `ShadeCool` (`:102-104`), and computes a warm complementary ground colour (`:105-106`);
   - the sky colour follows the target at 5 percent per frame (`:100-101`);
   - the amounts are set to zero when `[light] Enable` is off or in the A/B "off" frame (`:109-117`).
8. **Per-program push.** `LightApply` sends the eight `vec4` entries of `tyLt` on `glUseProgram`, once per program per frame (`src/light.inl:120-127`, hook at `src/tyrender.cpp:329`). The declaration is inserted by `EnsureLtDecl` (`src/tyrender.cpp:169-172`).
9. **Block layout.** This is the full layout as used in the code:

| Entry | Components | Meaning | Set at |
|---|---|---|---|
| `tyLt[0]` | x, y, z, w | Filter radius, temporal offset, tap count, probe factor | `src/light.inl:110` |
| `tyLt[1]` | rgb, w | Shade colour, tint amount | `:111`, `:112` |
| `tyLt[2]` | rgb, w | Sky colour, hemisphere amount | `:111`, `:113` |
| `tyLt[3]` | rgb, w | Ground colour, shadow saturation | `:111`, `:114` |
| `tyLt[4]` | x, y, z, w | Foliage amount, static hemisphere, grid flag, crack closing | `:115` |
| `tyLt[5]` | rgb, w | Shadow fill colour, shadow depth | `:116` |
| `tyLt[6]` | x, y, z, w | Plane bias on, debug view on, plane clamp, slope bias | `:117` |
| `tyLt[7]` | x | Skin bias | `:117` |

The comment at `src/tyrender.cpp:165-168` documents only `tyLt[0]` to `tyLt[4]`. Entries 5 to 7 are documented here and in the code only.

## Code pointers

| What | File:line | Note |
|---|---|---|
| Config loader | `src/light.inl:16-32` | All `[light]` keys and the shadow debug keys |
| Rewrite entry | `src/light.inl:34-81` | Gate, flags, insertions |
| Forced fallback | `src/light.inl:36` | `ForceFallback` bit 16 |
| Grass detection | `src/light.inl:37` | Text test |
| Shadow-only view | `src/light.inl:40-42` | `tyLt[6].y` |
| Tint and hemisphere flags | `src/light.inl:43-46` | Including SkipCharacters and DebugWater bit 8 |
| Shade functions | `src/light.inl:49-64` | `TyShade`, `TyShadeFoliage` |
| Hemisphere function | `src/light.inl:65-71` | `TyHemi` |
| Insertion of calls | `src/light.inl:73-79` | Replaces the receive line and the light line |
| Link lookup | `src/light.inl:83-86` | `tyLt` location per program |
| Per-frame block | `src/light.inl:88-119` | Sky smoothing, tint, amounts |
| Per-program push | `src/light.inl:120-127` | `glProgramUniform4fv` of 8 entries |
| Declaration insert | `src/tyrender.cpp:169-172` | `EnsureLtDecl` |
| Link hook | `src/tyrender.cpp:564` | `LightOnLink` from `AfterLink` |
| Fog colour capture | `src/atmos.inl:518` | `fogColour` named uniform |
| Call order | `src/tyrender.cpp:388` | After PBR and shadow patch |
| A/B off frame | `src/light.inl:109` | Amounts forced to zero |

## Settings

Code default (file:line) and shipped value from `src/default_tyrender.ini`. Full tables: [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default (file:line) | Header comment in `light.inl:8` | Shipped | Live or restart | Effect |
|---|---|---|---|---|---|
| `[light] Enable` | 1 (`src/light.inl:18`) | (none) | 1 (`default_tyrender.ini:73`) | Gate is at compile time (inferred); amounts go to zero live (`:109`) | Master switch |
| `[light] ShadowTint` | 1 (`:18`) | (none) | not set | Compile-time gate; amount live | Shadow tint on or off |
| `[light] Hemisphere` | 1 (`:19`) | "Hemisphere=1" | not set | Compile-time gate; amount live | Hemisphere term on or off |
| `[light] Foliage` | 1 (`:19`) | "Foliage=1" | not set | Compile-time gate; amount live | Grass lift on or off |
| `[light] SkipCharacters` | 1 (`:19`) | (none) | not set | Compile-time (inferred) | Characters keep engine shading |
| `[light] ShadowTintAmount` | 1.0 (`:21`) | 0.45 | not set | Live via reload (`tyLt[1].w`) | Strength of the shade colour |
| `[light] TintFromFog` | 0.15 (`:21`) | 0.35 | not set | Live via reload (sky colour) | How much the fog colour sets the shade colour |
| `[light] ShadowSaturation` | 0.0 (`:21`) | 0.25 | not set | Live via reload (`tyLt[3].w`) | Saturation gain in shadow |
| `[light] ShadeCool` | 0.22 (`:21`) | (none) | not set | Live via reload | Cool shift of the shade colour |
| `[light] ShadowDepth` | 0.12 (`:21`) | (none) | not set | Live via reload (`tyLt[5].w`) | Extra darkening in shadow |
| `[light] HemisphereAmount` | 0.10 (`:22`) | 0.30 | not set | Live via reload (`tyLt[2].w`) | Hemisphere strength |
| `[light] HemisphereStatic` | 0.5 (`:22`) | 0.5 | not set | Live via reload (`tyLt[4].y`) | Hemisphere strength on static meshes |
| `[light] FoliageAmount` | 0.10 (`:22`) | 0.5 | not set | Live via reload (`tyLt[4].x`) | Grass lift |
| `[light] ShadowFill` | 0.015 (`:22`) | (none) | not set | Live via reload (`tyLt[5].rgb`) | Sky fill in shadow |
| `[debug] ShadowView` | 0 (`:24`) | (none) | not set | Live via reload (code); comment says restart | Shows only the shadow term |

The header comment column is included to show the conflicts. See Discrepancies.

## Cost

- **Not measured** in the repo. No row for `[light]` in `docs/PERFORMANCE.md`.
- **What scales it:** the number of fragment shaders rewritten and the number of pixels they shade. The rewrite adds a few multiplies and a few more `vec4` uniform reads per pixel. No separate per-frame CPU cost beyond one block build per frame (`src/light.inl:88-119`).

## Limits

- This is a colour model, not light transport. It does not add real bounce light (that is the GI pages), and it does not know which light is which.
- Shade colour comes from the engine fog colour, smoothed. Scenes with unusual fog colours get unusual shade colours (inferred from `src/light.inl:92-99`).
- Grass is detected by three text tests (`src/light.inl:37`). Another alpha-tested program with the same text would be treated as grass (inferred).
- Characters are excluded by default (`SkipCharacters`), so their shading is the engine's.
- The shade colour uses the engine's `lightReceive` and `shadowAmount` names. If a variant uses other names, it is not rewritten.

## Known issues

- **#5 (water "disco ball" on Ty, blue flicker).** The working theory in the plans is that the hemisphere term or the shadow tint contributes. The bisect plan toggles `[light] Hemisphere` first (`docs/PLAN_EXEC.md:40`; `docs/PLAN_CONSOLIDATED.md:38`; `docs/ISSUES_SEED.md:8`). `[pbr] DebugWater` bit 8 switches both off for all draws (`src/pbr.inl:22`, `src/light.inl:46`). Cause: not confirmed in the repo. Status: open in the playtest notes (`CHANGELOG.md:36`).
- **Other than #5,** the search for "tint", "hemisphere" and "foliage" in `README.md`, `CHANGELOG.md` and `docs/` found no open issue against this rewrite. The README lists the `[light]` keys with defaults (`README.md:317`). Shadow edge complaints are on [shadow-filter.md](shadow-filter.md).

## How to verify

1. **Shadow-only view.** Set `[debug] ShadowView=1` and write the reload file. Lit areas should be white and shadows black (`src/light.inl:40`). This shows the shadow term alone, not the tint.
2. **Tint on and off.** Compare the same frame with `[light] ShadowTintAmount=0` and with the default, after the reload file (live, inferred from `src/light.inl:112`). Shadows should turn grey with 0 and cool blue with the default.
3. **Hemisphere on and off.** Set `[light] HemisphereAmount=0` and reload (live, inferred from `src/light.inl:113`). Upward faces should lose their cool tone.
4. **Grass.** Set `[light] FoliageAmount=0` and reload. Grass in shade should go greyer (inferred from `src/light.inl:58-59`).
5. **Log lines.** `tyrender.log` contains `[light] config (...)` once at start-up (`src/light.inl:27`), and `[light] fog=... -> shade tint ...` once when the first fog colour arrives (`:118`).
6. **Rewrite applied.** In the `glShaderSource` log lines, a `LIGHT` tag in `[FS: ...]` means the light rewrite changed that shader (`src/tyrender.cpp:390`).

## NEEDS INVESTIGATION

- **N1. Which compile-time gates really need restart.** `[light] Enable`, `ShadowTint`, `Hemisphere`, `Foliage` and `SkipCharacters` change the text at compile time (`src/light.inl:35-46`). Only the amounts are live (`src/light.inl:109-117`). Method: start with a flag off, enable it after the reload, and see whether the look changes. Expected: no change until restart (inferred).
- **N2. Grass detection coverage.** Method: search the dumps for the three grass text tests, and list the programs flagged `grass` in the log (add a log line, a code change). Not checked here.
- **N3. Engine variant names.** The rewrite assumes `lightReceive`, `shadowAmount` and `diffuseColour *= light;` exist in the engine text (`src/light.inl:38-49`). Method: grep the dumps (`<game folder>\tyrender\dump\*.glsl`) for each name.

## Discrepancies

- **Header defaults versus code defaults** (`src/light.inl:8` against `src/light.inl:18-22`). The header says `ShadowTintAmount=0.45`, `TintFromFog=0.35`, `ShadowSaturation=0.25`, `HemisphereAmount=0.30`, `FoliageAmount=0.5`. The code defaults are 1.0, 0.15, 0.0, 0.10 and 0.10. `docs/SHADERS.md` section 8 uses the code defaults. Code wins. The header is stale.
- **`ShadowView` restart claim.** `src/light.inl:24` says "restart-only". The reload path calls `LoadLightConfig` (`src/tyrender.cpp:598`), and the value feeds a per-frame block (`src/light.inl:110`). Code wins: live.
- **Layout comment is incomplete.** `src/tyrender.cpp:165-168` documents `tyLt[0]` to `tyLt[4]` only. `tyLt[5]` to `tyLt[7]` are used by `src/light.inl:116-117` and `src/tyrender.cpp:245`.

## See also

- [shadow-filter.md](shadow-filter.md): the shadow edges and the `tyShadowStrength` value this page relies on.
- [pbr-surface-detail.md](pbr-surface-detail.md): the bumped normal `tyN` used by the hemisphere term.
- [05-shader-source-rewriting.md](../how-it-works/05-shader-source-rewriting.md): rewrite order and the compile checks.
- [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md): full key tables.
- [debugging.md](../guides/debugging.md): debug views and how to use them.
