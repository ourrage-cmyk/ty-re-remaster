# Shader inventory

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: C (maintainer), A (contributor).

## Purpose

This page lists every shader string, shader rewrite and runtime debug switch that the renderer owns, with its kind, file:line, GLSL version, the ini key that gates it, and its cost where the repo measures one. It is the index for the per-effect pages in this folder. The passes that run these shaders are described in [frame anatomy](../how-it-works/04-frame-anatomy-and-pass-order.md); the render targets they read and write are in [render targets](../how-it-works/06-render-targets-depth-and-state.md).

Two kinds of shader exist. Our own shaders are C++ raw string literals compiled by `CompileProg`. Rewrites are edits made to the game's shader source inside the `glShaderSource` hook, so they have no literal of their own (tyrender.cpp:388-393).

## How it works

### 1. Kinds

| Kind | Meaning | Where it is set |
|---|---|---|
| fullscreen | vertex stage `kVS` plus one fragment stage; draws a screen triangle or quad into an RT | ao.inl:95, gi.inl:133-135, gi2.inl:354-361, atmos.inl:365, fx.inl:181-183 |
| compute | `GL_COMPUTE_SHADER` (0x91B9) | atmos.inl:160 (`#version 430`), compiled at atmos.inl:265 |
| tessellation | generated TCS and TES attached at link time (GL_PATCHES) | tess.inl:177-379, linked at tess.inl:392-430 |
| rewrite | an edit to a game shader at `glShaderSource` or link time | tyrender.cpp:388-393 and the functions listed in section 2 |

### 2. Inventory

GLSL version is the `#version` line the source carries, or the one the caller prepends (marked "prefix", with the call line). "Gate" is the ini key that turns the item on. "Cost" is the measured row from `docs/results/cost_table.md` that matches the feature, or NI. Every item in the checklist at `_DOCS_PLAN.md` section 3.1 has a row.

