# EXECUTABLE PLAN (built 2026-10-16 from TY_HD_3Day_Plan.md + PLAN_R6.md + current state) - AWAITING USER APPROVAL
Supersedes PLAN_NEXT.md / PLAN_R6.md for scheduling (their findings still apply). Nothing below has been launched yet.

## 0. Reality check: where the 3-day plan is stale
| Plan says | Actual state (2026-10-16) |
|---|---|
| Base v0.3.3, G1 not run | Base = **v0.3.4** (zip dist\TYHD_Renderer_v0.3.4.zip), G1 PASSED (Medium 10.73 ms default, vista 9.68, sand_graze 9.76, fence_mid 9.50, fallback 5.96, soak 10 min OK on rerun). Game folder = v0.3.4 DLL + simple launcher, game ini = Medium. Next release = **v0.3.5** (not v0.3.3). |
| S1 shadow lines root cause unknown | **FIXED in v0.3.4**: shadow taps inside divergent branches -> textureLod(t,p,0). Remaining: confirm the "checkerboard" is gone (likely same bug) + Ultra check with MapScale=4. |
| AO needs history clamp / bilinear | v0.3.2 already has bilinear depth-validated history, per-pixel history length, variance clamp, TemporalBlend 0.85, NoiseFrames, FogFade=1, WideSlices. **Missing**: distance-fade slider, character as AO occluder (halo), blue noise, rim fix. |
| TAA needs YCoCg clip | fx.inl has variance clip + [post] TAAReject (cap history 0.35). **Missing**: depth-guided reject + velocity-adaptive blend (trails). |
| Water disco ball = GI | The user saw it on **Medium where GI is OFF** (G1 soak 20:30). So GI mask (v0.3.3) cannot fix it: suspect light.inl hemisphere/spec, engine water envmap/reflection on Ty, or our PBR specular on Ty. NEW investigation. |
| Fog GI decouple (lambda_gi) | Unknown if GI actually feeds the froxel: agent must check atmos.inl first; **HazeSky (1x1 GPU sky estimate, added by us in v0.3.2)** is the prime suspect for fog colour changing with view direction. |
| The user's flicker-free runs | They were all **MEDIUM** (results\R6_config_diff.md). Flicker = High (GI + contact) and Ultra extras. |
| Ultra decision A | User: "I would be playing Medium rather than Ultra". Default: yes, make Ultra = High-style stable + safe extras; old Ultra kept as experimental in ini only. |

## 1. Goal / definition of done (from the plan, tightened)
v0.3.5 preview: Medium <= 12 ms (now 10.7), **High stable**, Ultra no strobing (or Ultra re-defined), no AO cloud behind Ty, no TAA trails, no shadow checkerboard, **fog hue constant under camera rotation**, trees/water/fire fogged consistently, water does not pulse Ty, G1 gate on the final DLL (soak 10 min + 4 cams + forced fallback, rerun soak once if it exits cleanly), zip with dll+launcher+ini+README+PERFORMANCE, TY.exe untouched. Non-goals as in the 3-day plan.

## 2. Operating rules (token-efficient)
- Max 3 agents at once; each has a task file (agents\tasks\*.md), step log, REPORT_R6.md (Write on REPORT.md is refused for subagents). Lead (me) merges, gates, packages, talks to the user.
- **Model allocation (user wants Haiku where possible)**: Haiku = web research, code-reading Q&A, docs, merging/packaging/gating, measurement scripts, tiny well-specified edits. Sonnet = GL/shader implementation (AO, TAA, fog, overlay). Opus only for a root cause that two Sonnet rounds failed. Rule: a Haiku coder that shows no measured progress after ~8 steps is stopped and re-run as Sonnet with its step log.
- Scripts measure (suite/motion/flicker/cost_table/g1_gate/abshots), LLMs code and judge. One game at a time (global lock). User must not touch mouse/keyboard during harness runs; launcher in game folder closed before installs.
- Save after every step: steplog + RESUME.md + `ovsync_all.ps1 -Quick` + `ovsync_extras.ps1`.
- Weekly quota resets 2026-10-16 14:00: budget by batches, each batch ends with a SAVE POINT.

