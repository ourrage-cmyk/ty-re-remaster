# Tessellation and surface relief

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributors), B (testers who tune `[tess]`), C (reviewers checking the character rule).

## Purpose

Two related mechanisms give flat painted textures some depth without new art assets. Both take their height from the texture's own brightness.

- **Surface relief** (fragment shader). Ground, rock and fence surfaces get a parallax shift and a perturbed normal (the vertex-lit "lite" set is described at `src/pbr.inl:43`). This is the part that is on in shipped Medium. The shading code is in `src/pbr.inl`. This page covers only its distance and depth controls, and how it connects to tessellation. Full shading detail is on [pbr-surface-detail.md](pbr-surface-detail.md).
- **Geometry tessellation** (tessellation control and evaluation stages). World triangles are split on the GPU and pushed inward along the normal by the same height field. It is on only when `[tess] Scope` is `all` or `env`, which the Ultra preset uses. The shipped file uses `skin`, which means no geometry tessellation.

Characters are never tessellated (`src/tess.inl:106`). Their fragment relief is switched off through the texture tags (`src/pbr.inl:76`). Design intent: the feet must never sink into the ground, so relief only moves inward (`src/tess.inl:354`).

The height field is `kTyHeightGLSL` (`src/tess.inl:163-173`). It is shared by the fragment shader (`src/pbr.inl:70`) and the tessellation evaluator (`src/tess.inl:312`). It uses no procedural noise (`src/tess.inl:9`); see Discrepancies for one unused helper.

## How it works

### A. Geometry tessellation (only with Scope all or env)

1. **Vertex shader rewrite at compile time.** `RewriteVSForTess` (`src/tess.inl:98-159`) changes a vertex shader only if all of these hold: `[tess] Enable` is 1 (line 99); the shader matches the standard template (lines 100-101); it has normals and a lit or shadow-normal define (lines 101-105); it is not skinned (line 106); Scope is `all` or `env` (line 107); and it is not alpha-tested foliage (line 108). The outputs are renamed to `tcIn_*` and a world-position output is added (lines 154-157). The rewritten source is test-compiled first (`src/tyrender.cpp:569`). If that fails, the original is kept.
2. **Stage generation at link time.** The `glLinkProgram` hook calls `TessLink` (`src/tyrender.cpp:333`, `:579-587`). That generates a tessellation control stage and an evaluation stage, both `#version 400` (`src/tess.inl:381-408`). Stage pairs are cached by the varying signature only (`src/tess.inl:399-404`), so a config change does not regenerate them until restart (inferred from the cache key).
3. **Control stage** (`MakeTCSWorld`, `src/tess.inl:259-305`). For each patch edge the level is the smaller of two rules: edge length divided by `TargetEdge` (4 world units), and a screen-size rule that uses `TargetPx` (12 pixels) and the distance to the camera (`src/tess.inl:266-273`). Both rules use the edge midpoint, so neighbouring patches agree on shared edges and do not crack (comment at `src/tess.inl:256-258`). Levels are clamped to `WorldMaxLevel` (48, line 272). Patches outside the frustum, or back-facing when `Cull` is 1, get zero levels (`src/tess.inl:284-300`). The layout is `fractional_odd_spacing` (`src/tess.inl:310`), so levels change smoothly with distance.
4. **Evaluation stage** (`MakeTESWorld`, `src/tess.inl:309-377`). It interpolates the vertices and applies Phong tessellation (Boubekeur and Alexa, 2008). The point is projected onto each vertex's tangent plane and the results are blended (`src/tess.inl:331-335`). The smoothing strength fades between `FadeStart` and `FadeEnd` (1500 to 4000 world units) and is capped at `MaxSmoothing` (3 units, lines 334-335). Surfaces facing up may only smooth downward (lines 336-337), so walkable ground cannot rise.
5. **Relief in the evaluation stage** (`src/tess.inl:340-361`). Three mip levels of the diffuse texture's luminance give a height (`TyH3`, lines 169-171). The depth is that height, inverted, raised to `ReliefCurve` (line 354). The amplitude is `ReliefDepth` times `GroundRelief` on up-facing surfaces and `PropRelief` elsewhere (line 355). A soft knee limits strong relief (line 356). The relief fades with distance between `ReliefFadeStart` and `ReliefFadeEnd` (line 348) and fades at patch borders over `EdgeFade` world units (line 357). The displacement is `w -= nI * amp` (line 358): inward only.
6. **Legacy displacement path.** The older plus-or-minus displacement (`src/tess.inl:362-370`) runs only when `ReliefDepth` is 0 (`src/tess.inl:318`).
7. **Drawing.** Tessellated programs accept only `GL_PATCHES` (`src/tyrender.cpp:268`, `:271`). `TessPatchMode` sets three vertices per patch (`src/tyrender.cpp:574-578`). Draws that are not triangles are skipped and counted, and the count appears in each `[frame]` line (`src/tyrender.cpp:618`).
8. **Culling input.** `TessDrawPrep` tells the control stage which screen winding the engine culls (`src/tess.inl:431-441`).

