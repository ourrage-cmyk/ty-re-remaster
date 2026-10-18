# Frame anatomy and pass order

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), C (maintainer). Testers: see the "How to verify" section.

## Purpose

This page says what happens during one game frame, in order, and where the renderer hooks into it. The renderer does not own the frame. The game draws its scene, copies colour and depth out of the default framebuffer, and presents. The renderer adds its passes at two points: inside the game's draw calls (per-draw work such as the GI source mask and emitter splats) and at the game's depth copy (AO, GI, emitter composite and atmosphere, in that order). Post-processing runs at the first 64 by 64 viewport after that copy. Everything is numbered below with `file:line` so a reader can check each step.

Game-side steps are not in this repository. Where the order of a game step is inferred from the hook positions, it is marked **(inferred)**. The full engine frame anatomy the code refers to (a file named "F3 anatomy") is not in the repository, see NEEDS INVESTIGATION.

## How it works

### The frame, numbered

```
GAME FRAME (game steps are not in the repo; their order is partly inferred)

 [A] Shadow maps (inferred)          game draws cascade depth maps; the renderer
     FBO != 0, viewport 2048x2048     scales that viewport by [shadows] MapScale
                                      (tyrender.cpp:255, 272-279)

 [B] Scene draws into FB0            per draw call, the renderer adds up to four things:
     glDrawElements  tyrender.cpp:267  1. TexDrawPrep sets the per-draw material uniform (tex.inl:224-240);
                                          SpillLogDraw and FogDiagDraw only count draws for logs
     glDrawArrays    tyrender.cpp:270     2. late-fog uniform boost for see-through draws (atmos.inl:329)
                                          3. EmitQBegin/EmitQEnd: occlusion query, emitter-class draws (emit.inl:137-152)
                                          4. GIMaskBegin ... GIMaskEnd: second draw, alpha = 0 (gi2.inl:38-55)
                                          5. EmitBegin ... EmitEnd: second draw, emitter splat (emit.inl:173-223)

 [C] Engine colour copy              glCopyTexImage2D, RGB/RGBA, level 0, FB0
     tyrender.cpp:284-285             -> the renderer stores this texture handle (g_colorCopyTex)

 [D] Engine depth copy               glCopyTexImage2D, DEPTH_COMPONENT(16/24/32), FB0
     tyrender.cpp:286-287             -> TRIGGER. Stores g_sceneDepthTex, g_screenW/H, sets g_sceneDepthFrame
     tyrender.cpp:289                    PbrUpdate(): camera position for PBR and tess (pbr.inl:212)
     GProf 1-2  RunAO            ao.inl:566     FB0 *= AO            (multiply)      tyrender.cpp:290
     GProf 3-4  RunGI            gi.inl:104     FB0 += bounce light  (additive)      tyrender.cpp:291
     GProf 20-21 RunEmit         emit.inl:226   FB0 += emitter light (additive)      tyrender.cpp:292
     GProf 5-6  RunAtmos         atmos.inl:358  FB0 = fog(FB0)                       tyrender.cpp:293
     GProf 18-19 colour re-copy  tyrender.cpp:294-300   engine colour copy is refreshed
                                                        so later readers see [A]-[D] results

 [E] Game bloom / brightness chain   game code, not in repo. Reads the colour copy
                                     (re-copied in step D). Order relative to [F]: NEEDS INVESTIGATION (NI-Ch03-2)

 [F] First glViewport(64,64) after [D]   tyrender.cpp:254
     GProf 7-8  RunFX            fx.inl:177     TAA -> FXAA -> grade (sharpen, tone) -> FB0
                                  (the code comment calls this the start of the game's brightness chain)

 [G] Game HUD                        game code, not in repo. Drawn after [F] (inferred from the code comment at tyrender.cpp:253)

 [H] SwapBuffers hook               tyrender.cpp:591
     every 8 frames: Snapshot, debug-camera, motion, AO/atmos/GI flag files, reload (tyrender.cpp:594-604)
     every frame:    TexReport (605), GProf 9 = frame end (606)
                     ABShotFrame (F11 A/B, 607), OverlayFrame (F10 panel, 608)
                     real SwapBuffers (609), SessFrame (610), GProfNextFrame (611), g_frame++ (612)
                     UpdateJitter for the next frame (613, fx.inl:161-167)
```

