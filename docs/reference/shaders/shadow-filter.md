# Shadow filter (cascade shadow patch)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (tester, tunes shadows) and A (contributor).

## Purpose

The engine casts shadows from a cascade of shadow maps. The stock lookup is a small box filter on the first cascade and a single tap on the others (the code comment describes this as "cascade 1 = 2x2 or 4x4 box PCF with hard-coded 1/2048 texel, cascades 2/3 = single tap, shadows darken at most 50%", `src/tyrender.cpp:141`). This page covers the rewrite that replaces those lookups with our own filter, adds a strength control, allows larger shadow maps, and adds two optional tools: a cheaper filter for far cascades and crack closing for thin gaps in casters.

The rewrite only changes how shadow edges are sampled and how dark shadows are. It does not change the shadow maps' contents, the light direction, or the colour of shadowed pixels. The colour of shadowed pixels is covered in [natural-light.md](natural-light.md).

## How it works

1. **Gate.** `PatchShadowShader` runs only on fragment shaders that contain the text `sampler2DShadow shadowTexture1` and a `void main` (`src/tyrender.cpp:175-177`). It is called only when `[shadows] Patch` is 1 (`src/tyrender.cpp:152`, `:388`).
2. **Order.** Inside the fragment branch of `glShaderSource`, the shadow patch runs after the PBR rewrite and before the light rewrite (`src/tyrender.cpp:388`).
3. **Text edits.** The patch makes these changes to the engine text (all are literal string replacements, `src/tyrender.cpp:180-191`):
   - The four stock box-loop headers for cascade 1 are changed so each loop runs once. The sums are then normalised by 1 instead of 16 or 4 (`:180-185`). The PCF work moves into the helper.
   - The cascade 1 lookups are redirected to `TyPCF` (`:186-187`).
   - The cascade 2 and cascade 3 lookups are redirected to `TyPCFL` and receive the far-cascade flag `tyShLod` (`:189-190`).
   - The stock strength literal `shadowAmount * 0.5` becomes `shadowAmount * tyShadowStrength` (`:191`). The literal was `0.5` in the stock shader (`src/tyrender.cpp:553`).
   - Helper code is inserted before `void main`: two uniforms, the explicit-LOD tap `TyS`, and the filters `TyPCFL` and `TyPCF` (`:195-244`).
   - On skinned programs (`#define SKIN 1` present), the receiver-plane bias branch is replaced by an unconditional skin bias (`:245`).
   - The patch returns true if any one replacement matched (`:192`). A partial match therefore still produces a rewrite. See NEEDS INVESTIGATION S1.
4. **Filter, stable grid mode (`[shadows] Grid=1`, default).** Uniform `tyLt[4].z` is 0 in this mode (`src/light.inl:115`), and the shader takes the stable branch (`src/tyrender.cpp:212-232`).
   - Eight bilinear compares are taken on a diagonal cross and an axis cross, at radii of about 1.7 and 1.9 texels (`:214-216`).
   - If all eight agree (sum below 0.001 or above 7.999), the function returns at once. Most pixels stop here (`:217-218`).
   - Otherwise a binomial grid is evaluated. It is 3x3 when `Taps` is below 24 (default 16) and 4x4 when `Taps` is 24 or more (`:224-230`).
   - Crack closing (below) can raise the result (`:221`, `:231`).
