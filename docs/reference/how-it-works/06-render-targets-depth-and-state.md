# Render targets, depth and state

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), C (maintainer).

## Purpose

This page lists every texture and framebuffer the renderer creates, the format and size of each, and how the passes read depth and rebuild position and normals from it. It also explains how camera matrices and the TAA jitter are captured, how the renderer copes with a change of window size, and how each pass saves and restores GL state so the game's own draws are not disturbed. The frame order these targets sit in is on [frame anatomy](04-frame-anatomy-and-pass-order.md).

## How it works

### 1. Engine targets (observed, not allocated by us)

The game owns the default framebuffer FB0 and its colour and depth textures. The renderer sees them only through hooks:

| Target | How seen | file:line | Format and size |
|---|---|---|---|
| FB0 colour | `glCopyTexImage2D` RGB or RGBA, level 0 | tyrender.cpp:284-285 | copy of the window; RGB/RGBA as the game chose |
| FB0 depth copy | `glCopyTexImage2D` DEPTH_COMPONENT (0x1902) or 16/24/32 (0x81A5-7) | tyrender.cpp:286-287 | size of the copy; becomes `g_sceneDepthTex` |
| Shadow maps | `glTexImage2D`, internal format 0x81A7 (DEPTH_COMPONENT32), 2048 by 2048 | tyrender.cpp:274 | enlarged by `[shadows] MapScale` when `Patch=1` (default 2, so 4096 by 4096; default_tyrender.ini:47) |
| Post RTs (1024 by 512) | not seen by hooks | docs/START_HERE.md:85 | NEEDS INVESTIGATION (NI-Ch03-8) |

The renderer logs every `glTexImage2D` that has no pixel data, so RT allocations can be read from `tyrender.log` (tyrender.cpp:276). It also logs the first three frames of copies (tyrender.cpp:281).

Whether FB0 is multisampled depends on the game's own settings (see MSAA below).

### 2. Renderer targets

All sizes use `w` by `h` for the window and `hw` by `hh` for the half-resolution size (w/2 by h/2, rounded down). The formats are GL internal-format enums. "Half" is `hw` by `hh`.