### Steps in words

1. **Shadow maps.** The renderer does not draw them. It recognises them: a viewport of 2048 by 2048 while an FBO is bound is enlarged by `[shadows] MapScale` (tyrender.cpp:255). The matching texture allocation is enlarged in `glTexImage2D` (tyrender.cpp:274). The shadow maps are rendered before [D] (inferred).
2. **Scene draws.** Each game draw passes through the hooks in `glDrawElements` (tyrender.cpp:267-269) or `glDrawArrays` (tyrender.cpp:270-271). The renderer's own draws use the original functions (`o_...`), so the hooks never see them. Two extra draws are issued for some materials: the GI source mask and the emitter splat. Both are described in the shader pages ([GI mask](../shaders/gi-hiz.md), [emitter](../shaders/emitter-light.md)).
3. **Colour copy.** The game copies FB0 colour. The renderer keeps the texture handle (tyrender.cpp:284-285). This copy is the scene colour before any renderer pass, and it is what AO and GI read for albedo (ao.inl:706, gi2.inl:458).
4. **Depth copy (trigger).** The game copies FB0 depth. The renderer accepts depth internal formats 0x1902 (DEPTH_COMPONENT), 0x81A5, 0x81A6 and 0x81A7 (tyrender.cpp:286). It records the copy size as the screen size (tyrender.cpp:287). This is the only per-frame trigger for AO, GI, emitter composite and atmosphere.
5. **AO** (GProf 1-2) multiplies the AO term onto FB0 with a ZERO / SRC_COLOR blend (ao.inl:707). See [AO](../shaders/ao-gtao.md).
6. **GI** (GProf 3-4) adds the bounce term onto FB0 with ONE / ONE blending (gi2.inl:509). Its colour source is the engine colour copy from step 3, not the AO result (gi2.inl:458). See [GI Hi-Z](../shaders/gi-hiz.md).
7. **Emitter composite** (GProf 20-21) blurs the emission buffer and adds it to FB0 (emit.inl:226-258). It runs only when at least one emitter splat was drawn this frame (emit.inl:229).
8. **Atmosphere** (GProf 5-6) first copies FB0 into a private texture (atmos.inl:413), then draws the fog over FB0 (atmos.inl:494). The fog therefore sees AO, GI and emitter light. See [atmosphere](../shaders/atmosphere-fog.md).
9. **Colour re-copy** (GProf 18-19). If any of steps 5 to 8 set `g_sceneModified`, the renderer re-copies FB0 into the engine's colour copy texture (tyrender.cpp:294-300). The game's bloom then reads the modified image. Setting `[ao] Exp` bit 1 disables this (tyrender.cpp:294, ao.inl:504).
10. **Post** (GProf 7-8). The first 64 by 64 viewport after step 4 runs RunFX (tyrender.cpp:254). RunFX copies FB0, then runs TAA into a history texture, FXAA into a temporary texture, and the grade into FB0 (fx.inl:208-254). The order TAA, FXAA, grade is fixed in code. Which of TAA or FXAA runs depends on `[post] AA` (fx.inl:214, 236).
11. **Frame end.** GProf 9 is written before the real present (tyrender.cpp:606). The overlay (F10) and the F11 A/B capture run after the renderer passes and before present (tyrender.cpp:607-608), so screenshots exclude the overlay.

### Why the passes sit where they do (Q087)