### B. Fragment surface relief (on in shipped Medium)

1. **Which shaders.** The rewrite applies to lit standard variants (`PbrEligible`, `src/pbr.inl:38-42`) and to vertex-lit world surfaces with normals (`PbrLite`, `src/pbr.inl:45-46`). Skinned shaders are excluded unless the texture tags say otherwise (`src/pbr.inl:40`).
2. **Gate.** Relief runs only when `ReliefDepth` is above 0 (`src/pbr.inl:84`). `[pbr] Enable` must also be 1 (`src/pbr.inl:39`, `:46`, `:214`). `[tess] Enable` is not checked here (see Discrepancies).
3. **Parallax.** Up to 12 steps (`ReliefSteps`, default 8, clamped 3 to 12, `src/tess.inl:66`) march along the view direction through the height field (`src/pbr.inl:99-115`). The depth used is 70 percent of `ReliefDepth` (`src/tess.inl:65`). The shift is reduced by 60 percent when the geometry is tessellated (`src/pbr.inl:101`). The effect fades at grazing angles (`src/pbr.inl:98`) and with distance (`src/pbr.inl:83`).
4. **Normal.** The lighting normal is perturbed from central differences of the height field (`src/pbr.inl:117-125`).
5. **Character exclusion.** The per-draw `tyTex` uniform switches relief off for character textures (`src/pbr.inl:76`, `tyOn`). The tag source is described on [texture names and tags](../how-it-works/07-texture-names-and-tags.md).

### C. Per-frame uniforms

`TessApply` (`src/pbr.inl:175-188`) and `PbrApply` (`src/pbr.inl:189-203`) push the relief depth, fade distances, steps and mip bias every time a program is used. This is why most relief keys change live (see Settings).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Config loader and code defaults | `src/tess.inl:33-73` | every `[tess]` key |
| Vertex rewrite gate | `src/tess.inl:98-108` | Enable, template, skinned, Scope |
| Height field | `src/tess.inl:163-173` | `kTyHeightGLSL`, luminance at 3 scales |
| Control stage (world) | `src/tess.inl:259-305` | `MakeTCSWorld` |
| Evaluation stage (world) | `src/tess.inl:309-377` | `MakeTESWorld` |
| Stage cache and link | `src/tess.inl:381-425` | `CompileStage`, `TessBeforeLink`, `TessAfterLink` |
| Draw-side hooks | `src/tyrender.cpp:566-587`, `:268`, `:271` | rewrite, link, GL_PATCHES |
| Fragment relief rewrite | `src/pbr.inl:38-125` | parallax and normal |
| Per-program uniforms | `src/pbr.inl:175-203` | `TessApply`, `PbrApply` |
| Relief on/off gate | `src/pbr.inl:39`, `:84`, `:214` | not `[tess] Enable` |
| GL 4.0 gate | `src/tess.inl:72`, `:397` | `TessVersionGate` |
| Entry point check | `src/tess.inl:87-95` | `glCompileShader` and friends |

