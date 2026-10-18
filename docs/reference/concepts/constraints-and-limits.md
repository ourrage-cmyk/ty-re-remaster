# Constraints and limits

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B, C.

## Purpose

This is the honest "cannot" page. Each limit gives the cause, the consequence, any workaround, and a link to the page that owns the detail. It covers the forward-rendering model, the screen-space data the effects use, depth gaps, the OpenGL version ceiling, the 32-bit address space, the single tested card, temporal artefacts, and the test and merge tooling.

## How it works

The mod has no world data. It has the frame the game has already drawn, plus the depth copy the game makes. Every effect is a screen-space or view-space pass that reads those buffers. Most limits below follow from that one fact.

## Limits

### L1. Forward renderer, no G-buffer (Q087)

- **Cause.** The game draws lit geometry in its own shader programs, one pass per object, with lights applied in each shader (`src/pbr.inl:4-5`, `src/pbr.inl:60-61`). The mod's own depth-derived view normals are built only inside its GI pass (`src/gi2.inl:5`).
- **Consequence.** New lighting cannot be added as a cheap second light loop. The plan states this directly: "every lit fragment shades itself" and per-draw dynamic lights would cost the number of lights times every lit pixel (`docs/MATERIAL_CLASSIFICATION_PLAN.md:81-83`). Emitter light is therefore planned as a half-resolution splat (`docs/MATERIAL_CLASSIFICATION_PLAN.md:84-86`).
- **Why forward, rationale.** No decision record in the repo states why. **NI** (a new gap, not in the plan's N-list; method: ask the owner or find the decision in the planning notes). **inferred:** the game's engine is forward, so a deferred path would have to rewrite every lit shader and its draw submission, which the non-goals rule out (`docs/GOAL.md:22`).
- **Workaround.** None inside the mod. The planned emitter path is the workaround for one case.
- **What deferred would take.** Not costed in the repo. The research note lists world-space alternatives instead (see L2).
- Owned by: [how-it-works/04-frame-anatomy-and-pass-order.md](../how-it-works/04-frame-anatomy-and-pass-order.md).

### L2. Screen-space data only, no world-space data (Q160)

- **Cause.** AO, GI and the colour part of fog read the depth buffer and the camera (`docs/research/WORLD_SPACE_AO_GI_FOG.md:16-20`). The depth copy is the only geometry input (`src/ao.inl:1-3`, `src/atmos.inl:1-4`).
- **Consequence.** Anything off screen is missing from the calculation. Anything that moves in and out of view changes the result (disocclusion). The research note traces the flicker and the colour drift to this (`docs/research/WORLD_SPACE_AO_GI_FOG.md:12`).
- **Workaround.** Candidate ideas only, none implemented: a baked world AO field, irradiance probes on a world grid, a fog colour fixed to sun and height, and a distance-based AO fade (`docs/research/WORLD_SPACE_AO_GI_FOG.md:22-30`). The AO fade by view distance already exists as `[ao] FadeStart` and `FadeEnd` (`src/ao.inl:418`, `src/ao.inl:507-508`), default off (`FadeEnd` 0).
- Owned by: [shaders/ao-gtao.md](../shaders/ao-gtao.md), [shaders/gi-hiz.md](../shaders/gi-hiz.md), [decisions/research-notes.md](../decisions/research-notes.md).

### L3. Depth gaps for water, transparent and foliage (Q161)

- **Cause (inferred).** See-through draws (water, foliage cards) can leave the depth buffer showing what is behind them, so the fog pass sees that depth. The mod's own comments describe the effect; the exact mechanism is covered on the fog page.
- **Consequence.** Water and foliage are not fogged the same way as opaque surfaces. `src/default_tyrender.ini` lines 79-80 say water is drawn after the volumetric fog pass and is otherwise unfogged.
- **Workaround.** `[fog] SeeThruDepth=1` writes depth for see-through draws before the fog pass (`default_tyrender.ini:74-75`, code at `src/tyrender.cpp:269` and `src/atmos.inl:321`). Off by default. Untested (`CHANGELOG.md:16`). `[fog] WaterBoost` and `WaterNames` add fog to named water draws (`CHANGELOG.md:24-26`, `src/tex.inl:76-78`). Also off or 1.0 by default.
- Owned by: [shaders/water-fog-and-decals.md](../shaders/water-fog-and-decals.md), [shaders/atmosphere-fog.md](../shaders/atmosphere-fog.md).

### L4. OpenGL version ceiling and per-effect gates (Q023, Q163)

- **Cause.** The game runs an OpenGL compatibility context (see [game-and-engine-facts.md](game-and-engine-facts.md)). The mod's passes need GLSL 330, tessellation needs GL 4.0 and GLSL 400, and the froxel atmosphere needs GL 4.3 and GLSL 430 (`src/fx.inl:8`, `src/tess.inl:179`, `src/atmos.inl:160`, `src/atmos.inl:258-260`).
- **Consequence.** On lower hardware some effects are off. The gates are listed in [game-and-engine-facts.md](game-and-engine-facts.md).
- **Fallback behaviour.** The README says a failing effect switches itself off and the renderer "does not fall back to a slower path" (`README.md:265`). The code agrees for AO (`src/ao.inl:546`) and for the atmosphere in the default configuration (`src/atmos.inl:371`, "no heavy ray-march fallback"). It does not agree in full: when the froxel path is not used (`[atmos] Froxel=0`), or when it fails and `[atmos] ContactShadows` is above zero, the atmosphere draws its per-pixel ray-march path instead (`src/atmos.inl:371`, `src/atmos.inl:453-454`, `src/atmos.inl:493-496`). See the Discrepancies section.
- **Workaround.** Use `[portability] TessVersionGate=1` (default) so tessellation stays off below GL 4.0 (`default_tyrender.ini:101-102`). Use the presets to lower the cost.
- **Tested.** One card only (see L8).

### L5. Engine light count (Q159)

- **Cause.** The number of lights the game's shaders accept is set inside the game's shader code, which is not in the repo.
- **Consequence.** The emitter plan depends on that limit. `docs/PLAN_NEXT.md:35` says "forward-render light cap is 16". **doc.** No code in the repo sets a cap.
- **Status.** **NI** (N-11). Method: read the omni-light loop and uniform array sizes in a dumped shader (`[debug] DumpShaders=1`).

### L6. 32-bit address space

- **Cause.** The game is a 32-bit executable with a 2 GB address space by default (`README.md:200`).
- **Consequence.** With the HD texture pack, the driver and the renderer's buffers, the game can run out of memory ("CMKHeap - Out of memory", `README.md:200`, `src/patch_imports.ps1:72-73`).
- **Workaround.** The patched copy `TY_tyrender.exe` is marked Large Address Aware, which gives a 4 GB address space on 64-bit Windows (`src/patch_exe.inc:38`, `src/patch_imports.ps1:73`). The original `TY.exe` is not changed (`src/patch_exe.inc:7`). The flag is not set on the original, so the original keeps the 2 GB limit (inferred).
- **Limit remaining.** The mod adds its own buffers. Memory use on very large texture packs is not measured in the repo.

### L7. Characters are excluded from relief (Q166)

- **Cause.** Relief on a character would move the feet into the ground. The goal says characters must render correctly, with feet on the ground (`docs/GOAL.md:8`, `docs/GOAL.md:17`).
- **Consequence.** Ty and NPCs get no bump, no relief and no tessellation (`src/tess.inl:6-7`, `src/tess.inl:107`, `src/pbr.inl:76`, `src/tex.inl:202`). Characters are classed by their texture tags (see [material-model.md](material-model.md)).
- **Workaround.** Character AO and character GI have their own keys (`README.md:284-290`). The character policy is a row in the policy table, not a special case (`docs/MATERIAL_CLASSIFICATION_PLAN.md:76`).

### L8. One tested card, untested platforms (Q024, Q163)

- **Cause.** Only one owner-operated machine has been used (`README.md:11`, `docs/START_HERE.md:7`).
- **Consequence.** The NVIDIA, Intel, AMD-other and Linux or Proton behaviour is unknown. The README asks for logs from those owners (`README.md:230`).
- **Status.** Minimum hardware is not stated (proposed N-29). Vendor-specific behaviour is N-21, Linux and Proton is N-10, and the Steam interplay is N-9.

### L9. Frame cost ceiling

- **Cause.** Each effect adds GPU time. The budget the goal sets is 16.7 ms (60 fps) at 2560x1440 for the default preset (`docs/GOAL.md:11`, `docs/GOAL.md:16`).
- **Consequence.** Medium is about 12 ms on the test card. High (about 18 ms) and Ultra (about 31 ms) are above the 60 fps budget (`docs/PERFORMANCE.md:14-17`).
- **Workaround.** Presets, sliders, and the launcher's per-option cost tags. The launcher tags are derived from measured costs (`docs/PERFORMANCE.md:88`).
- **Limit.** Per-pass timers under-count hidden full-resolution copies and resolves, so the frame total is the figure to use (`docs/PERFORMANCE.md:60`).

### L10. TAA ghosting and trails (Q162)

- **Cause.** Temporal anti-aliasing reuses the previous frame. A moving object leaves its old colour in the history.
- **Consequence.** Ghosting and trails on fast animation (`README.md:222`). The v0.3.5 change reduced ghosting (`CHANGELOG.md:51`, issue #2).
- **Workaround.** The TAA history reject, and the choice `[post] AA` between TAA, FXAA or both (`README.md:305`). The rejection details are in [shaders/taa-fxaa.md](../shaders/taa-fxaa.md) and were not re-verified on this page.

### L11. Temporal flicker of AO, GI and shadows

- **Cause.** Screen-space samples and temporal accumulation change with the camera and the frame count.
- **Consequence.** Flicker in some playtests, mainly on High and Ultra (`docs/research/WORLD_SPACE_AO_GI_FOG.md:8`, `README.md:163`). The AO flicker is said to be fixed on `4a2ef95` (`README.md:163`). Blue flicker on Ty in water is traced to GI sampling water (`README.md:213`, `CHANGELOG.md:36`).
- **Workaround.** Use Medium if you see flicker (`README.md:163`). `[debug]` flag files and the debug views help bisect it (see [guides/debugging.md](../guides/debugging.md)).

### L12. Shadow-filter and bright-line artefacts

- **Cause.** Shadow taps were sampled inside divergent branches, so the GPU picked the wrong level or filter. The owner's v0.3.4 note records this as the root cause and the fix as an explicit LOD sample (`docs/RESUME.md:68`).
- **Consequence.** Thin bright lines on character shadows and a grid-like look (`README.md:215-217`, `docs/RESUME.md:31`).
- **Status.** Partly addressed in v0.3.4. The owner's playtest of 2026-10-16 reports the lines fixed on Medium (`docs/PLAN_CONSOLIDATED.md:13`). High and Ultra were not yet tested at that time.

### L13. Shader text rewriting is fragile

- **Cause.** The mod changes the game's shader source by text replacement at compile time (`src/pbr.inl:60-61` looks for exact strings; `src/light.inl:37` checks for a string). If the game's shader text changes, the match fails.
- **Consequence.** The effect is switched off for that program, or the program is left stock. The log records it.
- **Workaround.** Test-compile the rewritten shader and fall back to the original text if it fails (`src/tyrender.cpp:380`, `src/tyrender.cpp:390`, `TestCompile` declared at `src/tyrender.cpp:360`). The fallback and log lines are in [how-it-works/05-shader-source-rewriting.md](../how-it-works/05-shader-source-rewriting.md).

### L14. Mouse and focus in the test harness

- **Cause.** The game's menus select by mouse hover. The game window must be in front for keys to reach it (`docs/START_HERE.md:73`).
- **Consequence.** Automated runs need the mouse left alone and a focus helper. This affects testing, not play. Ask the test owner before a manual run.
- **Owned by:** [guides/test-harness.md](../guides/test-harness.md).

### L15. Build and merge tooling

- **Cause.** The merge script cannot copy new files, and whole-file line-ending conflicts fail (`docs/START_HERE.md:71`).
- **Consequence.** New files must be copied by hand. A CRLF-only change can block the merge.
- **Encoding hazards.** Several source files start with a byte-order mark, for example `src/tex.inl:1`, `src/fx.inl:1`, `tools/rkvtool.ps1:1`. `src/tyrender.cpp:1` contains mis-encoded characters in its first comment line. Keep these in mind when editing with a tool that re-encodes.
- **Hard-coded owner paths.** `src/gen_tex_tags.ps1:4`, `:10` and `:21` read and write fixed locations on the owner's machine. The build and release scripts have the same problem (N-12, N-13). This is a limit on building outside the owner's setup. It is not an instruction to users.

### L16. Texture labels are the owner's list

- **Cause.** Texture behaviour depends on the tag list, which the owner filled in by hand (`data/texture_tags.csv`, 1933 rows).
- **Consequence.** A mislabelled texture gets the wrong behaviour, and an untagged texture is "unknown" (see [material-model.md](material-model.md)).
- **Workaround.** The per-user override file `tyrender\texture_tags.csv` (`src/tex.inl:83-84`).

### L17. Non-goals (Q164, Q165)

The goal document lists these as out of scope (`docs/GOAL.md:21-22`):

- Baking textures.
- Replacing the engine's draw submission (AZDO).
- Vulkan or DirectX ports.
- Changing game logic.
- HD texture authoring.

Also not implemented, because no code or design for them exists in the repo (checked by search for reflection, SSR, ray tracing): screen-space or real reflections, ray-traced shadows or GI, shadow-casting new lights, and anything that needs new game assets. **inferred** from the absence of code.

## Cost

Costs are on the reference pages (see `reference/presets.md`, planned). The main figures are in [what-is-this.md](what-is-this.md) and in `docs/PERFORMANCE.md`.

## Known issues

Each limit above links to its issue. The issue list is in [decisions/known-issues.md](../decisions/known-issues.md).

## How to verify

- L4 (gates): open `tyrender.log` and search for `[fallback]`, `[atmos] froxel`, `[tess] GL below 4.0`.
- L6 (memory): the launcher and installer write the Large Address Aware patched copy; check the file header flag in `TY_tyrender.exe` with a PE tool.
- L3 (fog depth): run the autoplaytest variant `fog_seethru1` (`tools/playtest/autoplaytest.ps1:56`) and compare.
- L5 (light cap): see N-11.

## NEEDS INVESTIGATION

- N-11 Light cap. Method: dumped shader loop.
- N-21 Vendor-specific behaviour. Method: community logs.
- N-10 Linux and Proton. Method: a Proton run.
- N-9 Steam interplay with the renamed exe (Ch02).
- N-12 and N-13 owner paths in build and tool scripts. Method: inventory with a search for the owner's workstation path prefix in `tools` and `src`, replaced with placeholders only.
- N-29 (proposed) Minimum GPU and GL. Method: forced-fallback runs.
- Rationale for forward rendering. Method: ask the owner; a decision record may exist in the planning notes not in the repo (add to decisions-log, Ch13).

## Discrepancies

- **Fallback.** `README.md:265` ("does not fall back to a slower path") and `src/atmos.inl:371` ("no heavy ray-march fallback") agree for the default configuration. The code also uses a per-pixel ray-march path when the froxel volume is disabled (`[atmos] Froxel=0`) or fails with contact shadows on (`src/atmos.inl:453-454`, `src/atmos.inl:493-496`). The README statement is therefore incomplete. Recommend the README be corrected by the reviewer.
- **Atmosphere comment.** `src/atmos.inl:5` says `Density=0.00006`. The code default is `0.000015` (`src/atmos.inl:288`). Ch06 to confirm.
- **Contact shadows.** `src/atmos.inl:287` and `src/atmos.inl:296` default contact shadows to 0.35 in code. The shipped ini sets 0 (`src/default_tyrender.ini:35`), and `README.md:297` shows 0.

## See also

- [what-is-this.md](what-is-this.md)
- [game-and-engine-facts.md](game-and-engine-facts.md)
- [material-model.md](material-model.md)
- [../how-it-works/11-fallbacks-and-failure-handling.md](../how-it-works/11-fallbacks-and-failure-handling.md)
- [../shaders/atmosphere-froxel-compute.md](../shaders/atmosphere-froxel-compute.md)
- [../decisions/known-issues.md](../decisions/known-issues.md)