| Owner (pass) | Name | Size | Format | Count | Created at | Recreated on resize |
|---|---|---|---|---|---|---|
| AO | `ao.texA`, `ao.texB` | half | RG16F (0x822F) | 2 | ao.inl:551 | yes; old ids not deleted (comment at ao.inl:551) |
| AO | `ao.texH[2]` (history) | half | RGBA16F (0x881A) | 2 | ao.inl:552 | same |
| AO | `ao.texD` (linear view depth) | half, 5 levels (0 to 4) | R32F (0x822E) | 1 | ao.inl:554-556 | same |
| AO | `ao.fbo`, `ao.vao` | | | 1 each | ao.inl:548 | shared with legacy GI, atmosphere (gi.inl:137, atmos.inl:368) |
| GI Hi-Z | `g2.texHz` (depth pyramid) | full, levels 0 to `g2.levels`-1 | R32F | 1 | gi2.inl:371-374 | yes; deleted first (gi2.inl:367-369) |
| GI Hi-Z | `g2.texRaw`, `g2.texGB` | half | RGBA16F | 2 | gi2.inl:375 | same |
| GI Hi-Z | `g2.texHC[2]`, `g2.texPing[2]` | half | RGBA16F | 4 | gi2.inl:376 | same |
| GI Hi-Z | `g2.texHM[2]` (moments) | half | RGBA32F (0x8814) | 2 | gi2.inl:376 | same |
| GI Hi-Z | `g2.texMask` (source mask copy) | half | RGBA8 (0x8058) | 1 | gi2.inl:415-417, created when a mask exists | when size changes |
| GI Hi-Z | `g2.fbo` | | | 1 | gi2.inl:364 | |
| GI legacy | `gi.texRaw`, `gi.texH[2]` | half | RGBA16F | 3 | gi.inl:143-144 | yes; old ids not deleted (inferred leak) |
| Emitter | `g_emitTex[2]` (emission buffer) | half | RGBA16F | 2 | emit.inl:116 | yes; deleted first (emit.inl:114) |
| Emitter | `g_emitDepthFull` | full | DEPTH_COMPONENT24 (0x81A6) | 1 | emit.inl:120 (`MakeDepthTex`, emit.inl:93-98) | same |
| Emitter | `g_emitDepthHalf` | half | DEPTH_COMPONENT24 | 1 | emit.inl:120 | same |
| Emitter | `g_emitFbo`, `g_emitVao` | | | 1 each | emit.inl:110 | |
| Atmosphere | `atm.tex` (private copy of FB0) | full | RGBA8 | 1 | atmos.inl:377-378 | yes; old id not deleted (inferred leak) |
| Atmosphere | froxel volume `vol.tex` | 3D, `ceil(w/FP)` by `ceil(h/FP)` by 64 (FP = `[atmos] FroxelPixels`, default 12, clamped 4 to 64) | RGBA16F | 1 | atmos.inl:273-283 | yes; deleted first (atmos.inl:276) |
| Atmosphere | `atm.low` (half-res fog, non-froxel path only) | half | RGBA16F | 1 | atmos.inl:484 | yes; old id not deleted (inferred leak) |
| Atmosphere | `skyE.tex[2]` (sky colour estimate) | 1 by 1 | RGBA16F | 2 | atmos.inl:421 | no |
| Post | `fx.texScene`, `fx.texTmp` | full | RGBA8 | 2 | fx.inl:191 (`MakeTex8`, fx.inl:169-175) | yes; old ids not deleted (inferred leak) |
| Post | `fx.texHist[2]` (TAA history) | full | RGBA16F | 2 | fx.inl:193 | same |
| Post | `fx.fbo` | | | 1 | fx.inl:185 | |

Pixel count reference at 2560 by 1440 (computed from the formats, not measured; drivers may add padding): half RG16F about 3.7 MB; half RGBA16F about 7.4 MB; half RGBA32F about 14.7 MB; full RGBA8 about 14.7 MB; full RGBA16F about 29.5 MB; full R32F pyramid about 19.7 MB; froxel volume at 2560 by 1440 is 214 by 120 by 64 RGBA16F, about 13.1 MB. Totals per pass (computed): AO about 27 MB, GI Hi-Z about 97 MB, emitter about 33 MB, post about 88 MB. Atmosphere is about 28 MB on the froxel path. Shadow maps are not counted here.

### 3. Formats in one place (Q010, Q088)

- **Colour.** The renderer's own colour work is 8-bit in RGBA8 (`fx.texScene`, `atm.tex`), and 16-bit float in the intermediate passes (RGBA16F). The engine's colour copy is taken as the game provides it. Whether FB0 is 8-bit LDR is **(inferred)** from the RGBA8 copy in fx.inl:210 and fx.inl:171; the game's own format is NI-Ch03-8.
- **Depth.** The engine's depth copy is used as-is (its format is whatever the game chose). The renderer's linear depth is R32F (AO, GI).
- **Normals.** No target stores engine normals. The AO pass rebuilds them from depth on the fly (ao.inl:115-122). The GI pass stores its own view-space G-buffer in `texGB` (gi2.inl:5, 447).
- **Overlay.** Not a render target of this chapter (overlay uses glDrawPixels; see [overlay](09-overlay-input-and-hotkeys.md)).

### 4. Depth reconstruction (Q089)

1. The depth texture is the engine's depth copy, bound on unit 0 (ao.inl:614, 659, 704).
2. A depth sample `d` in [0, 1] becomes a normalised device z in one of two ways (ao.inl:109):
   - `ZMode=0` (default, ao.inl:503): `z = 2d - 1` (GL convention).
   - `ZMode=1`: `z = d`.