- **Forward, not deferred (inferred).** The renderer never sees a G-buffer. It sees one colour copy and one depth copy (tyrender.cpp:284-287). Normals are rebuilt from depth in each pass (ao.inl:115-122). The per-draw light data is one uniform array, `tyLt`, set per program (tyrender.cpp:169-172, light.inl:7). Those are forward-shading patterns. The code does not state the reason for the engine's forward design. The docs state a forward light cap of 16 (docs/PLAN_NEXT.md:35), which is not verified in code (NI-11 in the master list).
- **What deferred would take.** The repo does not discuss it. The research note lists world-space alternatives for AO, GI and fog and states that the current effects are screen-space (docs/research/WORLD_SPACE_AO_GI_FOG.md:9-14, 22-34). The note does not cost a G-buffer. Any deferred change would also need the engine's own draws rewritten, which is outside what the hooks do today.
- **Why after opaque geometry.** The passes need a depth buffer that already holds the opaque scene. The depth copy in step 4 is the first point where the engine has that. Passes placed earlier would see no depth. Passes placed later (after HUD) would darken UI. This reasoning is **(inferred)** from the hook positions.

### Hook-by-hook table

| What | Hook / function | file:line | Note |
|---|---|---|---|
| Viewport 64x64 starts post | `glViewport` | tyrender.cpp:252-254 | once per frame (`g_fxDoneFrame`) |
| Shadow map scale | `glViewport`, `glTexImage2D` | tyrender.cpp:255, 274 | only if `[shadows] Patch=1` and `MapScale>1` |
| Per-draw work | `glDrawElements` | tyrender.cpp:267-269 | tess mode, GI mask, emitter splat |
| Per-draw work | `glDrawArrays` | tyrender.cpp:270-271 | same, without late fog |
| Colour copy | `glCopyTexImage2D` | tyrender.cpp:284-285 | RGB or RGBA |
| Depth copy (trigger) | `glCopyTexImage2D` | tyrender.cpp:286-287 | depth formats 0x1902, 0x81A5-7 |
| Camera for passes | `PbrUpdate` | tyrender.cpp:289, pbr.inl:212 | |
| AO / GI / emit / atmos | `RunAO`, `RunGI`, `RunEmit`, `RunAtmos` | tyrender.cpp:290-293 | each skipped in debug views (see below) |
| Colour re-copy | `o_glCopyTexImage2D` | tyrender.cpp:294-300 | only if `g_sceneModified` |
| Post | `RunFX` | tyrender.cpp:254, fx.inl:177 | |
| Frame boundary | `h_SwapBuffers` | tyrender.cpp:591-630 | reload, overlay, A/B, timers |
| Frame count and jitter | end of `h_SwapBuffers` | tyrender.cpp:612-613 | |

### Debug views and skip flags

- The debug views are exclusive. When `g_aoDebug` is set, GI, emitter and atmosphere are skipped (tyrender.cpp:291-293). When `g_giDebug` is set, emitter and atmosphere are skipped (tyrender.cpp:292-293).
- Each pass also checks its own off-switch: `ao_off`, `gi_off`, `atmos_off` flag files, and `post_off` for FX (ao.inl:520, gi.inl:100, atmos.inl:311, tyrender.cpp:596). The flag files are polled every 8 frames (tyrender.cpp:595-596).
- The F11 A/B "off" frame sets `g_abOff` for one frame (abshot.inl:123, 130). While it is set, AO (ao.inl:567), GI (gi.inl:105), GI mask (gi2.inl:39), emitter (emit.inl:139, 227), atmosphere (atmos.inl:362) and FX (fx.inl:178) all return without drawing. The shadow strength falls back to the stock literal (tyrender.cpp:558).

### Timing marks (Q095)

The GPU timer marks are GL timestamp queries. Each mark is set with `GProf(m)` (gpuprof.inl:17-21). The renderer keeps a ring of four frames and reads a frame's results three frames later, so reading never waits (gpuprof.inl:1-3, 23-45).