5. **Filter, noise mode (`Grid=0`).** The shader uses a rotated four-tap probe with an early-out (`src/tyrender.cpp:233-234`). It then takes `Taps` taps on a Vogel-type spiral with a golden-angle step (`:235-242`). The rotation comes from per-pixel interleaved-gradient noise (`:210`). When TAA is on and `Temporal=1`, the noise offset cycles over 8 frames (`src/light.inl:110`) and the tap count is halved to at least 4 (`:108`). This is meant to be integrated by TAA. With `Temporal=0` the pattern is fixed per pixel.
6. **Far cascades (`[lod] Enable=1` and `[lod] Shadow=1`).** `g_shLod` is set from those two keys (`src/tyrender.cpp:158`) and passed as `tyShLod` (`:559`). For `tyFar > 0.5` the stable branch returns the eight probe taps plus crack closing and skips the grid (`:222`). In noise mode the tap count is halved to at least 4 (`:235`).
7. **Explicit LOD.** Every tap uses `textureLod(..., 0.0)` (`src/tyrender.cpp:202`). The code comment says the implicit LOD was undefined inside non-uniform control flow, which caused the dotted bright line called S1 (`:199-201`).
8. **Crack closing.** If an occluder lies on both opposite sides of a pixel within about two texels, the pixel is kept shadowed (`src/tyrender.cpp:219-221`). The value is `tyLt[4].w`, from `[shadows] CloseCracks` (`src/light.inl:30`). The code comment says this is for the grid filter only (`src/light.inl:30`).
9. **Receiver-plane and slope bias (off by default).** When `[shadows] PlaneBias=1` (`tyLt[6].x`), the shader estimates the depth gradient of the shadow map per UV from screen derivatives (`src/tyrender.cpp:206-209`). Each tap compares against that plane instead of the centre depth. `PlaneClamp` caps the bias (`tyLt[6].z`, `src/light.inl:26`, `:117`). The slope term `SlopeBias` (`tyLt[6].w`) is applied only inside the `PlaneBias` branch (`src/tyrender.cpp:206-209`, nesting; see Discrepancies).
10. **Strength per program.** After each link, `AfterLink` looks up `tyShadowStrength` and `tyShLod` (`src/tyrender.cpp:561-563`). `ApplyShadowStrength` writes the value: `GrassStrength` for programs that use the grass vertex template, otherwise `Strength`. In the A/B "off" frame it writes the stock 0.5 (`src/tyrender.cpp:554-560`, `src/abshot.inl:101-107`).
11. **Map size.** `[shadows] MapScale` is clamped to 1..4 (`src/tyrender.cpp:153`). When the engine allocates a 2048x2048 depth map with no pixel data, the allocation is enlarged by `MapScale` (`src/tyrender.cpp:274`). The shadow-pass viewport of 2048x2048 is scaled the same way when a non-zero framebuffer is bound (`src/tyrender.cpp:255`). The shader reads the real map size with `textureSize()`, so the filter radius adapts (`src/tyrender.cpp:204`).

Uniform values are set per program per frame by `LightApply`, through the shared `tyLt` block (`src/light.inl:120-127`). The block layout is in [natural-light.md](natural-light.md).