3. The world position is `invVP * (uv * 2 - 1, z, 1)`, divided by w (ao.inl:107-112). `invVP` is the inverse of the scene view-projection captured for this frame (ao.inl:569, Invert4 at ao.inl:70-92).
4. The view position is that world position multiplied by the view matrix, which the renderer rebuilds from the VP rows (ao.inl:113, 584-585). Camera position is the near-plane centre moved back by the near distance (ao.inl:575-583).
5. For the AO horizon search, linear view depth is taken from the depth copy at half resolution: `1 / dot(invRow3, (ndc.xy, z, 1))` (ao.inl:184-206, `kPre0`). If the four full-resolution depths of a 2 by 2 block agree within 5 per cent, their average is used. Otherwise the real sample closest to the mean is kept, so no depth is invented across a silhouette (ao.inl:194-205).

### 5. Normals (Q090)

- **AO.** Five-tap cross product on view positions. For each axis, the neighbour with the smaller depth step is used, which rejects silhouettes (ao.inl:115-122). The normal is only used in debug view 2 and inside the horizon search.
- **GI.** The same method on the GI depth pyramid. The pyramid holds `1/viewZ` (gi2.inl:3), and the trace uses its level 0 (gi2.inl:196-209). The result is normalised, with a fallback to +Z if the cross product is zero (gi2.inl:209).

### 6. Upsampling (Q091)

- **AO composite** (ao.inl:406-416). Four nearest half-resolution texels, bilinear weights, times a depth-similarity weight `exp(-400 * dz^2)` where `dz` is the relative view-depth difference. Result is `pow(clamp(sum/wsum), u_power)`. The AO texture carries viewZ in its G channel (written at ao.inl:374 in the temporal pass).
- **GI composite** (gi2.inl:322-333). Same idea, with the half-resolution view depth from `texGB` (its W component) and a similarity weight `exp(-300 * dz^2)`. Samples with negative depth (sky or invalid) are skipped (gi2.inl:327).
- **AO blur** (ao.inl:378-393). Two-dimensional, edge-aware loop. Radius from `[ao] BlurRadius` (default 2, so 5 by 5). The weight combines a depth-similarity term with a radial falloff.

### 7. Camera, VP and jitter (Q056, Q057)

- **Capture.** Each `glGetUniformLocation` call records locations named `VPMatrix` in `g_vpLocs` (tyrender.cpp:340), and the names `shadowVPMatrix1`, `shadowVPMatrix2`, `shadowDir`, `fogColour`, `shadowTexture1`, `shadowTexture2`, `fogCoords`, `diffuseTexture` in `g_namedLocs` (tyrender.cpp:341-342).
- **Scene VP.** `glUniformMatrix4fv` on a `VPMatrix` location, with FB0 bound and a perspective matrix (the bottom row must have a combined x, y, z length of at least 0.5; ao.inl:58-68), is stored as `g_sceneVP` (ao.inl:65). Stored before jitter is applied, so `g_sceneVP` is unjittered (ao.inl:57).
- **Named uniforms.** `shadowVPMatrix1/2` are stored as shadow matrices (atmos.inl:510-515). `shadowDir` and `fogColour` are stored as 4-vectors (atmos.inl:516-518). `fogCoords` is rewritten by `FogOverride` (tyrender.cpp:404-408, 413, 422).
- **Jitter source.** `UpdateJitter` runs after each present (tyrender.cpp:613). It is on only when `[post] AA` is 2 or 3, post is enabled and not turned off, and the window size is known (fx.inl:162). The sample is Halton(2, 3) over an 8-frame cycle, converted to NDC units of 2 divided by the window size (fx.inl:164-166).
- **Jitter application.** The renderer adds the jitter, times w, to the clip-space x and y of the uploaded scene VP (tyrender.cpp:352-356). Only the scene VP upload is changed, not other matrices.
- **TAA.** Uses the unjittered `g_sceneVP` and `fx.prevVP` for reprojection, and the jitter as a UV offset of half the NDC jitter (fx.inl:220-225).

### 8. Resolution and aspect (Q126)