| Mark | Set at | Meaning |
|---|---|---|
| 0 | gpuprof.inl:44 | frame start (after the previous present) |
| 1 / 2 | tyrender.cpp:290 | around RunAO |
| 3 / 4 | tyrender.cpp:291 | around RunGI |
| 5 / 6 | tyrender.cpp:293 | around RunAtmos |
| 7 / 8 | tyrender.cpp:254 | around RunFX |
| 9 | tyrender.cpp:606 | frame end, before present |
| 10 / 11 | ao.inl:618, 660 | AO depth chain (v2: Pre0 and mips; v1: whole legacy GTAO) |
| 12 | ao.inl:662 | after the AO horizon pass, before temporal |
| 13 / 14 | ao.inl:692 | after temporal and blur |
| 15 | ao.inl:710 | after AO composite |
| 16 / 17 | ao.inl:629, 651 | v2 only: GTAO horizon search |
| 18 / 19 | tyrender.cpp:296, 298 | colour re-copy |
| 20 / 21 | tyrender.cpp:292 | RunEmit |

The log lines are written every 300 valid samples (gpuprof.inl:35-40):

- `[gpu] frame F | ao A gi G atmos T post P | engine+rest R (N draws)`
  - `frame` = marks 0 to 9
  - `ao` = marks 1 to 2, `gi` = 3 to 4, `atmos` = 5 to 6, `post` = 7 to 8
  - `engine+rest` = frame minus the four above. It includes everything not in a mark: the game's draws, the GI mask draws, emitter splats and their depth copies (emit.inl:188), and the per-draw late-fog work.
- `[gpusub] sub0 .. sub4 | emit E`
  - `sub0` = marks 10 to 11, `sub1` = 12 to 13 (temporal and blur), `sub2` = 14 to 15 (AO composite), `sub3` = 16 to 17 (v2 horizon search), `sub4` = 18 to 19 (colour re-copy), `emit` = 20 to 21 (gpuprof.inl:33, 38).

If the timer entry points cannot be loaded, timing turns off silently (gpuprof.inl:13).

`[debug] GpuProf=0` is named in the header of gpuprof.inl (gpuprof.inl:3) but nothing reads it. The variable is a constant 1 (gpuprof.inl:5). See Discrepancies.

### Colour and depth as seen by passes (Q047, Q048, Q092)

- **Depth copy identification (Q047).** A `glCopyTexImage2D` call is treated as the depth copy when the target is TEXTURE_2D, the level is 0, the FBO is 0, and the internal format is a depth format (tyrender.cpp:284, 286). That is the whole test. There is no check on the game's call site.
- **First 64 by 64 viewport (Q048).** The renderer compares the viewport size of each `glViewport` call with 64 by 64 and runs RunFX on the first such call after the depth copy of the same frame (tyrender.cpp:254). The guard `g_fxDoneFrame` stops a second run in the same frame.
- **Why re-copy colour (Q092).** The game's bloom reads its own colour copy, which the game made before the renderer passes ran. Without the re-copy, bloom would not see AO, GI, emitter light or fog. The code comment says so (tyrender.cpp:294). The bloom's use of that texture is inferred from the comment: the game code is not in the repo.
- **Colour source for GI and AO.** Both read the engine colour copy from step 3, which is before the renderer's own changes. Atmosphere and FX read FB0 directly (atmos.inl:413, fx.inl:210).

### Known caveat: a draw can be skipped

On a tessellated program, a draw whose mode is not GL_TRIANGLES returns early and is not drawn (tyrender.cpp:268, 271). The counter `g_tessBadDrawsExt` records it, and the `[frame]` log line every 600 frames prints it (tyrender.cpp:618). This is deliberate: tessellated programs accept only GL_PATCHES.

## Code pointers

| What | file:line | Note |
|---|---|---|
| Frame hooks (viewport, draws, copies) | `src/tyrender.cpp:252-303` | the whole frame trigger |
| Per-draw hooks | `src/tyrender.cpp:267-271` | |
| Depth-copy pass sequence | `src/tyrender.cpp:286-300` | GProf marks inline |
| SwapBuffers hook | `src/tyrender.cpp:591-630` | |
| AO entry | `src/ao.inl:566` | |
| GI entry | `src/gi.inl:104`; Hi-Z `src/gi2.inl:394` | |
| Emitter entry | `src/emit.inl:226` (RunEmit); per-draw `:173` (EmitBegin) | |
| Atmosphere entry | `src/atmos.inl:358` | |
| Post entry | `src/fx.inl:177` | |
| GI mask | `src/gi2.inl:31-55` | |
| GPU timers | `src/gpuprof.inl:1-50` | |
| Frame-end timestamp | `src/tyrender.cpp:606`; next frame `:611` | |
| Jitter per frame | `src/fx.inl:161-167` | |