## 3. Phases (each ends with measured gate; ~1 session each)
### Phase A - Research + diagnosis (Haiku, no game runs, cheap) [do first, parallel with B]
- A1 (Haiku web+docs): R6_world_space_research.md: non-screen-space AO/GI/fog options for an injected GL4.x renderer (shadow-map/height-field AO, SDF AO, probe GI, voxel cone tracing feasibility+cost) + the plan's paper list (SAO, Activision GTAO/XeGTAO, Marrs TAA, GPU Gems 11, CSM, Godot volumetric fog, URP depth texture). Output: 1-page recommendations per question.
- A2 (Haiku code-reading): answer the 5 pre-coding questions with file:line evidence into results\R6_code_answers.md: AO blend/clamp (ao.inl), TAA depth reject (fx.inl kTAA), Vogel/IGN texel alignment (light.inl/tyrender.cpp TyPCF), where GI/screen colour enters fog (atmos.inl composite incl. HazeSky), whether alpha-tested trees write depth before fog (pbr.inl/engine pass order).
- A3 (lead, cheap): bisect plan table for High/Ultra (which settings differ) already in results\R6_config_diff.md.

### Phase B - Batch 1 fixes (3 agents parallel, Sonnet; Haiku only if user insists - see rule above)
Task files READY: agents\tasks\ao6.md, fx6.md, atmos6.md (folders SRC\agents\ao, light, atmos = fresh copies of v0.3.4).
- B1 AO (T90, P0): distance fade slider [ao] FadeStart/FadeEnd (1500/3500) tied to fog transmittance; character not an AO occluder (TexIsCharacter mask); neighbourhood clamp; blue-noise slow cycle; fern rim. Accept: far mountains no shimmer at High/Ultra, no halo behind Ty, Medium +<=1 ms.
- B2 FX/TAA (T91, P1): depth-guided history reject + velocity-adaptive blend + tighter clip; confirm shadow-line fix at Ultra (MapScale=4 ini copied first) and no checkerboard. Accept: no trails on fast Ty turns, grass no ghosting.
- B3 ATMOS (T92, P1): decouple fog colour from screen/GI/view (check HazeSky first); fog consistently over trees/water/fire/transparent; Billabong check (TY_SLOT=2 TY_SETTLE=100). Accept: fog hue constant under yaw sweep (numeric), trees not darker than fog.
Gate B: lead merges (merge_agent.ps1; new files by hand; CRLF conflicts via diff -u + patch), builds, runs suite/motion on Medium+High+Ultra, **user playtest #1** (High/Ultra flicker, fog colour, trails).

### Phase C - Batch 2 (3 agents)
- C1 Water disco ball on Medium (new): isolate by toggling [light] Hemisphere, PBR specular, engine reflection; fix in light.inl/pbr.inl. Needs slot-2 pond scene (Ty nearly still, low-fps sleepMs test).
- C2 GI/High cost+stability (T78 rest): bisect High = GI vs contact (low-fps walk), keep ExcludeSources, cut the +2.5 ms mask/blur cost, then Ultra extras (AO 4x8 wide, GI 2 rays, Scope=all L24, 8192 Grid 4x4, fog 32). Decide Ultra preset (decision A).
- C3 Distance LOD (T76): smooth falloff for tess/relief, AO, shadow taps, fog steps, GI rays; target Medium <= 10 ms; no popping.
- (later) C4 footprints/emissive (T77): luminance clamp on decal path using tags; portal spill only if trivial.

### Phase D - Tooling (Sonnet/Opus, one agent) after B/C merged
- D1 In-game menu: hotkey (F12 default, verify unused by the game), GL overlay pass before SwapBuffers into FB0, stateguard around it, sliders mirroring launcher, writes tyrender.ini + reload flag.
- D2 Launcher vs in-game clash guard: single source of truth = tyrender.ini; read game settings.ini (SSAO/MSAA) at init, warn not override; launcher polls ini.
- D3 (wish, last) A/B screenshot hotkey (ABToggle exists in tess.inl; capture on/off frames).