- **Size source.** Each frame's depth copy sets `g_screenW` and `g_screenH` (tyrender.cpp:287). Every pass gets `w` and `h` from that call.
- **Re-initialisation.** Each pass compares the size it was built for with the new one and rebuilds its targets when they differ: AO (ao.inl:535), GI Hi-Z (gi2.inl:366), GI legacy (gi.inl:142), emitter (emit.inl:112), atmosphere (atmos.inl:376, 483, and EnsureVolTex at atmos.inl:274), post (fx.inl:190). Rebuilds happen inside the frame, so a resize is handled on the next depth copy.
- **Old targets.** Several passes do not delete the old texture ids when they rebuild (see table). The AO code says so (ao.inl:551). The effect, if the driver does not reclaim them, is extra GPU memory after each resize until the game exits (inferred).
- **Window events.** There is no `WM_SIZE` handler. The session report reads the client size and fullscreen state every frame and logs `window WxH ... -> ...` when either changes (sesslog.inl:335-341). The window size is also read by the overlay, screenshots and the debug camera (overlay.inl:163, abshot.inl:27, debugcam.inl:60).
- **Fixed constants.** Two places assume a size. The tessellation fallback uses 1280 by 720 when the window size is not yet known (tess.inl:178). The atmosphere blur radius scales by height over 1440 (atmos.inl:449). Shadow map size does not depend on the window.
- **Out-of-window reads.** `glReadPixels` calls that leave the window are skipped (tyrender.cpp:308-313).
- **Untested.** Ultrawide, 4:3, DPI scaling and fullscreen transitions beyond 2560 by 1440 windowed. NI-23 in the master list.

### 9. MSAA (Q093)

- **Game side.** The game chooses MSAA. The docs say "MSAA by pixel format, 2x default" (docs/START_HERE.md:85). The renderer does not set a pixel format. Its only MSAA hooks are logs (`glRenderbufferStorageMultisample`, tyrender.cpp:325-326). Whether FB0 is multisampled at run time is NI-Ch03-7.
- **Game setting.** The game's own `settings.ini` has a `[Display] MSAA` value (off, 2x, 4x, 8x). The renderer reads it only for a warning (tyrender.cpp:645-658). The launcher labels its AA "Off" as "game MSAA only" (launcher_ui.inc:448, ui_part_c.inc:149). See the settings clash entry (#13, CHANGELOG.md:20).
- **Our reads.** The post copy comment says "MSAA resolves implicitly" (fx.inl:208). That claim is not verified here. The other copies from FB0 (colour, depth, atmosphere, emitter depth) have the same open question (NI-Ch03-7).
- **Blit.** The renderer hooks `glBlitFramebuffer` only to log it (tyrender.cpp:327-328). It does not resolve anything itself.

### 10. State save and restore (Q094)

Every pass follows the same pattern. It reads the GL state it will change, changes it, draws, and writes the saved values back. The table lists what each pass saves, what it changes and does not put back, and where it sits.