| Name | Kind | file:line | GLSL | Gate | Cost | Page |
|---|---|---|---|---|---|---|
| kVS (shared vertex stage) | fullscreen VS | ao.inl:95 | 330 (own line) | none; used by all fullscreen passes | part of each pass | this page |
| kCommon (depth to world/view helpers) | fragment part | ao.inl:101 | none; prefix at caller (ao.inl:538) | `[ao] Enable` (ao.inl:492) | in AO | [ao-gtao](ao-gtao.md) |
| kGTAO (legacy AO) | fullscreen FS | ao.inl:125 | 330 (prefix, ao.inl:538) | `[ao] Legacy=1` (ao.inl:506) | NI | [ao-gtao](ao-gtao.md) |
| kPre0 (depth mip 0) | fullscreen FS | ao.inl:184 | 330 (own line) | `[ao] Enable` | NI | [ao-gtao](ao-gtao.md) |
| kPreN (depth mip N) | fullscreen FS | ao.inl:208 | 330 (own line) | `[ao] Enable` | NI | [ao-gtao](ao-gtao.md) |
| kGTAO2 (horizon search) | fullscreen FS | ao.inl:221 | 330 (own line) | `[ao] Enable`, `Legacy=0` | ao_off row, see below | [ao-gtao](ao-gtao.md) |
| kTemporal (AO) | fullscreen FS | ao.inl:320 | 330 (prefix, ao.inl:541) | `[ao] Enable` | NI | [ao-gtao](ao-gtao.md) |
| kBlur (AO) | fullscreen FS | ao.inl:378 | 330 (own line) | `[ao] Enable` | NI | [ao-gtao](ao-gtao.md) |
| kComposite (AO) | fullscreen FS | ao.inl:395 | 330 (prefix, ao.inl:540) | `[ao] Enable` | ao_off row, see below | [ao-gtao](ao-gtao.md) |
| kGI (linear march, legacy GI) | fullscreen FS | gi.inl:12 | 330 (prefix, gi.inl:133) | `[gi] Enable` (gi.inl:91), `Mode` (gi.inl:95) | NI | [gi-legacy](gi-legacy.md) |
| kGITemporal (legacy GI) | fullscreen FS | gi.inl:42 | 330 (prefix, gi.inl:134) | `[gi] Enable` | NI | [gi-legacy](gi-legacy.md) |
| kGIComposite (legacy GI) | fullscreen FS | gi.inl:60 | 330 (prefix, gi.inl:135) | `[gi] Enable` | NI | [gi-legacy](gi-legacy.md) |
| kGIC2 (Hi-Z character prefix) | GLSL fragment part, prepended | gi2.inl:98 | none; prepended to Hi-Z bodies (gi2.inl:356) | `[gi] Enable`, `Mode` | NI | [gi-hiz](gi-hiz.md) |
| kGIPyr0 (depth pyramid level 0, 1/z) | fullscreen FS | gi2.inl:104 | 330 (prefix, gi2.inl:354) | `[gi] Enable`, `Mode` | NI | [gi-hiz](gi-hiz.md) |
| kGIPyrDown (pyramid downsample) | fullscreen FS | gi2.inl:113 | 330 (prefix, gi2.inl:354) | `[gi] Enable`, `Mode` | NI | [gi-hiz](gi-hiz.md) |
| GI_CHAR_GLSL (macro, character mask helpers) | GLSL macro | gi2.inl:125 | none; included at gi2.inl:134 and gi2.inl:314 | `[gi] Enable`, `Mode` | NI | [gi-hiz](gi-hiz.md) |
| kGITrace2 (Hi-Z trace, MRT) | fullscreen FS | gi2.inl:134 | 330 (prefix, gi2.inl:360) | `[gi] Enable`, `Mode`, `HiZIter` (gi.inl:95) | gi_hiz1 row, see below | [gi-hiz](gi-hiz.md) |
| kGITemporal2 (Hi-Z temporal) | fullscreen FS | gi2.inl:230 | 330 (prefix, gi2.inl:360) | `[gi] Enable`, `Mode` | NI | [gi-hiz](gi-hiz.md) |
| kGIAtrous (a-trous filter) | fullscreen FS | gi2.inl:265 | 330 (prefix, gi2.inl:361) | `[gi] Enable`, `Mode`, `Atrous` (gi.inl:95) | NI | [gi-hiz](gi-hiz.md) |
| kGIComposite2 (Hi-Z composite) | fullscreen FS | gi2.inl:314 | 330 (prefix, gi2.inl:361) | `[gi] Enable`, `Mode` | NI | [gi-hiz](gi-hiz.md) |
| kAtmos (inscatter, fragment path) | fullscreen FS | atmos.inl:14 | 330 (prefix, atmos.inl:365) | `[atmos] Enable` (atmos.inl:292) | atmos_off row, see below | [atmosphere-fog](atmosphere-fog.md) |
| kAtmosCS (froxel volume) | compute | atmos.inl:160 | 430 (own line) | `[atmos] Froxel` (atmos.inl:304, default 1) | NI | [atmosphere-froxel-compute](atmosphere-froxel-compute.md) |
| kSkyEst (1x1 sky estimate) | fullscreen FS | atmos.inl:219 | 330 (prefix, atmos.inl:418) | `[atmos] Enable` | NI | [sky-estimate-and-haze-color](sky-estimate-and-haze-color.md) |
| kEmitBlur (emitter blur) | fullscreen FS | emit.inl:64 | 330 (own line) | `[emit] Enable` (emit.inl:34, default 0) | NI | [emitter-light](emitter-light.md) |
| kEmitComp (emitter composite) | fullscreen FS | emit.inl:76 | 330 (own line) | `[emit] Enable` | NI | [emitter-light](emitter-light.md) |
| kEmitDepth (emitter depth) | fullscreen FS | emit.inl:82 | 330 (own line) | `[emit] Enable` | NI | [emitter-light](emitter-light.md) |
| kFXAA | fullscreen FS | fx.inl:8 | 330 (own line) | `[post] Enable` (fx.inl:148), `FXAA` (fx.inl:148), `AA` (fx.inl:149) | NI | [taa-fxaa](taa-fxaa.md) |
| kGrade (colour grade) | fullscreen FS | fx.inl:57 | 330 (own line) | `[post] Enable`, `Tonemap` (fx.inl:155) | NI | [grade-and-sharpen](grade-and-sharpen.md) |
| kTAA | fullscreen FS | fx.inl:99 | 330 (own line) | `[post] AA=2` or `3` (fx.inl:149) | taa_off row, see below | [taa-fxaa](taa-fxaa.md) |
| kTyHeightGLSL (height field helper) | GLSL snippet | tess.inl:163 | none; included in generated TES at tess.inl:312 | `[tess] Enable` (tess.inl:35) | tess_off row, see below | [tessellation and relief](tessellation-and-relief.md) |
| generated TCS (world) | tessellation control | tess.inl:260 | 400 (own line) | `[tess] Enable` | tess_off row, see below | [tessellation and relief](tessellation-and-relief.md) |
| generated TCS (skinned, legacy) | tessellation control | tess.inl:177 | 400 (own line) | `[tess] Enable` | tess_off row, see below | [tessellation and relief](tessellation-and-relief.md) |
| generated TES (world) | tessellation evaluation | tess.inl:310 | 400 (own line) | `[tess] Enable` | tess_off row, see below | [tessellation and relief](tessellation-and-relief.md) |
| generated TES (skinned, legacy) | tessellation evaluation | tess.inl:206 | 400 (own line) | `[tess] Enable` | tess_off row, see below | [tessellation and relief](tessellation-and-relief.md) |
| TessRewriteVS (VS output rename) | rewrite | tyrender.cpp:566 (calls RewriteVSForTess) | inherits the game's VS version (inferred) | `[tess] Enable` | tess_off row, see below | [tessellation and relief](tessellation-and-relief.md) |
| PatchShadowShader (shadow filter) | rewrite | tyrender.cpp:174 | inherits the game's FS version (inferred) | `[shadows] Patch` (tyrender.cpp:152, default 1) | NI | [shadow-filter](shadow-filter.md) |
| LightRewriteFS (tint, hemisphere, foliage) | rewrite | light.inl:34 | inherits the game's FS version (inferred) | `[light] Enable` (light.inl:18) | NI | [natural-light](natural-light.md) |
| PbrRewriteVS | rewrite | pbr.inl:48 | inherits the game's VS version (inferred) | `[pbr] Enable` (pbr.inl:18) | NI | [pbr-surface-detail](pbr-surface-detail.md) |
| PbrRewriteFS | rewrite | pbr.inl:57 (raw block at pbr.inl:73) | inherits the game's FS version (inferred) | `[pbr] Enable` | NI | [pbr-surface-detail](pbr-surface-detail.md) |
| DecalRewriteFS | rewrite | tex.inl:248 | inherits the game's FS version (inferred) | `[tex] Enable` (tex.inl:67) | NI | [texture names and tags](../how-it-works/07-texture-names-and-tags.md) |
| EmitRewriteFS | rewrite | emit.inl:47 | inherits the game's FS version (inferred) | `[emit] Enable` (emit.inl:34) | NI | [emitter-light](emitter-light.md) |
| FogOverride (engine fog, uniform rewrite) | uniform rewrite, no shader text | tyrender.cpp:404 | n/a | `[atmos]` fog keys | NI | [atmosphere-fog](atmosphere-fog.md) |

