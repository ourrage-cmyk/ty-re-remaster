# Temporal and fast anti-aliasing (TAA and FXAA)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributors), B (testers who tune `[post]`).

## Purpose

Edges of geometry and alpha-tested cards shimmer or stair-step at the game's native resolution. This module smooths them in two ways.

- **TAA** (temporal anti-aliasing) renders each frame with a tiny sub-pixel camera offset, then blends the current image with the reprojected result of earlier frames. Over a few frames it averages several samples per pixel.
- **FXAA** is a single-frame filter. It finds high-contrast luma edges in the finished image and softens them.

`[post] AA` selects the mode: 0 off, 1 FXAA, 2 TAA, 3 TAA with FXAA (`src/fx.inl:149`). The launcher labels are "Off (game MSAA only)", "FXAA (fast blur)", "TAA (smoothest, recommended)" and "TAA + FXAA" (`src/launcher_ui.inc:448-449`). Shipped Medium uses TAA (`default_tyrender.ini:51`). The Low preset uses FXAA (`src/launcher_ui.inc:11`, field `aa` = 1).

Both filters run inside the final-image pass in `src/fx.inl`. That pass runs after the 3D image is complete and before the HUD. The sharpen and colour grade that follow are on [grade-and-sharpen.md](grade-and-sharpen.md).