| Pass | Saved and restored | Changed, not put back | file:line (save / restore) |
|---|---|---|---|
| AO (`RunAO`) | GL_FRAMEBUFFER binding, viewport, active unit, 2D binding of units 0 to 2, VAO, blend src and dst, depth mask, colour mask, enables for blend, depth test, cull, scissor, stencil, current program | none, apart from the read-buffer note below | ao.inl:588-592 / 714-721 |
| GI Hi-Z (`RunGI`) | same set, units 0 to 4 | none, apart from the read-buffer note below. The mask copy binds READ to 0 and DRAW to `g2.fbo` (gi2.inl:421-422); the restore sets both through GL_FRAMEBUFFER | gi.inl:120-127 / 192-198 |
| GI source mask (draw) | colour mask, depth mask, blend enable, blend functions (glBlendFuncSeparate) | none; restored in GIMaskEnd | gi2.inl:43-55 |
| Emitter composite (`RunEmit`) | FB0, viewport, VAO, program, blend funcs, colour and depth masks, active unit and unit 0 binding, enables for blend, depth, cull, stencil, scissor | none found | emit.inl:231-235 / 260-265 |
| Emitter splat (per draw) | draw and read FBO, viewport, VAO, program, blend funcs, depth func, unit 0 binding, enables for blend, scissor, depth test, colour and depth mask | the draw's own program and VAO are set back to the draw's values (emit.inl:205), not saved again | emit.inl:181-184 / 211-221 |
| Atmosphere (`RunAtmos`) | FB0, viewport, active unit, 2D binding of units 0 to 3, VAO, blend funcs, masks, enables | units 4 to 7 are set to 0 when used (atmos.inl:495-497), not restored to their previous value; unit 7 is bound only for the water mask | atmos.inl:402-410 / 500-506 |
| Post (`RunFX`) | FB0, viewport, active unit, unit 0 binding, VAO, blend funcs, masks, enables | units 1 and 2 are set to 0 after the TAA draw (fx.inl:231) and not restored | fx.inl:198-204 / 256-260 |
| Colour re-copy | binding of the active unit | none | tyrender.cpp:295, 299 |
| Late fog (per draw) | saved `fogCoords` | restored by LateFogEnd; not called on the tessellation early return (see 04 Limits) | atmos.inl:341 / 345 |
| See-through depth (per draw) | depth mask | set to 0 on end, which equals the saved value by the precondition in atmos.inl:354 | atmos.inl:355 / 357 |

**Note on READ_FRAMEBUFFER.** The passes bind through GL_FRAMEBUFFER, which sets both read and draw. If the game ever uses different read and draw framebuffers, the restore does not put back the read one. StateGuard checks READ_FRAMEBUFFER separately (stateguard.inl:16), so this shows up in the leak log.

**How the leak check works.** `[debug] StateGuard=1` snapshots 54 pieces of state at odd marks (1, 3, 5, 7) and compares at the even marks (2, 4, 6, 8). Any difference is written as `[state] LEAK after <pass>: NAME before -> after`, at most four distinct lines per pass (stateguard.inl:9-10, 12-30, 31-40). The guard is off by default.

**Why passes use original functions.** Passes call the un-hooked functions (`o_...`), and their texture and unit changes bypass the hook's record of which texture is bound (`g_bound2D`, tyrender.cpp:259-260) and of the active unit (tyrender.cpp:330). Because each pass restores bindings by value, the records are correct again when the game's next draw happens. The emitter code notes that a depth copy left on unit 0 once produced a white band (emit.inl:201-203).

### 11. Candidate leaks (not verified)

Two places restore less than they change, per the table above:

- **Post units 1 and 2** after TAA (fx.inl:231).
- **Atmosphere units 4 to 7** when they are set to 0 (atmos.inl:495-497).

Whether these matter depends on whether the game reads those units before rebinding them. Test with `[debug] StateGuard=1` (NI-Ch03-9).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Engine colour and depth copies | `src/tyrender.cpp:284-287` | handles stored here |
| AO targets | `src/ao.inl:526-562` | `MakeTex`, `InitAO` |
| AO depth chain | `src/ao.inl:184-206` | `kPre0`; mips in `kPreN` ao.inl:208-219 |
| Depth reconstruction | `src/ao.inl:101-123` | `kCommon` |
| VP capture | `src/ao.inl:58-68`; `src/tyrender.cpp:340-342, 345-359` | |
| GI Hi-Z targets | `src/gi2.inl:349-391` | |
| GI mask | `src/gi2.inl:31-55, 412-426` | |
| Emitter targets | `src/emit.inl:93-131` | |
| Atmosphere targets | `src/atmos.inl:254-284, 376-381, 421, 483-486` | |
| Post targets | `src/fx.inl:169-196` | |
| Jitter | `src/fx.inl:159-167` | |
| Upsample | `src/ao.inl:406-416`; `src/gi2.inl:322-333` | |
| State guard | `src/stateguard.inl:1-40` | |