Cost rows that exist. `docs/results/cost_table.md` reports frame ms for the Balanced (C1) build at the default view, with the feature turned off, compared to C1 at 11.09 ms:

| Variant | Frame ms | vs C1 | Feature in the table |
|---|---|---|---|
| ao_off | 9.94 | -1.15 | AO passes (kGTAO2, kComposite) |
| atmos_off | 9.00 | -2.09 | kAtmos, kAtmosCS |
| tess_off | 11.04 | -0.05 | tessellation |
| taa_off | 11.49 | +0.40 | kTAA |
| gi_hiz1 | 15.51 | +4.42 | GI Hi-Z with one ray (a setting, not an off switch) |

The taa_off row is higher than C1, which is the opposite sign to a pass removal. It is recorded as measured; the cause is NI-Ch03-S1 (below).

### 3. Debug views

| Name | Kind | Set by | file:line | Effect |
|---|---|---|---|---|
| ao_debug 1 / 2 / 3 | flag file | `tyrender\ao_debug` (read at ao.inl:521) | ao.inl:5 | 1 = AO only, 2 = normals, 3 = view depth |
| [ao] DebugView | ini key | `[ao] DebugView` | ao.inl:515 | initial value of the AO debug view |
| gi_debug 1 | flag file | `tyrender\gi_debug` (read at gi.inl:101) | gi.inl:6 | 1 = GI only; uniform `u_debug` at gi.inl:82 |
| [gi] DebugView | ini key | `[gi] DebugView` | gi.inl:95 | initial value; 3 shows the GI mask (gi2.inl:33) |
| atmos_debug 1 | flag file | `tyrender\atmos_debug` (read at atmos.inl:312) | atmos.inl:6 | 1 = inscatter only; the half-res path is skipped while set (atmos.inl:481) |
| [pbr] Debug | ini key | `[pbr] Debug` | pbr.inl:18 | PBR debug output |
| [tess] Debug | ini key | `[tess] Debug` | tess.inl:35 | tessellation debug output |
| [tex] Debug | ini key | `[tex] Debug` | tex.inl:67 | texture tag debug output |
| [debug] ShadowView | ini key, restart only | `[debug] ShadowView` | light.inl:24 | 1 = shadow term alone (white lit, black shadow) |

Debug views are test aids. They change output and are not part of the normal render.

### 4. Uniform block for rewrites

Rewrites share one `vec4` array named `tyLt`. The declaration is `uniform vec4 tyLt[8];` (tyrender.cpp:171). Shader code reads indices 0 to 7; for example `tyLt[7].x` at tyrender.cpp:245. The layout comments at tyrender.cpp:166-168 list indices 0 to 4. Index 6 carries the shadow-patch switches (tyrender.cpp:206) and the per-program light values are set by `LightApply` (light.inl). See [natural-light](natural-light.md) for the values.

