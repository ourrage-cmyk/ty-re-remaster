# DIRECTION - checkpoint 2 (T21), 2026-10-13
Evidence read: GOAL.md, todo board, VERDICTS.md (R1-4), ledger (last 15), preset_matrix.md, balanced_result.md, inis\cand_B3/C1/preset_*.ini, soak_output.txt, v_fallback manifest+log, DIRECTION checkpoint1. Images (4): m_gi/ty_strip, m_gi/sheet, m_tess/sheet, v_fallback/sheet.

## 1. On track?
| # | criterion | status | evidence |
|---|---|---|---|
| 1 | No regressions | PARTLY | Ty intact off/feature/ultra (m_gi/ty_strip); 10-min soak OK with preset_Balanced.ini (soak_output); forced fallback renders, its 7 log errors are only the forced `#error` ones (v_fallback). NOT tested: flicker/ghosting/shimmer under MOTION (all evidence is still frames); m_tess/sheet `off/vista` is BLACK (missing capture passed as "OK" -> the harness does not fail on blank images) |
| 2 | Balanced <= 16.7 ms | PARTLY | C1 = 11.11 ms default view (bal_C1). Default view only; vista/heavy views not measured on merged build (earlier suites show last-view +2-4 ms). Per-effect costs on the MERGED build, PERFORMANCE.md, launcher: not done (T25) |
| 3 | Look | PARTLY | far rock hazier/less saturated, near unchanged: MET (m_gi/sheet; Ultra density also washes mid-range walls pink - tune down). Sand grain: met per lead 100% crop (tess_d8, not viewed by me). Fence grain/rope: UNVERIFIED. Feet: geometry unmoved at Scope=skin -> safe. Shadows tinted/cool, no stipple: MET (R3); sun not warmed -> "more natural" only half. AO/GI noise: stills OK, motion unknown |
| 4 | Portability | PARTLY | forced-fallback path works; real GL<4.3 path never run; vendor guards unaudited; one GPU. Fallback is EXPENSIVE: forced frame 30.72 ms (legacy AO 2.94, raymarch 6.34) - a failing machine lands at 32 fps |
| 5 | Package | NOT MET | T26 pending; LAA done |

## 2. Biggest risk
1. **Unverified artefacts that only show at 100% or in motion** (rope/rug-paint speckle/rail stipple of the detail layer, AO v2 thin-occluder + motion shimmer). These are exactly what the Ty community will screenshot on day one, and criterion 1 ("no flicker") cannot be judged from any suite we have. This beats the other candidates: the launcher/preset update is mechanical (T25), the single-GPU status is accepted by GOAL.md if stated honestly, and sun warmth is a small polish.
2. **Preset ladder is incoherent and relies on code defaults**: Medium (12.55) costs MORE than Balanced (11.11); High (23.6) is far above budget; no preset writes [light] or detail-layer keys, so what Balanced shows depends on compiled defaults.

## 3. Decisions
- **Balanced = C1 + nothing yet; keep >= 3 ms reserve.** 11.1 ms is at the easy default view on the user's GPU; heavier views and slower NVIDIA/Intel cards eat headroom. Spend at most ~1.6 ms now: **contact shadows 0.35 ONLY if the T41 feet crop shows Ty visibly grounded** (it serves "feet rest on the ground"); otherwise off.
- **GI hiz1: High/Ultra only** (+4.7 ms, barely visible). **Tess Scope=all: Ultra only** (B3 vs C1 = +5.8 ms on merged build, little gain). **Shadow MapScale4/Taps32: High**; into Balanced only if T40 shows <= 1 ms AND visible.
- **Fallback policy: failure -> OFF / original engine path, not legacy heavy paths** (no legacy AO, no raymarch, no legacy GI as automatic fallback). Fallback must never be slower than the preset it replaces.
- **Detail layer: one fix session max** (T42); if rug/rail artefacts remain, mask those materials or lower DetailStrength - never ship a speckle.
- **Sun warmth: one low-token attempt (T43), non-blocking**; else defer to v0.4 and say so in README.
- **Drop T32** (C1 passes with 5.6 ms headroom). Linux/Proton, more GPUs: defer to T29 / community beta.

## 4. Edits to todo.json (lead applies)
1. T32: status -> dropped, note "C1 = 11.11 ms passes; not needed".
2. ADD T40 auto "Merged-build cost table": C1 vs C1 +/- each option (contact 0.35, MapScale4/Taps32, GI hiz1, Scope=all, AO off, atmos off, light off, detail off), measured at default AND vista view, in-session. deps T23. Output feeds PERFORMANCE.md + launcher.
3. ADD T41 auto "Merged look + motion suite" on preset_Balanced: in-session A/B off vs Balanced, 100% crops of rope, rug paint, fence rail/grain, Ty feet (contact on/off); plus a 2 s scripted camera pan/orbit at fence+grass capturing consecutive frames, metric = frame-to-frame diff on static geometry (shimmer). Harness must FAIL on black/missing images (fixes the m_tess off/vista gap). deps T23.
4. ADD T42 agent (tess+ao, cap 1 session, est medium) "Fix what T41 finds: rug speckle, rail stipple, AO thin-occluder shimmer; else mask/lower defaults". deps T41.
5. ADD T43 lead (low, optional) "[light] SunWarmth key, modest default, A/B one view; defer if > 1 session". deps T21. Not a dep of T26.
6. ADD T44 lead (low) "Fallback -> OFF policy for AO v2 / froxel / GI / tess; rerun v_fallback with preset_Balanced.ini; gate: forced frame <= Balanced frame, renders, no crash". deps T21.
7. T25 re-scope: monotonic ladder Low < Medium < Balanced < High < Ultra by measured ms (Medium below 11 ms, High <= ~20); EVERY preset writes explicit [light], [tess] Detail*, [atmos] Density keys; launcher costs from T40; atmos Density for Ultra lowered (mid-wall wash). deps add T40, T41 (contact decision).
8. T26 deps add T25, T42, T44. T27 deps add T41.
9. T29 add: "README states RX 7600 XT only; ask community for NVIDIA/Intel/Proton reports".

## 5. Next checkpoint
T27 (release readiness). Single gate before it: **T41 passes on the final Balanced ini - no visible speckle/stipple in rope/rug/rail/feet 100% crops, frame-to-frame shimmer on static geometry not above the off build, Ty intact, AND Balanced <= 16.7 ms at the WORST suite view (vista), not only the default view.**