## Settings

| Key | Code default | Shipped (Medium) | Live or restart | Effect on this page |
|---|---|---|---|---|
| `[ao] ZMode` | 0 (ao.inl:503) | not set | live | depth convention (section 4) |
| `[ao] Legacy` | 0 (ao.inl:506) | 0 (default_tyrender.ini:4) | live | v1 AO (fewer targets) |
| `[gi] Mode` | 1 (gi.inl:95) | 1 (default_tyrender.ini:9) | live | Hi-Z (large targets) or legacy |
| `[atmos] HalfRes` | 1 (atmos.inl:292) | not set | live | half-res fog target on the non-froxel path |
| `[atmos] Froxel` | 1 (atmos.inl:304) | 1 (default_tyrender.ini:33) | live | froxel volume |
| `[atmos] FroxelPixels` | 12 (atmos.inl:304) | not set | live | froxel grid size |
| `[shadows] MapScale` | 2 (tyrender.cpp:153) | 2 (default_tyrender.ini:47) | live | shadow map size |
| `[post] AA` | 2 (fx.inl:149) | 2 (default_tyrender.ini:51) | live | TAA history and jitter |
| `[debug] StateGuard` | 0 (stateguard.inl:32) | not set | live | leak log |

Full tables: [settings-post-shadows-light-pbr-tess](../reference/settings-post-shadows-light-pbr-tess.md), [settings-ao-gi-lod](../reference/settings-ao-gi-lod.md), [settings-atmos-fog-emit](../reference/settings-atmos-fog-emit.md), [settings-system-debug](../reference/settings-system-debug.md).

## Cost