### 5. Version rules

- Fullscreen passes use GLSL 330 (kVS ao.inl:95; every fullscreen FS listed above that has a `#version` line or prefix).
- Tessellation generated shaders use GLSL 400 (tess.inl:179, 207, 260, 310).
- The froxel compute shader uses GLSL 430 (atmos.inl:160).
- Rewrites keep the game's version. This page does not verify the game's own version strings (NI-Ch03-S2).

## Parameters

Gate keys used in the table, with the default in the code:

| Key | Default | file:line |
|---|---|---|
| `[ao] Enable` | 1 | ao.inl:492 |
| `[ao] Legacy` | 0 | ao.inl:506 |
| `[gi] Enable` | 1 | gi.inl:91 |
| `[gi] Mode` | 1 | gi.inl:95 |
| `[atmos] Enable` | 1 | atmos.inl:292 |
| `[atmos] Froxel` | 1 | atmos.inl:304 |
| `[atmos] FroxelPixels` | 12, clamped 4 to 64 | atmos.inl:304 |
| `[emit] Enable` | 0 | emit.inl:34 |
| `[post] Enable` | 1 | fx.inl:148 |
| `[post] AA` | 2 | fx.inl:149 |
| `[tess] Enable` | 1 | tess.inl:35 |
| `[tex] Enable` | 1 | tex.inl:67 |
| `[light] Enable` | 1 | light.inl:18 |
| `[pbr] Enable` | 1 | pbr.inl:18 |
| `[shadows] Patch` | 1 | tyrender.cpp:152 |

Full key list: see the settings reference, not this page.

## Known issues

- The shadow-patch, light, PBR, decal and emitter rewrites all run inside one `glShaderSource` call (tyrender.cpp:388). Their order matters because the decal and emitter rewrites both edit the same output line (tyrender.cpp:388 comment). A new rewrite must be checked against that order.
- A rewrite that fails `TestCompile` is dropped and the original source is used (tyrender.cpp:393; tess.inl path at tyrender.cpp:566-571 restores `orig`).

## How to verify

- Inventory check: `grep -n 'R"' src/*.inl src/tyrender.cpp` and `grep -n '#version' src/*.inl` in the repo. Every hit should appear in section 2 or 3. The check was run at 4db1d7d; the result is the table above.
- Runtime: the log line `glShaderSource shader=... [FS:PBR SHADOW LIGHT DECAL]` (tyrender.cpp:390) lists which rewrites a given fragment shader received.
- Debug views: set the flag file or ini key from section 3 and reload.

## Discrepancies

- **tyLt[8] vs tyLt[6].** The declaration is `tyLt[8]` (tyrender.cpp:171) and code uses indices up to 7 (tyrender.cpp:245). The layout comment at tyrender.cpp:166-168 lists 0 to 4, and light.inl:7 calls the block `tyLt[6]`. The code is consistent (no index past 7 is used); the comments and the light.inl wording are out of date. Not a runtime defect.
- **Debug switch name.** `_DOCS_PLAN.md` section 3.1 names `[shadows]ShadowView`. The code key is `[debug] ShadowView` (light.inl:24). The plan item should be corrected.
- **taa_off cost sign.** `docs/results/cost_table.md` shows taa_off at +0.40 ms versus C1 (see section 2). Turning TAA off should not add time on its own. Recorded as measured.
- **Plan list incomplete for debug views.** The plan's debug list omits `atmos_debug` and `gi_debug` file names; both are listed in section 3 above.

## Not verified

- NI-Ch03-S1: why taa_off is slower than C1 in cost_table.md. Method: rerun the taa_off variant with the GProf marks (see [frame anatomy](../how-it-works/04-frame-anatomy-and-pass-order.md)) and compare mark 15 (RunFX) between the two builds.
- NI-Ch03-S2: the GLSL version of the game's own shader strings that rewrites edit. Method: dump them with `tyrender\dump` (tyrender.cpp:131-143) and read the first line of each dump.
- NI-Ch03-S3: per-shader ms costs for every row marked NI. Method: GProf marks per pass as in section 4 of the frame anatomy page; the repo has no per-shader table.
- NI-Ch03-S4: the exact GI Mode value that selects the legacy path versus Hi-Z. The branch at gi.inl:120-129 was not read in full for this page; the fallback condition at gi.inl:130 is read.

## See also

- [Frame anatomy and pass order](../how-it-works/04-frame-anatomy-and-pass-order.md)
- [Render targets, depth and state](../how-it-works/06-render-targets-depth-and-state.md)
- [Fallbacks and failure handling](../how-it-works/11-fallbacks-and-failure-handling.md)