## Settings

The settings tables are in the reference pages. Keys that change the frame order or what the passes do:

| Key | Code default | Shipped (Medium) | Live or restart | Effect |
|---|---|---|---|---|
| `[ao] Enable` | 1 (ao.inl:492) | 1 (default_tyrender.ini:3) | live (reload) | AO pass at step 5 |
| `[gi] Enable` | 1 (gi.inl:91) | 0 (default_tyrender.ini:8) | live | GI pass at step 6 and GI mask at step 2 |
| `[atmos] Enable` | 1 (atmos.inl:292) | 1 (default_tyrender.ini:32) | live | atmosphere at step 8 |
| `[emit] Enable` | 0 (emit.inl:32-43) | 0 (default_tyrender.ini:107) | live | emitter splat and composite |
| `[post] Enable` | 1 (fx.inl:148) | 1 (default_tyrender.ini:50) | live | post at step 10 |
| `[post] AA` | 2 (fx.inl:149) | 2 (default_tyrender.ini:51) | live | 0 none, 1 FXAA, 2 TAA, 3 TAA and FXAA |
| `[debug] StateGuard` | 0 (stateguard.inl:32) | not set | polled every 300 frames | leak log for steps 5-10 |
| `[debug] CaptureFrame` | 0 (tyrender.cpp:58) | not set | restart (read at DllMain) | logs one whole frame call by call |
| `[debug] DumpShaders` | 1 (tyrender.cpp:60) | not set | restart | shader dumps are written by default |
| `[ao] Exp` | 0 (ao.inl:504) | not set | live | test bits: 1 skips colour re-copy, 2 skips AO composite draw, 8 skips all AO draws |

Shadow map scaling is `[shadows] MapScale` (tyrender.cpp:153, default 2, clamped 1 to 4). Shipped value 2 (default_tyrender.ini:47). Full list: [settings-post-shadows-light-pbr-tess](../reference/settings-post-shadows-light-pbr-tess.md), [settings-ao-gi-lod](../reference/settings-ao-gi-lod.md), [settings-atmos-fog-emit](../reference/settings-atmos-fog-emit.md), [settings-system-debug](../reference/settings-system-debug.md).

Reload behaviour: `tyrender\reload` triggers LoadAOConfig, LoadFXConfig, LoadAtmosConfig, LoadGIConfig, LoadEmitConfig, LoadShadowConfig, LoadPbrConfig, LoadTessConfig, LoadLightConfig, LoadShotConfig, LoadOverlayConfig (tyrender.cpp:597-603). Settings read only in DllMain are restart-only (tyrender.cpp:714-731).

## Cost

- **Frame totals (measured).** docs/PERFORMANCE.md, v0.3.2, 2560x1440 windowed, AMD Radeon RX 7600 XT, vsync off: Medium "about 12 ms" (line 13), default view 12.08 ms (line 36), forced fallback with every effect off 7.44 ms (line 40), stock game 5.7 ms (line 11).
- **Option deltas, relative to Medium (measured, v0.3).** AO off about -1.15 ms; volumetric fog and light shafts off about -2.1 ms; contact shadows 0.35 about +1.7 ms; bounce light (Hi-Z, 1 ray) about +4.4 ms; TAA off about +0.4 ms (PERFORMANCE.md lines 50-55). A second table in docs/results/cost_table.md gives the same ordering. That file does not name its build or GPU, so it is not used as a source here.
- **The GI mask.** docs/PERFORMANCE.md line 28 says the source mask is part of the High preset's extra cost, and that its split is not measured.
- **Per-pass timers under-count.** The docs say so (PERFORMANCE.md line 60). The marks in the table above cover the passes, not the copies or the masked and splatted draws, which sit inside `engine+rest`.
- Per-pass cost on the current build is **not measured** beyond the rows above.

