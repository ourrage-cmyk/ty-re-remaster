# Task: ATMOSPHERE (agent folder C:\CLAUDE\SRC\agents\atmos\, owns atmos.inl)
Read `_common.md` first. State: froxel compute path implemented and measured (1.68 ms vs 3.82 ms old, atmos-only, default view), keys Intensity/Desaturation/HazeLift/SkyHaze/HazeHue/Froxel/FroxelPixels, fallback to the old half-res ray march. See your step log + REPORT.md.
Earlier suspected regression (Ty rendered as exploded shards) was NOT reproducible in fresh sessions with the Large-Address-Aware exe and is now believed to be an out-of-memory symptom; the lead's re-checks are `results\v_atmos`, `v_atmos_r1`, `v_atmos_r2` (look at their `ty_strip.png`). If you can reproduce it, use `[debug] StateGuard=1` and log the leaked state.
Remaining work:
1. Review `C:\CLAUDE\RESEARCH\results\v_atmos\sheet.png` (off / froxel / raymarch x 5 views, produced by the lead's suite). Note: your earlier -CamSeq views never moved the camera because your copy of debugcam.inl had a path bug; the lead has fixed it in your folder (infra.ps1 sync-infra), so `vista`, `sand_graze`, etc. are real now.
2. GOAL: far rocks clearly less saturated/hazier, near objects (sand, fence, ropes, Ty) not affected, sky unchanged, no banding/pink cast. Use the suite manifest metric `rock_sat` (rock-wall saturation; off vs froxel should drop clearly, e.g. >= 30%) plus your eyes at 100% crops. Tune the defaults (Intensity/Desaturation/HazeLift) to look natural, not milky; verify on the real `vista` view.
3. Cost: keep atmosphere <= ~2 ms total (contact shadows included); re-measure with the suite (AB `off` vs `froxel`) and log.
4. Robustness: no GL4.3 / failure -> automatic fallback (already). Check resolution change handling.
5. Finalize REPORT.md. No tyrender.cpp change expected.
