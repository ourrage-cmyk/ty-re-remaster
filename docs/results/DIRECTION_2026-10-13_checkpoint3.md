# DIRECTION - checkpoint 3 (T27): v0.3 release readiness, 2026-10-13
Evidence: GOAL.md, checkpoint2 memo, todo board, cost_table.md, preset_matrix.md, soak_output.txt, v_fallback2 + rel_smoke + t41_still manifests, dist v0.3 zip/README/PERFORMANCE/ini, SHA1 of the DLLs. Images (2): t41_still/sheet.png, t41_still/ty_strip.png.

**Key finding: the shipped DLL is not the binary most tests ran on.** dist + rel_smoke = `15C86A6FB3`; T41 stills + v_fallback2 = `3AAAB5D30F` (merge\out); soak ran 02:26, BEFORE T44 changed the fallback code (02:55). Only rel_smoke (1 view, 11.08 ms, 0 errors) touched the shipped binary.

## 1. Acceptance
| # | criterion | status | evidence / gap |
|---|---|---|---|
| 1 | No regressions | PARTLY | Ty intact bal/off/balc (ty_strip); no speckle/stipple in t41 stills; fallback->OFF renders at 6.96 ms (v_fallback2). Gaps: motion flicker/shimmer never measured; 10-min soak predates T44 and the shipped build |
| 2 | Balanced <= 16.7 ms | PARTLY | 11.08-11.10 ms default view (rel_smoke, t41, cost_table), 5.6 ms headroom. Vista/worst view not measured (checkpoint-2 gate not met). Per-option costs + launcher: MET (PERFORMANCE.md, T25) |
| 3 | Look | PARTLY | Far rock hazier/less saturated, near unchanged: MET (sheet default/fence rows) - but fence_close background is washed almost white-pink, too strong at Intensity 2.5. Shadows tinted, no stipple: MET (R3). Feet: MET (ty_feet, inward-only). Sand profile at graze: subtle at sheet scale (lead's 100% crop says visible). Fence grain: weak. Rope: no frame clearly shows rope strands -> UNVERIFIED. Sun warmth: not done (T43) |
| 4 | Portability | PARTLY | Fallback->OFF policy + ForceFallback test OK; README states RX 7600 XT only (honest). Real GL<4.3 / NVIDIA / Intel / Proton never run; vendor-guard audit not done |
| 5 | Package | MET | zip: dll, launcher, ini (=Balanced), README, PERFORMANCE.md; TY.exe untouched; LAA copy (T26) |

## 2. Verdict: SHIP as "v0.3 preview / community beta" - after gate G1 (automated, no user time)
G1 on the SHIPPED dll (sha 15C86A): rerun soak 10 min with dist ini + v_fallback (ForceFallback=31) + perf at vista and the heaviest suite view. Pass = soak OK, forced renders <= Balanced ms, worst view <= 16.7 ms. If vista > 16.7: ship anyway but drop the "60 fps" wording to "default view" and fix in v0.3.1.
Not a full v1 release: criteria 1-4 are partly met; label it preview and ask for reports (T29 text already in README).

## 3. Top 3 remaining risks
1. **Binary drift**: soak/fallback/T41 evidence belongs to other builds; a regression between 3AAAB5 and 15C86A would ship unseen (G1 closes it; add sha check to release.ps1).
2. **Motion artefacts** (AO/TAA shimmer, TAA trails, detail-layer crawl) - the first thing the community films; criterion 1 cannot be judged from stills.
3. **Other GPUs**: one AMD card; NVIDIA/Intel shader compile differences + fallback path untested in the wild; slower cards may miss 60 fps with only 5.6 ms headroom at the easy view.

## 4. Doc fixes before upload (lead, minutes)
- README "Known limitations" says "Tessellation has no displacement" - contradicts v0.3 "inward-only displacement"; reword.
- ini header "60+ fps on a mid-range GPU" -> "about 90 fps at 1440p on an RX 7600 XT (default view)".
- PERFORMANCE.md: state stock frame 5.7 ms here vs 4.6 ms in GOAL (different view/build) so readers are not confused; note "vista not measured" until G1 adds it.

## 5. Todo edits (lead applies)
1. ADD T45 auto (blocks T28/T29): "G1 release gate on shipped dll 15C86A: soak 10 min, v_fallback forced=31, perf default+vista+heaviest view". deps T26.
2. ADD T46 lead (low): README/ini/PERFORMANCE wording fixes above; repackage; release.ps1 records + asserts dll sha equals last soak/fallback sha.
3. ADD T47 auto (v0.3.1): motion suite - 2 s scripted pan at fence+grass, frame-diff on static geometry, bal vs off; fail if bal shimmer > off by set margin. Then T42-style one-session fix if it fails.
4. ADD T48 lead (v0.3.1): atmos Intensity 2.5 -> ~1.8-2.0 for Balanced (fence_close far wall washes out); A/B 3 views, keep rock_sat drop.
5. ADD T49 auto (v0.3.1): rope + fence-grain 100% crop views with real camera on a rope (current rope_a/b frames show no rope).
6. ADD T50 (v0.4): vendor-guard audit of GL calls + real GL 3.3/4.0 context test (force context version) instead of ForceFallback bits only.
7. T43 SunWarmth: move to v0.4, non-blocking. T32: drop (still READY on board; checkpoint 2 dropped it).
8. T29: keep; add "collect NVIDIA/Intel/Proton tyrender.log from beta testers; record in results\hw_reports.md".

## 6. Next checkpoint
After T45+T46 -> T28/T29 user sign-off. Single test: G1 passes on sha 15C86A.

## Status note (docs pass 2026-10-13)
The v0.3 release items in section 3 (README known-limitations wording, ini header) were superseded by v0.3.2 shipped docs. Current state: RESEARCH\STATUS_2026-10-13.md and RESEARCH\RESUME.md. The shipped ini header still says Balanced and was not changed in this pass.