## Limits

- Passes cannot see geometry that is not on screen, or behind other geometry, because they work from one depth copy (inferred from step 4).
- A pass sees only the scene colour and depth at the time of its trigger. Anything drawn after step 4 is seen by no pass except RunFX, and anything drawn after RunFX is not touched.
- Draws that the renderer re-issues (GI mask, emitter splat) cost a second draw each. Their cost is not in any mark.
- Non-triangle draws on tessellated programs are dropped, see the caveat above.
- The first 64 by 64 viewport is a heuristic. If the game ever uses that size before the depth copy, or never uses it after, RunFX does not run in that frame (inferred from tyrender.cpp:254).

## Known issues

- **F11 A/B "off" frame** skips AO, GI, atmosphere, FX, the shadow strength and more (CHANGELOG.md:32, issue #96). Status: untested. The skips are at ao.inl:567, gi.inl:105, atmos.inl:362, fx.inl:178 and tyrender.cpp:558.
- **Emitter white band.** `[emit] Enable=1` renders a flat white upper band and stripes in the autoplaytest view (CHANGELOG.md:39). The code comment at emit.inl:201-203 describes the cause: an albedo sampled on unit 0 read the scene depth. The fix in code rebinds unit 0. Whether the reported band is still present on this commit is NEEDS INVESTIGATION (NI-Ch03-6). Keep emitters off.
- **Out-of-window `glReadPixels`.** The game reads a 16 by 16 depth patch around the sun each frame for its lens-flare occlusion. When the patch leaves the window, one Intel driver crashed inside `glReadPixels` (verified 2026-10-11 in the code comment). The renderer skips such reads and logs the first 20 (tyrender.cpp:304-315). No issue number is in the repo.
- **GI mask cost.** The source mask costs extra on High (PERFORMANCE.md line 28). The change that added it is #7 (CHANGELOG.md:52). Optimisation is open.
- **TAA ghosting** (#2) is in the post pass. See [TAA and FXAA](../shaders/taa-fxaa.md).
- **Settings clash** (#13). Game-side SSAO or MSAA duplicates AO and edge smoothing. The renderer warns in the log (tyrender.cpp:637-659). See [MSAA note](06-render-targets-depth-and-state.md#9-msaa-q093).

## How to verify

1. **Log one frame call by call.** Create the file `<game folder>\tyrender\cap.req`. The renderer checks for it every 8 frames and logs the next frame between `=== BEGIN CAPTURE frame N ===` and `=== END CAPTURE frame N ===` (tyrender.cpp:622-628, 592). The helper `tools/capframe.ps1` does this and copies the block out. Its `-Game` default is an owner path, so pass `-Game "<game folder>"`. Expected order in the block: `glCopyTexImage2D` for colour (ifmt 0x1907 or 0x1908), then `glCopyTexImage2D` for depth (ifmt 0x1902 or 0x81A5-7), then `glViewport 64 64`, then the post draws.
2. **Check the mark mapping.** The timers are always on (`[debug] GpuProf` is not read). Read `tyrender.log` every 300 frames for `[gpu]` and `[gpusub]` lines. Expected: `ao` and `gi` are non-zero when those passes run, and `emit` is 0.000 when `[emit] Enable=0`.
3. **Toggle passes one at a time.** Create `<game folder>\tyrender\ao_off`, `gi_off`, `atmos_off` or `post_off`. The flag is read on the next 8-frame poll (tyrender.cpp:595-596). Each file removes one step from the list above. Compare with the stock look.
4. **Debug views.** `<game folder>\tyrender\ao_debug` containing 1, 2 or 3 gives AO-only, normals and view depth views (ao.inl:5, 521). With an AO debug view on, GI, emitter and atmosphere do not run (tyrender.cpp:291-293).
5. **Colour re-copy.** Set `[ao] Exp=1` and reload. Bloom should lose the AO and fog look it had. (Expected change: visible in bright areas. Not measured.)
6. **GL errors.** The session report logs the first GL error of each kind (sesslog.inl:96-102). Check `tyrender\logs\` after a session.

## NEEDS INVESTIGATION

- **NI-4** (master list). Full engine frame anatomy (the "F3" file is not in the repo). Method: `tools/capframe.ps1` on several frames; compare with the list above.
- **NI-Ch03-1.** Order of the shadow passes relative to the scene draws, and whether the game draws a depth-only pre-pass. Method: capture a frame and look for 2048-by-2048 viewports before the first colour draw.
- **NI-Ch03-2.** Whether the game's bloom and brightness passes run before or after the 64 by 64 viewport that starts RunFX, and where the HUD is drawn. Method: capture a frame, note the first 64 by 64 viewport, and the first draws after the depth copy. A HUD draw after RunFX confirms step G.
- **NI-Ch03-3.** Whether the colour copy (step 3) happens every frame before the depth copy. RunAO only checks that the handle is non-zero (ao.inl:567), so a stale colour copy would go unnoticed. Method: capture three consecutive frames and compare the call order.
- **NI-Ch03-4.** Whether the game's depth copy is always one call per frame. Method: count depth `glCopyTexImage2D` per frame in a capture; also useful for Q011.
- **NI-Ch03-5.** Whether the late-fog boost can outlive its draw on the tessellation early return (tyrender.cpp:267-268; see the Limits caveat). Method: enable `[fog] WaterBoost=2` and capture a frame with a non-triangle draw on a tessellated program. Check whether `fogCoords` is set back by the game before the next draw.
- **NI-Ch03-6.** Whether the emitter white band in CHANGELOG.md:39 still occurs on this commit. Method: `[emit] Enable=1` in the autoplaytest view (docs/AUTOPLAYTEST.md) and screenshot.
- **NI-11** (master list). Forward light cap of 16 (docs/PLAN_NEXT.md:35). Method: read the light loop in a dumped shader (`tyrender\dump`).

## Discrepancies

- **`[debug] GpuProf=0`.** The header of gpuprof.inl (line 3) says it disables the timers. Nothing reads it (gpuprof.inl:5 is a constant 1). The first-pass ARCHITECTURE.md section 4 item 6 says the same.
- **`[gi] Enable` default.** Code default is 1 (gi.inl:91). The shipped ini sets 0 (default_tyrender.ini:8). The docs say GI is off in the shipped file; both are right for their source.
- **First-pass ARCHITECTURE.md section 4, item 1.** It says the GI mask excludes emissive and transparent draws. The default `ExcludeSources=7` also excludes character draws (gi.inl:93, gi2.inl:33, class CHARACTER with giBits 4 at tex.inl:44).
- **First-pass ARCHITECTURE.md section 4, item 4.** It says the order of TAA, FXAA and grade was not checked. It is now checked: TAA, then FXAA, then grade (fx.inl:213-254). The overlay is drawn later, in SwapBuffers (tyrender.cpp:608).
- **`fx.inl:208` comment** says FB0 is copied "MSAA resolves implicitly". Not verified here (NI-Ch03-7, see page 06).
- **`docs/START_HERE.md` line 85** gives the post targets as 1024 by 512 and the default MSAA as 2x. Neither is set in this repo. See page 06, NI-Ch03-8.

## See also

- [Render targets, depth and state](06-render-targets-depth-and-state.md) (formats, state save and restore, MSAA, resize)
- [Fallbacks and failure handling](11-fallbacks-and-failure-handling.md)
- [Loading and injection](02-loading-and-injection.md)
- [GL hooking](03-gl-hooking.md)
- [Logging, timers and session report](10-logging-timers-and-session-report.md)
- [Shader inventory](../shaders/README.md)
