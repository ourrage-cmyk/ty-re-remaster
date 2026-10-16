# Plan R6 (written 2026-10-13 late, low on tokens; launch next session)
Source: user feedback after watching the v0.3.4 harness runs (saved verbatim-ish below). Current build v0.3.4 (G1 passed, installed, game ini = Medium new colours). Community grids in AB_shots\community_grids are DRAFT ONLY (Ultra side has rug streaks/fan smear at ty_feet, washed fence background) - do not share.

## User feedback (summary)
1. AO is very noisy and likely the main flicker cause; AO is screen space -> big noisy dark cloud behind the character, shimmering.
2. Denoise/TAA adds ghosting on grass and motion-blur-like look; fast-moving objects leave trails. (Later at 20:30 grass ghosting was better.)
3. Character shadows: ugly checkerboard (later runs: line through shadows gone).
4. Launcher settings and in-game settings must not clash.
5. In-game overlay menu on an unused hotkey to change settings live. Wish (end of list): 2nd hotkey for A/B screenshots (shaders on/off, screenshot each).
6. Fog changes colour with camera direction (red when looking at red things, white otherwise) - "really bad". Suspect: GI (screen space) or HazeSky 1x1 sky estimate / fog colour from screen average. Make fog colour view-independent.
7. Billabong: distant trees stick out through fog (their shadows/AO make them darker than the fogged environment -> look in front of the fog). Water is less fogged than rocks (rock outlines visible against water). Fog application inconsistent -> rework: apply AO/shadow/GI BEFORE fog consistently and fog every surface (incl. water/transparent/late draws) by depth.
8. Water: Ty still lit like a disco ball (blue on/off) when in water at 20:30.
9. User watched harness runs: 20:22 run had NO flicker (little shadow noise, AO noisy, looked like no TAA, no shadow lines); 20:23 run had AA, distant shadows not flickering, AO noisy, no line in character shadows; 20:27 run = BEST so far (good AA, tess, AO slightly noisy, perfect shadow map, no trailing edge, little ghosting). ULTRA still shimmers/flutters in distant shadows/AO. TASK: find which configs ran at 20:22 / 20:23 / 20:27 (G1 gate sequence: soak/forced-fallback/perf cams - check timestamps of results\g1_*, results\soak, grid_ultra_v034, suite manifests, ledger.csv, tyrender_last.log copies) and diff their ini vs Ultra/High to find what causes Ultra shimmer.
10. User asks: can AO/GI/fog be made not screen-space bound? -> web research (world-space AO via shadow-map/height-field AO, voxel/probe GI, froxel fog already world-space but colour from screen - fix inputs).

## Tasks (max 3 agents in parallel; Opus for root-cause, Sonnet for implementation; each logs steps + REPORT_R6.md)
- R6-1 (lead, cheap first): identify the 20:22/20:23/20:27 configs from timestamps; write the ini diff vs Ultra into results\R6_config_diff.md. This decides the Ultra fix (likely: forced-fallback = AO/GI off, or a cam/preset difference).
- R6-2 AO agent: AO noise + halo behind character: per-pixel stable noise + stronger spatial denoise at half res, distance fade slider (F1, strength 1 near camera -> 0 far), character not an occluder (TexIsCharacter mask), AO applied before fog and attenuated by fog transmittance; measure with motion.ps1 (fwd/sleepMs) at Ultra; target: Ultra distant shimmer = Medium level.
- R6-3 fog agent: view-independent fog colour (no screen-average/GI feeding fog colour), fog all surfaces consistently (water/transparent/emissive/late draws), Billabong trees not darker than fogged surroundings (AO/shadow fade by transmittance), water disco-ball (exclude water from any lighting input on characters; check GI ExcludeSources + light.inl hemisphere).
- R6-4 TAA agent: trails behind fast objects + grass ghosting: velocity-aware history rejection (per-object motion unavailable -> depth-based reprojection + neighbourhood clamp + TAAReject tuning), character checkerboard shadows (confirm gone at Ultra).
- R6-5 launcher/in-game settings consistency: single source of truth = tyrender.ini; launcher reads live values; in-game overlay (ImGui-like minimal GL overlay on e.g. F10, does not use game keys) writing the same ini + reload. Later: F11 A/B screenshot hotkey.
- R6-6 web research (Haiku): non-screen-space AO/GI/fog options for a GL4.x injected renderer (shadow-map AO, SDF/heightfield AO, probe GI, voxel cone tracing feasibility), cost estimates -> RESEARCH\R6_world_space_research.md.
- Then: merge, G1 gate, user playtest, Medium community grids (honest) + Ultra after fixes.