The TAA design follows published temporal-supersampling methods (neighbourhood clamping in a decorrelated colour space, reprojection with the previous frame's camera; Karis 2014, Halton low-discrepancy jitter). These are named from general knowledge and were not checked online. The code is our own.

## How it works

```
FB0 (3D image complete, no HUD yet)
  | copy (fx.inl:208-210)
  v
texScene (RGBA8) ----> kTAA + history (RGBA16F x2)     AA 2 or 3, fx.inl:214
  |                         |
  |                         v
  +----------------> kFXAA (texTmp)                     AA 1, or AA 3, or fallback, fx.inl:236
                            |
                            v
                  kGrade: sharpen + grade --> FB0       fx.inl:245-254
                            |
                            v
                  HUD (game) --> overlay + F11 at SwapBuffers (tyrender.cpp:608)
```

Per frame:

1. **Jitter for the next frame.** `UpdateJitter` (`src/fx.inl:161-167`) runs once per presented frame (`src/tyrender.cpp:613`). With TAA on, it picks the next of eight Halton(2,3) points. The offset is less than half a pixel. The offset is added to the clip-space x and y of every projection matrix the game uploads (`src/tyrender.cpp:352-356`). Jitter is off when post is disabled, when the post module failed, during the F11 A/B off frame, or when AA is not 2 or 3 (`src/fx.inl:162`).
2. **Copy the finished image.** `RunFX` is entered at the first 64x64 viewport of the frame (`src/tyrender.cpp:254`). It copies the default framebuffer into `texScene` (`src/fx.inl:208-210`). The copy resolves the game's MSAA implicitly (comment at `src/fx.inl:208`).
3. **TAA resolve** (AA 2 or 3, only when the scene depth copy exists and the frame's matrix was captured; `src/fx.inl:214`). The output goes into one of two 16-bit float history buffers (`src/fx.inl:193`), used in turn. Inside `kTAA` (`src/fx.inl:99-140`):
   a. The current colour is read at its jittered position, so the sample lands on the true pixel centre (`kTAA`, line 106).
   b. The 3x3 neighbourhood is converted to a luma/chroma space (YCoCg). Its minimum, maximum, mean and variance define a box. The box is the intersection of min/max with mean plus or minus one standard deviation (lines 107-117). The variance multiplier is fixed at 1.0.
   c. The depth-closest of the nine pixels picks the point used for reprojection, so edges follow the nearest surface (lines 113, 119).
   d. That point is reprojected with the previous frame's matrix (saved at `src/fx.inl:232`). If it lands off screen or behind the camera, the current colour is used alone (lines 122-124).
   e. History is clamped to the box (line 127). The blend starts at `TAABlend` (0.85). It falls towards 0.4 as the reprojected point moves more than about 8 pixels on screen (lines 128-129).
   f. History rejection: if the raw history lies far outside the box, measured against the box width, the blend is capped at 0.35, scaled by `TAAReject` (lines 130-133).
   g. Depth-guided rejection: every history pixel stores its view depth in alpha. If that depth differs from the reprojected depth by roughly 3 to 10 percent, the blend goes to zero (lines 134-137).
   h. The output is a mix of current and history colour. Alpha carries the new depth for the next frame (line 138).
   History is used only if the previous frame also ran TAA (`src/fx.inl:226`).
4. **FXAA** (optional). It runs on the TAA output, or on the copy, when `AA` is 1, when `AA` is 3, or when `AA` is 2 but TAA did not run and `[post] FXAA` is 1 (`src/fx.inl:236`). With Medium (AA 2, TAA ran), FXAA does not run. The shader `kFXAA` (`src/fx.inl:8-55`) works on luma:
   - it exits early where local contrast is low; the threshold is the larger of 0.0312 and 12.5 percent of the local maximum (line 20);
   - it computes a sub-pixel blend amount from the neighbourhood average (lines 24-26);
   - it picks an edge direction from horizontal versus vertical second differences (lines 28-30);
   - it walks along the edge in both directions, up to 10 steps with a fixed step table (lines 41-45);
   - it blends toward the side the edge lies on, chosen by an end test (lines 46-52).
   The file header calls this an "FXAA 3.11-style" filter (`src/fx.inl:4`). This page does not claim it matches that release line by line (inferred).
5. **Hand-off.** The last step's output is the input to the grade (`src/fx.inl:233`, `243`, `253`). The grade writes to the default framebuffer (`src/fx.inl:246`).
6. **State.** Blend, depth, cull, scissor and colour-mask state are switched off for the pass and restored afterwards (`src/fx.inl:198-206`, `256-260`).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Final-image entry point | `src/fx.inl:177` (`RunFX`) | called from `src/tyrender.cpp:254` |
| Config loader and code defaults | `src/fx.inl:146-157` | |
| Jitter choice | `src/fx.inl:159-167` | Halton 2 and 3, eight-point cycle |
| Jitter applied to matrix | `src/tyrender.cpp:352-356` | clip x and y shifted by jitter times w |
| Jitter update call | `src/tyrender.cpp:613` | once per presented frame |
| TAA shader | `src/fx.inl:97-140` | `kTAA` starts at line 99 |
| FXAA shader | `src/fx.inl:8-55` | `kFXAA` |
| TAA and FXAA selection | `src/fx.inl:214`, `src/fx.inl:236` | |
| History buffers | `src/fx.inl:193` | RGBA16F, two of them |
| Post failure latch | `src/fx.inl:178-184` | a failed compile sets `fx.failed` for the session |
| Overlay and F11 after post | `src/tyrender.cpp:608` | not filtered by TAA or FXAA |

## Settings

Keys that matter here. The full table is in [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[post] Enable` | 1 (`src/fx.inl:148`) | 1 (`default_tyrender.ini:50`) | live | Master switch for the whole final pass, grade included |
| `[post] AA` | 2 (`src/fx.inl:149`) | 2 (`default_tyrender.ini:51`) | live | 0 none, 1 FXAA, 2 TAA, 3 TAA plus FXAA |
| `[post] FXAA` | 1 (`src/fx.inl:148`) | not set | live | Matters only for AA 2 when TAA cannot run. AA 1 and AA 3 run FXAA regardless |
| `[post] TAABlend` | 0.85 (`src/fx.inl:150`) | not set | live | History weight before motion and rejection. Higher is smoother and ghosts more |
| `[post] TAAReject` | 1.0 (`src/fx.inl:151`) | not set | live | Strength of far-history rejection. 0 keeps the older behaviour (comment on the same line) |

Live: `[post]` keys are read again on `tyrender\reload` (`src/tyrender.cpp:598`) and when the overlay edits the `post` section (`src/overlay.inl:52`). `docs/PERFORMANCE.md` also lists `[post]` as live.

## Cost

- **Measured, one build, one GPU.** Build: "merged v0.3 build" (the source names no exact commit). GPU: AMD Radeon RX 7600 XT. Setting: 2560x1440, default view. The variant "TAA off (FXAA)" measured 11.49 ms against 11.09 ms for Medium, so +0.40 ms (`docs/STATUS_2026-10-13.md:38`). `docs/PERFORMANCE.md:53` gives the same +0.40 ms with the note "TAA is not the expensive part". The repo does not explain why turning TAA off would cost more. See N-07.2.
- **Launcher cost model (estimate, not a measurement).** AA 2 is charged 0.8 ms at 2560x1440 (`src/launcher_ui.inc:55`, comment at line 42). AA 0 is charged 0.3 and AA 1 is charged 0.5, but those values cover the whole finishing pass.
- **Per-pass time:** not measured in the repo. Per-pass timers under-count copies and resolves (`docs/PERFORMANCE.md:60`).
- **Memory (computed from formats, not measured):** at 2560x1440 the two RGBA16F history buffers take about 56 MiB and the two RGBA8 buffers (`texScene`, `texTmp`) take about 28 MiB.
- **What scales it:** pixel count. TAA reads a 3x3 neighbourhood and one depth sample per tap. FXAA reads a small cross and runs a short edge walk.

## Limits

- **No per-object motion vectors.** Reprojection uses the camera only (`src/fx.inl:97-98`). Characters, wind-moved grass and any object that moves on its own reproject to old positions. The clamp and rejection terms reduce the damage. They cannot remove it.
- **TAA needs the depth copy and a captured matrix for the frame** (`src/fx.inl:214`). When either is missing, TAA does not run for that frame. Then FXAA runs only if `[post] FXAA` is 1; otherwise the frame has no AA (inferred from `src/fx.inl:236`).
- **Surfaces that do not write depth** (see-through cards, water, some transparents) reproject against the wrong depth. The depth-guided rejection (`src/fx.inl:134-137`) depends on the depth copy. See [constraints-and-limits.md](../concepts/constraints-and-limits.md) for the depth gaps (inferred for this specific effect).
- **Jitter exists only with TAA.** FXAA-only modes have no sub-pixel sampling.
- **A failed post shader disables all post.** A compile or link failure of `kFXAA`, `kGrade` or `kTAA` sets `fx.failed` (`src/fx.inl:184`). `RunFX` then returns for the rest of the session (`src/fx.inl:178`). That turns off the grade and sharpen too. The failure is not retried.
- **No gamma on the clip box.** The variance multiplier is fixed at 1.0 (`src/fx.inl:116-117`). The planned gamma range of 0.75 to 1.25 (`docs/FIX_PLAN_20261016.md:20`) is not in the code.
- **Sharpen runs after AA.** Any residual aliasing or noise in the TAA output is sharpened too (inferred).
- **Shadow edges and jitter** can interact. Shadow noise is covered on the shadow page ([shadow-filter.md](shadow-filter.md)).

## Known issues

- **#2, TAA ghosting and trails.** `CHANGELOG.md`, v0.3.5 "Fixed": "TAA ghosting reduced (#2)". The README lists faint trails on fast animation as a known issue (`README.md:222`). The 2026-10-17 Ultra and High playtest reported heavy TAA ghosting (`docs/PLAN_CONSOLIDATED.md:14`). The current code already has depth-guided rejection and a motion-dependent blend (`src/fx.inl:128-137`). Earlier planning text says these were missing (`docs/PLAN_EXEC.md:10`). The Unreleased section of `CHANGELOG.md` has no entry for the depth-guided rejection. Status at this commit: partly addressed in code, not confirmed by the owner (N-07.1).
- **Grass ghosting.** Wind-moved grass has no motion vectors. History rejection was added for it (`src/fx.inl:130-133`, comment "R5 grass ghosting"). The R6 task asked for a check on consecutive frames while walking (`docs/agent-tasks/fx6.md`). No result is in the repo (N-07.1).
- **Shimmer during fast camera moves.** "Has not been measured formally" (`README.md:224`).
- **Ty aliasing.** The tessellation task file says a check was needed on skinned draws under TAA jitter (`docs/agent-tasks/tess2.md`, item 1). No result is recorded.

## How to verify

1. **Mode switch.** Set `[post] AA` to 0, 1, 2 and 3 in the game folder's `tyrender.ini` (`<game folder>\tyrender.ini`). Use the launcher's live reload or restart. At 100 percent zoom on a fence or rope edge, AA 0 and AA 1 show a still image. AA 2 and AA 3 show a changing sub-pixel pattern on a still camera.
2. **Log lines.** `[fx] initialised WxH` appears once per resolution (`src/fx.inl:195`). `[fx] first run ok at frame N` appears once (`src/fx.inl:261`). If neither appears, check `[post] Enable`, the `post_off` flag file (`src/fx.inl:6`, `:178`) and the F11 A/B state.
3. **Shader failure.** The shared compile helper logs `[ao] <name> fs compile FAILED` for every program, including the post programs (`src/ao.inl:450`). The `[ao]` tag on a `taa`, `fxaa` or `grade` line is a naming quirk, not an AO failure.
4. **Static stability.** `tools/motion.ps1` in static mode reports a temporal second difference. In the harness, the Ultra variant measured 0.566 and the TAA-off variant 1.461 (`docs/STATUS_2026-10-13.md:59`). That is an older build on the RX 7600 XT.
5. **Ghosting.** Record consecutive frames while Ty turns quickly and while walking through grass. Use the `fwd` option of `tools/motion.ps1` (`docs/agent-tasks/fx6.md`). This writer did not run the game. See [test-harness.md](../guides/test-harness.md).
6. **No debug view.** `src/fx.inl` has no debug output key (inferred from reading the file). Use A/B pairs instead ([ab-captures.md](../guides/ab-captures.md)).

## NEEDS INVESTIGATION

- **N-07.1 TAA trails and grass ghosting at this commit.** Method: run `tools/motion.ps1` with the `fwd` option on the fast Ty-turn view and the grass views. Compare AA 2 with AA 3 and AA 1. Take consecutive 100 percent crops. Record the build id from the launcher title. Owner review of the result.
- **N-07.2 Sign and size of the TAA-off cost.** Method: read the `[gpu]` lines in `tyrender.log` for AA 0, 1, 2 and 3 on one build, default view, several runs each. Record the GPU and driver.
- **N-07.3 Planned gamma range.** Method: ask the owner whether the gamma range in `docs/FIX_PLAN_20261016.md:20` was dropped on purpose. The code has no such parameter.

## Discrepancies

- First-pass `SHADERS.md` section 5.1 says FXAA "runs after TAA". With AA 2 and TAA running, it does not (`src/fx.inl:236`). It runs with AA 1 and AA 3.
- First-pass `ARCHITECTURE.md` line 54 gives TAA, then FXAA, then grade as one fixed order. That holds for AA 3 only.
- First-pass `SHADERS.md` section 5.2 lists depth-guided rejection as planned. It is in the code (`src/fx.inl:134-137`). The gamma range is still not in the code.
- `docs/PLAN_EXEC.md:10` says depth-guided reject and a velocity-adaptive blend are missing. The code is newer. Code is the reference here.
- The `[ao]` log tag is used for post-program failures (`src/ao.inl:450`).
- The +0.40 ms figure appears in `docs/PERFORMANCE.md:53` (labelled "TAA off") and `docs/STATUS_2026-10-13.md:38` (labelled "TAA off (FXAA)"). Same number, different labels, and the sign is not explained in either.

## See also

- [Grade, sharpen and tonemap](grade-and-sharpen.md)
- [Tessellation and surface relief](tessellation-and-relief.md)
- [Frame anatomy and pass order](../how-it-works/04-frame-anatomy-and-pass-order.md)
- [Constraints and limits](../concepts/constraints-and-limits.md)
- [Settings: post, shadows, light, PBR, tessellation](../reference/settings-post-shadows-light-pbr-tess.md)
- [Known issues](../decisions/known-issues.md)