```
engine shadow lookup (stock text)
   |  glShaderSource: PatchShadowShader (text edits only)
   v
TyPCF / TyPCFL  --  stable grid (default)  or  noise mode
   |  early-out on fully lit / fully shadowed neighbourhoods
   v
shadowAmount * tyShadowStrength   (per program, from [shadows] Strength or GrassStrength)
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Config loader | `src/tyrender.cpp:150-159` | Reads `[shadows]` Patch, Temporal, Grid, MapScale, Taps, Radius, Strength, GrassStrength and `[lod] Shadow` |
| Patch entry point | `src/tyrender.cpp:174-247` | Detection, text edits, helper insertion |
| Detection text | `src/tyrender.cpp:175` | `sampler2DShadow shadowTexture1` |
| Cascade redirects | `src/tyrender.cpp:186-190` | Cascade 1 to `TyPCF`, cascades 2 and 3 to `TyPCFL` |
| Strength literal | `src/tyrender.cpp:191` | `shadowAmount * 0.5` replaced |
| Helper block | `src/tyrender.cpp:195-244` | `TyS`, `TyPCFL`, `TyPCF` |
| Stable branch | `src/tyrender.cpp:212-232` | Probes, early-out, grid, crack closing |
| Noise branch | `src/tyrender.cpp:233-242` | Rotated probe, spiral taps |
| Skin variant | `src/tyrender.cpp:245` | Skinned programs |
| Call order | `src/tyrender.cpp:388` | PBR, shadow, light, decal, emit |
| Strength per program | `src/tyrender.cpp:554-560` | Grass, stock value in A/B off frame |
| Link hook | `src/tyrender.cpp:561-563` | Locations of `tyShadowStrength` and `tyShLod` |
| Map enlargement | `src/tyrender.cpp:274` | Depth map allocation |
| Shadow viewport | `src/tyrender.cpp:255` | Viewport scale |
| Per-frame block | `src/light.inl:88-119` | `tyLt[0]`, `[4]`, `[6]`, `[7]` for this filter |
| Debug and bias keys | `src/light.inl:24-30` | ShadowView, PlaneBias, PlaneClamp, SlopeBias, SkinBias, CloseCracks |
| Grass detection | `src/tyrender.cpp:146`, `:376` | Grass vertex template, used for GrassStrength |
| A/B stock strength | `src/abshot.inl:101-107` | Off frame |

## Settings

Code defaults are the values passed to `GetPrivateProfile*` (file:line given). "Shipped" is `src/default_tyrender.ini`. Keys not listed there use the code default. "Live" means the value takes effect after the reload file (`src/tyrender.cpp:598`) or each frame. "Restart" means it is used once at start-up or at shader compile. Full tables are in [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default (file:line) | Shipped (`default_tyrender.ini`) | Live or restart | Effect |
|---|---|---|---|---|
| `[shadows] Patch` | 1 (`tyrender.cpp:152`) | 1 (`:46`) | Restart (compile-time gate, inferred) | Enables the rewrite |
| `[shadows] Grid` | 1 (`:152`) | not set | Live via reload (inferred from `light.inl:115`) | 1 = stable grid; 0 = noise mode |
| `[shadows] Temporal` | 0 (`:152`) | not set | Live via reload (inferred from `light.inl:110`) | Noise animation over 8 frames, only in noise mode with TAA |
| `[shadows] MapScale` | 2 (`:153`), clamped 1..4 | 2 (`:47`) | Restart (allocation at start-up, inferred) | Cascade map size = 2048 x MapScale |
| `[shadows] Taps` | 16 (`:154`), clamped 4..32 | 16 (`:48`) | Docs say restart; code suggests live (see Discrepancies) | Tap count. Below 24: 3x3 grid. 24 or more: 4x4 grid |
| `[shadows] Radius` | 1.4 (`:155`) | not set | Live via reload | Filter radius in shadow-map texels (`tyLt[0].x`) |
| `[shadows] Strength` | 0.5 (`:156`) | not set | Live via reload | Shadow darkness multiplier (replaces stock 0.5) |
| `[shadows] GrassStrength` | 0.35 (`:157`) | not set | Live via reload | Same for grass programs |
| `[lod] Shadow` (with `[lod] Enable`) | 0 (`:158`) | 0 (`default_tyrender.ini:25`) | Live via reload | Far cascades use the 8-probe filter |
| `[shadows] PlaneBias` | 0 (`light.inl:25`) | not set | Live via reload | Receiver-plane depth bias (off) |
| `[shadows] PlaneClamp` | 0.004 (`light.inl:26`) | not set | Live via reload | Cap on the plane bias |
| `[shadows] SlopeBias` | 0 (`light.inl:28`) | not set | Live via reload | Scales the bias. Only active when PlaneBias=1 |
| `[shadows] SkinBias` | 0 (`light.inl:29`) | not set | Live via reload | Bias on skinned programs (`tyLt[7].x`) |
| `[shadows] CloseCracks` | 0 (`light.inl:30`) | not set | Live via reload | Crack closing strength (`tyLt[4].w`) |
| `[debug] ShadowView` | 0 (`light.inl:24`) | not set | Live via reload (code; comment says restart, see Discrepancies) | Shows the shadow term alone |

## Cost

- **Measured (v0.3 measurements, docs/PERFORMANCE.md, RX 7600 XT, 2560x1440 windowed, vsync off):**
  - `Shadow maps 8192 x32 taps` about +5.1 ms over Medium (`docs/PERFORMANCE.md:56`).
  - `Shadow grid filter (v0.3.2 default) vs old filter` about +0.7 ms (`docs/PERFORMANCE.md:58`, and `:79` for `[shadows] Grid`).
  - `[shadows] MapScale 4 / Taps 32` about 5 ms more than Medium (`docs/PERFORMANCE.md:77`).
  - The measurements are labelled "v0.3" in the table (`docs/PERFORMANCE.md:46`), while the page title says "measured on v0.3.2" (`docs/PERFORMANCE.md:1`). See Discrepancies.
- **Not measured:** the stable-grid early-out, the far-cascade filter, and crack closing as separate items. An older session snapshot (`docs/RESUME.md:17`) reports the 8-tap early-out probe as +0.02 ms. This was not re-measured.
- **What scales it:** the tap count (`Taps`), the grid size (3x3 or 4x4), the map size (`MapScale`, which affects memory and cache use), and the share of pixels in penumbras (the early-out skips the rest).

## Limits

- The filter only sees what is in the engine's shadow maps. Gaps in casters that the maps record as gaps are handled only by crack closing, which is a local heuristic (`src/tyrender.cpp:219-221`).
- The rewrite depends on the exact text of the engine's stock shadow code. Any replacement that does not match is silently skipped (`src/tyrender.cpp:180-191`). No log line reports which replacements matched (see NEEDS INVESTIGATION S1).
- Receiver-plane bias uses screen derivatives of the shadow coordinate. These are noisy on very small skinned triangles, so the bias is switched off for skinned programs (`src/tyrender.cpp:245`, comment).
- Explicit LOD 0 means no mipmapping of the shadow map. This is intended; the comment gives the reason (`src/tyrender.cpp:199-202`).
- Far-cascade LOD (`tyShLod`) applies only to cascades 2 and 3. Cascade 1 always uses the full filter (`src/tyrender.cpp:186-190`).
- The filter does not change the penumbra size of the engine's light. The radius is a fixed number of texels (`tyLt[0].x`).
- Tested on one GPU only (`docs/PERFORMANCE.md:3`).

## Known issues

- **S1 (dotted bright line and checkerboard inside shadows).** Cause, as stated in the code: implicit derivatives inside non-uniform control flow. Fix: explicit LOD 0 (`src/tyrender.cpp:199-202`). Listed as fixed in v0.3.4 (`docs/PLAN_EXEC.md:8`).
- **#11 (checkerboard / dotted line).** Closed on 2026-10-16 after the owner's Medium playtest of v0.3.4 confirmed the lines on character shadows are fixed. High and Ultra were not yet played (`docs/PLAN_CONSOLIDATED.md:13`, `:41`).
- **#103 (sharp shadow line on the bridge near the fire).** Open (`README.md:215`; also `CHANGELOG.md:36` playtest note).
- **Character shadow lines.** A thin bright line can run through the shadow edge on characters (face, bag), and the shadow looks slightly grid-like (`README.md:217`).
- **Faint checkerboard.** Character and grass shadows still show a faint checkerboard pattern in the current build notes (`CHANGELOG.md:63`, "Known").
- **T75 (planned).** Texel-aligned noise, rotation period matched to the texel grid, depth-weighted upsample, and slope and receiver-plane bias (`docs/FIX_PLAN_20261016.md:21`). Status: planned. Note that PlaneBias, SlopeBias and CloseCracks already exist as off-by-default options (`src/light.inl:25-30`).
- **Ultra MapScale=4.** Verification item only (`docs/ISSUES_SEED.md:14`).

## How to verify

1. **Shadow term alone.** Set `[debug] ShadowView=1`, then create the reload file in `<game folder>\tyrender\` (see [hotkeys-and-files.md](../reference/hotkeys-and-files.md)). Lit areas should be white and shadowed areas black (`src/light.inl:40`). Hard, clean edges are expected. Dotted lines or noise inside shadows are the things to report.
2. **Rewrite applied.** In `tyrender.log`, look for `glShaderSource shader=<n> hash=<h> [FS: ... SHADOW ...]` (format at `src/tyrender.cpp:390`). The tag SHADOW means the patch changed that shader.
3. **Strength.** Set `[shadows] Strength=0`, then reload. Shadow darkening on props should disappear (inferred from `src/tyrender.cpp:191`, `:558`).
4. **Map size.** At start-up, `tyrender.log` contains `[shadows] shadow map enlarged to WxH` when `MapScale` is above 1 (`src/tyrender.cpp:274`). The expected sizes are 4096 for MapScale 2 and 8192 for MapScale 4.
5. **Noise versus grid.** Set `[shadows] Grid=0` and compare penumbras with `Grid=1`. Noise should appear as grain at the edges. This is an inferred expectation from the code, not a checked result.
6. **Far LOD.** Set `[lod] Enable=1` and `[lod] Shadow=1`, then reload. Cascade 2 and 3 edges should look slightly coarser at distance (inferred).

## NEEDS INVESTIGATION

- **S1. Which replacement patterns match in which engine shaders.** The patch silently skips patterns that do not match (`src/tyrender.cpp:180-191`). Method: run with `[debug] DumpShaders=1` (default) and search the dumps in `<game folder>\tyrender\dump\` for each pattern (`grep -l` per pattern). Or add a per-pattern counter to `PatchShadowShader` (a code change, not part of this docs task).
- **S2. Shadow map size and cascade count.** The code assumes 2048x2048 maps (`src/tyrender.cpp:255`, `:274`) and three cascades (the samplers `shadowTexture1..3`, `src/tyrender.cpp:175`, `:189-190`). Method: capture a frame (`cap.req`, see [debugging.md](../guides/debugging.md)) and read the `glTexImage2D` lines. Also see N-22 in the master plan.
- **S3. Restart or live for `Taps`.** Docs say restart (`docs/START_HERE.md:70`, `docs/PERFORMANCE.md:85`). Code suggests live (`src/light.inl:108-110`, `src/tyrender.cpp:598`). Method: change `Taps` in-game, write the reload file, and check whether the edge changes and whether the frame time changes.
- **S4. Stock constants.** The comment at `src/tyrender.cpp:141-143` describes the stock filter (texel size, cascade behaviour, 50% limit). Method: read the dumped stock shader and confirm.

## Discrepancies

- `docs/START_HERE.md:70` and `docs/PERFORMANCE.md:85` say `[shadows] Taps` is restart-only. The code path reads it on reload (`src/tyrender.cpp:598` calls `LoadShadowConfig`) and uses it in a per-frame block (`src/light.inl:110`). Code wins: treated as live (inferred). Test: S3.
- `README.md:300` gives MapScale as 1 = 2048, 2 = 4096, 4 = 8192. The code accepts 1 to 4 (`src/tyrender.cpp:153`), so 3 gives 6144 with the multiplication at `src/tyrender.cpp:274`. The README does not list 3.
- `docs/SHADERS.md` (first pass, section 8) lists PlaneBias, SlopeBias and CloseCracks as "planned" in the Known issues entry. The code has them as off-by-default options (`src/light.inl:25-30`). Code wins.
- `src/light.inl:24` says `ShadowView` is restart-only. The reload path calls `LoadLightConfig` (`src/tyrender.cpp:598`), so it is live. Code wins.
- `src/tyrender.cpp:193` says "4-tap probe ... 16 depth fetches". That describes the noise branch (`:233`). The stable branch uses eight probes (`:214-216`).
- The comment at `src/light.inl:30` says CloseCracks works on the grid filter only. The code applies it in the stable grid branch and in the far-cascade branch (`src/tyrender.cpp:221-222`). Code wins: far-cascade use is also affected (inferred).

## See also

- [natural-light.md](natural-light.md): the colour of shadowed pixels and the `tyLt` block.
- [pbr-surface-detail.md](pbr-surface-detail.md): the PBR rewrite that runs before this one.
- [distance-lod.md](distance-lod.md): the `[lod] Shadow` switch.
- [05-shader-source-rewriting.md](../how-it-works/05-shader-source-rewriting.md): how and when the text edits run.
- [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md): full key tables.
- [known-issues.md](../decisions/known-issues.md): the open list.