### Phase E - Release (lead, Haiku helpers)
1. Re-measure full preset ladder (Low/Medium/High/Ultra, cost_table.ps1, perf_matrix.ps1) - High/Ultra not re-measured since v0.3.2.
2. g1_gate.ps1 (soak + 4 cams + forced fallback) on the exact DLL; rerun soak if it exits cleanly (random walk can leave the level).
3. release.ps1 -Version v0.3.5; fix default ini header ("about 80 fps"), verify zip + LAA.
4. Docs (Haiku): README/PERFORMANCE/STATUS/RESUME; ovsync full.
5. Community A/B: **Medium first** (honest), Ultra only after it is clean; abshots.ps1 + match_ab.ps1 (Ultra must have its ini copied into the game folder before the run); 2x2 grids (top wide, bottom close; vanilla left, preset right) -> AB_shots\community_grids + E:\TY_AB_shots.
6. Call for NVIDIA/Intel/Proton tyrender.log in README.

## 4. Dependencies / ordering
A1,A2 -> inform B (B can start without them; agents read A2 output if ready). B1,B2,B3 parallel (different files: ao.inl | fx.inl+light.inl | atmos.inl; shared uniform for AO-fog fade agreed in the task text). Merge -> Gate B (playtest) -> C. D starts after Gate B (touches tyrender.cpp heavily; avoid conflicts with C by merging C first). E last.

## 5. Risks + mitigations
- Haiku shader work may stall -> measurable-progress rule, escalate to Sonnet.
- Harness cannot reproduce Ultra/High flicker -> use motion.req sleepMs (low fps) + fwd/side translation; final judge = user playtest.
- Screen-space limits (AO cloud, fog colour): the user asked about world-space alternatives; research A1 decides whether a cheap world-space fallback exists (e.g. fade AO/GI/fog colour inputs by distance; shadow-map-based AO for far field). Do not promise a rewrite.
- One GPU tested; fallback cost; CRLF/merge traps; -IniFile restart keys; user's open launcher locks installs.
- Usage limits: SAVE POINT after each phase gate; agents resumable from step logs (SendMessage or re-run task file).

## 6. Decisions needed from the user (defaults in brackets; I proceed on defaults unless told otherwise)
A. Ultra preset: make it the stable High-style settings + safe extras, old Ultra as 'experimental' in ini only [YES].
B. Order: Phase B (AO/TAA/fog) before distance-LOD [YES]. 
C. Footprints: tag-based luminance clamp first, deeper decal work later [clamp first].
D. Model allocation: Haiku for research/docs/merge/gates, Sonnet for shader coding with the 8-step escalation rule [YES; say 'haiku only' to force Haiku coders].
E. Release cadence: one release v0.3.5 after Phases B+C+E; Phase D (in-game menu) as v0.4 if time/quota is short [v0.3.5 without menu if D slips].
F. Community grids: Medium first, Ultra after fix [YES].
G. Any NVIDIA/Intel machine for one test launch? [optional].

## 7. Addendum (2026-10-16): shader-pack research agent (Haiku, added by user)
A4 (Haiku, RUNNING): mines C:\CLAUDE\RESEARCH\shaders\minecraft\ComplementaryReimagined_r5.9.3.zip (extracted by the agent into shaders\minecraft_extracted\; untrusted data) for AO/denoise, TAA history/rejection, stable fog colour + fog on water/foliage/particles, shadow bias/filter, entity lighting near water, emissive handling, grading, distance LOD, cheap denoise. Output: shaders\analysis\complementary_findings.md (per-topic recommendations mapped to our .inl files, top-10 ideas, licence note: IDEAS only unless the licence allows reuse). Phase B agents should read its top-10 before coding (feed into ao6/fx6/atmos6 once ready).