## Settings

Keys that matter here. The full table is in [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[tess] Enable` | 1 (`src/tess.inl:35`) | 1 (`default_tyrender.ini:42`) | restart | Gates the geometry rewrite only (`src/tess.inl:99`). Not the fragment relief |
| `[tess] Scope` | env-detail (`src/tess.inl:36`) | skin (`default_tyrender.ini:43`) | restart | `all` or `env` turns on world tessellation. Any other value turns it off |
| `[tess] ReliefDepth` | 3 (`src/tess.inl:48`) | not set (so 3) | live | Crack depth in world units, for geometry and for fragment relief at 70 percent. 0 turns fragment relief off |
| `[tess] GroundRelief`, `PropRelief` | 1.0, 0.7 (`src/tess.inl:55-56`) | not set | live | Depth multipliers for up-facing and other surfaces |
| `[tess] ReliefFadeStart`, `ReliefFadeEnd` | 700, 1600 (`src/tess.inl:57-58`) | not set | live | Distance over which relief fades out |
| `[tess] ReliefSteps` | 8, clamped 3 to 12 (`src/tess.inl:66`) | not set | live | Parallax steps in the fragment shader |
| `[tess] TargetEdge`, `TargetPx` | 4, 12 (`src/tess.inl:47`, `:38`) | not set | live | Size targets for tessellation levels (world and screen) |
| `[tess] WorldMaxLevel` | 48 (`src/tess.inl:68`) | not set | live | Cap on the tessellation level of world patches |
| `[tess] Cull` | 1 (`src/tess.inl:70`) | not set | restart | Frustum and back-face patch culling |
| `[tess] MaxLevel` | 8 (`src/tess.inl:71`) | 8 (`default_tyrender.ini:44`) | restart | Used only by the legacy control stage. See Discrepancies |
| `[pbr] Enable` | 1 (`src/pbr.inl:18`) | 1 (`default_tyrender.ini:55`) | restart | Gates fragment relief, PBR and bump |
| `[lod] TessEnable` | 0 (`src/tess.inl:61`) | 0 (`default_tyrender.ini:30`) | live | Distance fade of tessellation levels. Has an effect only with Scope `all` or `env` |
| `[portability] TessVersionGate` | 1 (`src/tess.inl:72`) | 1 (`default_tyrender.ini:102`) | at link | Skips tessellation below GL 4.0 |

Live versus restart. Keys used when a shader is rewritten (`src/tyrender.cpp:381`) need a restart. Keys sent as per-frame uniforms (`src/pbr.inl:184-187`, `:200-201`) update live. Keys baked into generated stage text (`src/tess.inl:317-318`) apply to stages generated after the restart. This matches `docs/PERFORMANCE.md:85`, which lists `[tess] Enable Scope MaxLevel` as restart-only.

## Cost

Figures carry build and GPU where the source gives them. The repo has no per-stage timing.

- **Scope all or env, world tessellation.** +6.22 ms over Medium (Medium reference 11.09 ms, frame 17.31 ms), one build of v0.3 merged, RX 7600 XT, 2560x1440, default view (`docs/STATUS_2026-10-13.md:34`). README and performance page give "about +6.2 ms" for the same option (`README.md:176`, `docs/PERFORMANCE.md:57`). README also says "about +4 to +6 ms, Ultra only" (`README.md:310`).
- **Round 2 tessellation variant.** "tess v4 (Scope=all, relief and PBR extras)" at +4.08 ms against a target of 3 ms, with 95 programs tessellated and no failures (`docs/results/VERDICTS.md:31`). The build id is not given in that row.
- **Round 4.** Default (Scope skin plus detail layer) +0.3 ms. Scope all about +3.9 ms "for little extra" (`docs/results/VERDICTS.md:43`). Build not named in that row.
- **Launcher model (estimate).** World tessellation is 4.2 ms plus 0.02 per level above 8. The detail layer is 0.3 ms (`src/launcher_ui.inc:45`, `:51`). This is a fitted model, not a measurement.
- **Relief off.** "-0.05 ms" for relief and tess off (`docs/STATUS_2026-10-13.md:37`, `docs/PERFORMANCE.md:52`). This conflicts with the +0.3 ms detail-layer figure above. See Discrepancies.
- **What scales it.** Triangle count on screen (for the control stage) and the number of tessellated programs. The screen-size rule scales with resolution (`src/tess.inl:270`). Fragment relief costs more on screen-filling surfaces and with more parallax steps (inferred).

## Limits

- **World triangles only.** Skinned draws are never tessellated (`src/tess.inl:106`). Non-triangle draws on a tessellated program are dropped (`src/tyrender.cpp:268`, `:271`).
- **Height is brightness, not geometry.** Relief comes from the texture's own colour. Painted shapes can read as relief and fine patterns can read as cracks (inferred from `src/tess.inl:162`).
- **Low-resolution textures give weak relief.** The level ground texture is very low resolution, and the HD texture pack does not replace it (`docs/results/VERDICTS.md:31`, root cause). The README says the same (`README.md:223`).
- **Thin objects get little benefit.** Rope and grass sources are flat (`README.md:221`).
- **Inward only.** Relief moves surfaces down, never up (`src/tess.inl:358`). Silhouettes do not grow.
- **GL 4.0 or newer for geometry.** Earlier contexts skip the stages (`src/tess.inl:397`). Fragment relief has no GL version gate of its own (inferred).
- **Standard template only.** Other vertex shaders are not rewritten (`src/tess.inl:100-101`).
- **Cost ceiling.** Scope all is heavy and misses the 3 ms target measured in round 2 (`docs/results/VERDICTS.md:31`).

## Known issues

- **Seam at patch borders (Scope all).** "Dark seam gap along a long ground-piece border" and "jaggy paint edges on the rug" were listed as known issues in the task file (`docs/agent-tasks/tess.md`, opening state paragraph). Round 4 still lists "Scope=all seam" as open (`docs/results/VERDICTS.md:43`). `EdgeFade` (`src/tess.inl:357`) is the mitigation.
- **Geometry relief barely visible.** Whole-image difference of 1.4 to 2.9 of 255 with Scope all (`docs/STATUS_2026-10-13.md:67`). The visible gain came from the fragment detail layer (same line, and `docs/results/VERDICTS.md:43`).
- **Dark speckle and rail stipple.** Listed as open after round 4 (`docs/results/VERDICTS.md:43`).
- **Characters.** Ridges and aliasing on Ty and a koala were the reason characters were excluded (`docs/recording/FINDINGS_AND_PLAN.md:17`). The exclusion is in place (`docs/recording/INDEX.md:20`). The bag and some props were not verified (`docs/recording/INDEX.md:58`, marked partial).
- **Ground relief was positive in playtest.** The user recording summary records ground tessellation as liked (`docs/recording/FINDINGS_ROUND2.md:5`). The 4a2ef95 playtest says "tessellation floor lift is on" (`CHANGELOG.md`, Playtest of 4a2ef95).

## How to verify

1. **Shipped state.** With the shipped `[tess] Scope=skin` and `[tess] ReliefDepth` at 3, no `[tess] program N linked WITH tessellation` line should appear (no program is rewritten). Ground and fence surfaces should still show fragment relief.
2. **Turn on geometry tessellation.** Set `[tess] Scope=all` in `<game folder>\tyrender.ini` and restart. Expect `[tess] program N linked WITH tessellation` lines (`src/tess.inl:420`). The `[frame]` line then shows `tess progs=N` and `skipped non-triangle tess draws=N` (`src/tyrender.cpp:618`).
3. **Density view.** `[tess] Debug=1` darkens vertices near sub-triangle edges (`src/tess.inl:248`, `:374`). This works only where the program has a colour varying.
4. **Forced failure.** `[debug] ForceFallback=8` makes every tessellation stage fail to compile (`src/tess.inl:382`; bit meaning at `src/ao.inl:437`). Expect `[fallback] FORCED tessellation stage compile failure` and programs that link without tessellation.
5. **Real stage failure.** A compile failure logs `[tess] stage 0x8e88 compile FAILED` (TCS) or `0x8e87` (TES) with the full source (`src/tess.inl:387`). A failed tessellated link logs `[tess] program N tessellated link FAILED -> restoring original` (`src/tess.inl:421`).
6. **Old GPU.** On GL below 4.0, `[tess] GL below 4.0: tessellation stages skipped` appears once (`src/tess.inl:397`).
7. **Fragment relief off.** Set `[tess] ReliefDepth=0`. The fragment relief stops (`src/pbr.inl:84`), and the geometry path uses the legacy displacement (`src/tess.inl:318`). Compare with an F11 pair.
8. **A/B within a session.** `[tess] ABToggle=1` and edits to `tyrender\cam.txt` flip relief and PBR extras. The log shows `[tess] AB toggle: live=N` (`src/pbr.inl:210`). F11 pairs are labelled `shaders-on` and `shaders-off` (`src/abshot.inl:121`, `:128`). See [ab-captures.md](../guides/ab-captures.md).
9. **Feet.** With geometry tessellation on, check that Ty's feet do not float or sink. The check is a crop of the feet at several poses (`docs/agent-tasks/tess.md`, step 2). Not run by this writer.

## NEEDS INVESTIGATION

- **N-07.7 Does `[tess] Enable=0` remove fragment relief?** The code says no (`src/pbr.inl:39`, `:46`, `:84`, `:214`; `src/tess.inl:99`). Method: at Medium, set `[tess] Enable=0` and then `ReliefDepth=0`. Compare the two crops with `ABToggle` or F11 pairs. If they differ, the code reading is wrong. This also decides whether the launcher's "Surface detail" checkbox does anything to fragment relief (`src/launcher_ui.inc:440`, `:64`).
- **N-07.8 Does `MaxLevel` change anything in world tessellation?** The code says no (`src/tess.inl:186` is the only use, in the legacy control stage). Method: with Scope all, run `MaxLevel=8` and `MaxLevel=24`, then `[tess] Debug=1`. Compare the density image and the `[gpu]` timings. The launcher cost model assumes it does (`src/launcher_ui.inc:51`).
- **N-07.9 Are non-triangle draws skipped in real play?** Method: with Scope all, read the `skipped non-triangle tess draws=` value in `tyrender.log` `[frame]` lines on the default view and on the heavy views.
- **N-07.10 What "REC round 2026-10-13" refers to.** The comment at `src/tess.inl:6` cites it. The repo does not define "REC". Inferred to mean the recording findings in `docs/recording/` (F11 and T51, `docs/recording/FINDINGS_AND_PLAN.md:17`, `:21`). Method: owner confirmation.
- **N-07.11 One-build cost for each tessellation option.** The repo has four different figures (see Cost). Method: GPU timers on one build, default view, with `[tess] Scope` skin, detail off, and all. Plan item N-20.
- **N-07.12 Whether painted lettering reads as relief.** The design says it can (`docs/agent-tasks/tess2.md`, item 2). Method: visual crops of the sign and rug at 100 percent.

## Discrepancies

- **Default Scope in three places.** Code default is `env-detail` (`src/tess.inl:36`). README says `env-detail` (`README.md:310`). Shipped file says `skin` (`default_tyrender.ini:43`), and the launcher reads `skin` as its default (`src/launcher_ui.inc:74`). `docs/RESUME.md` notes the same mismatch. Both values give no geometry tessellation, so the rendered result is the same.
- **`[tess] Enable` meaning.** README says it turns relief "on or off" (`README.md:309`). Code: it gates only the geometry rewrite (`src/tess.inl:99`). Fragment relief is gated by `[pbr] Enable` and `ReliefDepth` (`src/pbr.inl:39`, `:84`). See N-07.7.
- **`MaxLevel` is inert for world tessellation.** README says `MaxLevel` controls detail (`README.md:311`). The launcher writes it per preset (4, 8, 8, 24, `src/launcher_ui.inc:11-14`) and its cost model scales with it (`src/launcher_ui.inc:51`). The world control stage clamps to `WorldMaxLevel` through `tyTP.w` (`src/tess.inl:272`, `src/pbr.inl:184`). `g_tsMaxLevel` is read only by the legacy stage (`src/tess.inl:186`). See N-07.8.
- **`Alpha` and `EdgeLength` are inert for world tessellation.** `Alpha` (0.7) is used only by the legacy skinned evaluation stage (`src/tess.inl:226`). World tessellation uses `AlphaWorld` (`src/tess.inl:317`). `EdgeLength` (25) is used only by the legacy control stage (`src/tess.inl:185`). README lists `Alpha` as a detail key (`README.md:311`).
- **`Displacement` and `DisplacementLod` are inert with default relief.** They matter only when `ReliefDepth` is 0 (`src/tess.inl:318`, `:362`).
- **`[lod] TessEnable` comment.** `default_tyrender.ini:19` says tessellation distance LOD "is not wired". Lines 29-30 define the key. The key is read (`src/tess.inl:61`) and sent (`src/pbr.inl:185`), and it has an effect only with Scope all or env (inferred).
- **"No procedural noise" claim.** `src/tess.inl:9` says the height field uses no procedural noise. The evaluation-stage text also defines `TyH` and `Nz3`, a hash-noise helper (`src/tess.inl:323-325`). Neither is called (grep of `src/`). No runtime effect (inferred).
- **Glossary.** First-pass `GLOSSARY.md` says `skin` "tessellates character skin only, per the shipped ini". That is wrong. `skin` turns geometry tessellation off (`src/tess.inl:36`, `:107`).
- **Architecture.** First-pass `ARCHITECTURE.md` line 47 says tessellation is applied to selected draws. Under the shipped `skin` scope no geometry draw is tessellated (inferred from `src/tess.inl:107`). Fragment relief is applied to many draws.
- **Cost figures.** Four different figures for Scope all: +6.22 (`docs/STATUS_2026-10-13.md:34`), +4.08 (`docs/results/VERDICTS.md:31`), +3.9 (`docs/results/VERDICTS.md:43`), and "+4 to +6" (`README.md:310`). The detail layer is +0.3 ms (`docs/results/VERDICTS.md:43`) against -0.05 ms for relief off (`docs/STATUS_2026-10-13.md:37`).
- **ForceFallback bit 8.** `docs/PERFORMANCE.md:65` says "8 relief". The code says bit 8 forces a tessellation stage failure (`src/tess.inl:382`; `src/ao.inl:437`). No fallback bit exists for fragment relief.
- **Launcher slider key.** `src/launcher_ui.inc:31` writes `ReliefDepth` for "Surface relief". The dead `src/ui_part_a.inc:30` writes `Displacement`. `TYLauncher.c` includes only `launcher_ui.inc` (`src/TYLauncher.c:79`). Current behaviour follows `launcher_ui.inc`.

## See also

- [Temporal and fast anti-aliasing](taa-fxaa.md)
- [Grade, sharpen and tonemap](grade-and-sharpen.md)
- [Surface detail and PBR shading](pbr-surface-detail.md)
- [Shader source rewriting](../how-it-works/05-shader-source-rewriting.md)
- [Fallbacks and failure handling](../how-it-works/11-fallbacks-and-failure-handling.md)
- [Settings: post, shadows, light, PBR, tessellation](../reference/settings-post-shadows-light-pbr-tess.md)