- **Memory.** Computed from the formats above (not measured). Largest renderer consumers at 2560 by 1440: GI Hi-Z about 97 MB, post about 88 MB, emitter about 33 MB, atmosphere about 28 MB, AO about 27 MB. The shadow map is about 67 MB per 4096 by 4096 DEPTH32 map; the number of maps is NI.
- **Time.** Per-target costs are **not measured**. Frame-level option deltas are in [frame anatomy, Cost](04-frame-anatomy-and-pass-order.md#cost).
- **What scales it.** Window size, for every target. Half-resolution targets scale with a quarter of that. The GI pyramid also scales with the number of levels (about log2 of the larger side).

## Limits

- Half-resolution work loses detail at edges. The depth-aware upsamples (section 6) reduce, but do not remove, that loss (inferred).
- Only one depth copy and one colour copy are available per frame. Anything the passes need from elsewhere (normals, extra G-buffer data) is rebuilt from depth or drawn again.
- The renderer cannot write engine-native normals or material data, because it does not see them.
- Camera matrices are captured from uploads that happen on FB0 only. A camera upload made on another target is not seen.
- The jitter is applied only when the scene VP is uploaded as `VPMatrix`. A game that names its matrix differently would not get TAA jitter (inferred).

## Known issues

- **Resize.** Old targets are not deleted by AO (comment at ao.inl:551, "rare"), atmosphere, post, legacy GI or the atmosphere half-res target (see table). Extra memory each time the window changes size. No issue number in the repo.
- **Emitter depth copy in the draw.** The emitter splat copies depth from FB0 mid-draw (emit.inl:188). This can happen many times per frame, up to `MaxEmitters` (default 16). Not measured.
- **TAA ghosting** (#2, CHANGELOG.md:51) comes from the history targets in section 2. See [TAA and FXAA](../shaders/taa-fxaa.md).
- **Settings clash** (#13, CHANGELOG.md:20). Game MSAA plus renderer edge smoothing smooths edges twice (tyrender.cpp:656-658).

## How to verify

1. **Target list.** Run once and read `tyrender.log`. Expected lines: `[ao] initialised WxH (half W/2xH/2)`, `[gi] Hi-Z initialised WxH, N pyramid levels`, `[emit] emission buffer W/2xH/2 (depth tested)`, `[fx] initialised WxH`, `[atmos] froxel volume WxHx64` (ao.inl:560, gi2.inl:388, emit.inl:128, fx.inl:195, atmos.inl:283). Each `glTexImage2D tex=... ifmt=... WxH` line gives the engine's allocations (tyrender.cpp:276).
2. **Depth format.** Read the first three frames: `glCopyTexImage2D t=0xde1 ifmt=... src=... WxH` (tyrender.cpp:281). The ifmt on the depth line is the engine's depth format (NI-Ch03-8).
3. **Resize.** With the session report on, change the window size. Expected: a `window WxH ... -> ...` event (sesslog.inl:339), and a new `[ao] initialised` line. Check the memory with a GPU tool if available (not in repo).
4. **Half-resolution AO.** `<game folder>\tyrender\ao_debug` = 3 shows view depth (ao.inl:5). Stepping across a silhouette should show no halo (inferred from section 4).
5. **Leak check.** Set `[debug] StateGuard=1` in `tyrender.ini`, play for a few minutes, then search `tyrender.log` for `[state] LEAK after`. Expected: no lines. Any `post` or `atmos` line supports or refutes NI-Ch03-9.
6. **Jitter.** With `[post] AA=2`, the scene should shimmer in a fixed 8-frame pattern at rest; with `AA=0` it should not (inferred from fx.inl:164). Expected: no visible shimmer on static geometry with AA=0.

## NEEDS INVESTIGATION

- **NI-Ch03-7.** Whether a copy from a multisampled FB0 resolves implicitly (fx.inl:208), and whether FB0 is multisampled on this build. Method: set the game's MSAA to 2x, run, read the session report's GL error events (sesslog.inl:96-102) and `[sesslog]` limits (sesslog.inl:248). Alternatively query `GL_SAMPLES` on FB0 in a test build.
- **NI-Ch03-8.** Engine render-target formats and sizes: the depth copy format, FB0 bit depth (LDR claim, section 3), and the "post RTs fixed at 1024 by 512" claim (docs/START_HERE.md:85). Method: read the allocation and copy lines in `tyrender.log` (tyrender.cpp:276, 281).
- **NI-Ch03-9.** Whether the candidate leaks in section 11 change any game draw. Method: `[debug] StateGuard=1` and read `[state] LEAK after post` and `[state] LEAK after atmos` lines (stateguard.inl:39).
- **NI-23** (master list). Resolution, aspect and fullscreen beyond 2560 by 1440 windowed. Method: test 1920 by 1080 and 21:9; read each pass's `Init(w,h)`.
- **NI-Ch03-10.** Whether the old texture ids from a resize (section 8) are reclaimed by the driver, or accumulate. Method: resize the window three times and watch GPU memory with a vendor tool; not measured here.

## Discrepancies

- **`fx.inl:208`** says MSAA resolves implicitly. Not verified (NI-Ch03-7).
- **`docs/START_HERE.md:85`** says the post RTs are 1024 by 512 and MSAA is 2x by default. Neither appears in code (NI-Ch03-8).
- **First-pass ARCHITECTURE.md section 8** says the state guard snapshots "about 60" pieces of state. The code snapshots 54 entries (stateguard.inl:12-30). The header comment at stateguard.inl:2 says "~60".
- **`gi2.inl:1`** (header) says the Hi-Z pass falls back to the legacy path on any failure. The code does not (gi.inl:130, 139). Covered on page 11.

## See also

- [Frame anatomy and pass order](04-frame-anatomy-and-pass-order.md)
- [Fallbacks and failure handling](11-fallbacks-and-failure-handling.md)
- [GL hooking](03-gl-hooking.md)
- [Settings pipeline and live reload](08-settings-pipeline-and-live-reload.md)
- [Shader inventory](../shaders/README.md)
- [AO](../shaders/ao-gtao.md), [GI Hi-Z](../shaders/gi-hiz.md), [atmosphere](../shaders/atmosphere-fog.md)
