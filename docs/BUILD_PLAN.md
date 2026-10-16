# TY HD Renderer: build plan (from 2026-10-16)

Source of truth for the work: `docs/PLAN_3DAY_20261016.md` (3-day pickup plan), `docs/research/minecraft_shaderpack_port_ranking.md` (ideas only, no code copied), `docs/todo.json` (task board). Desktop checkout: `C:\CLAUDE`.

Target: 2560x1440, RX 7600 XT, 60 fps = 16.7 ms. Medium measures 12.08 ms (v0.3.2).

## Decisions (accepted 2026-10-16)

- A. Ultra stays as an experimental preset in the ini; shipped defaults are High-style until #6 is fixed.
- B. Batch 1 (AO, TAA, shadows) before distance LOD.
- C. Footprints (#8): tag-based luminance clamp first, deeper decal work later.
- D. Other GPUs: the user supplies NVIDIA or Intel logs when available; not blocking work.
- E. Ship v0.3.3 after batch 1 and 2 with one playtest.

Nothing ships, and no release is cut, without the user's sign-off.

## Rules

- Max 3 agents at a time, each with its own task file.
- One game process at a time. Launch only through the harness scripts (`agent_test`, `suite`, `motion`, `flicker`, `g1_gate`).
- No game runs while the user is playing.
- GPU timers only, never wall clock.
- Failure of any effect means the effect is OFF (fallback policy). Never ship a crashing shader.
- One PR per issue. Each PR states before/after, the A/B evidence, and the GPU-timer cost.

## Sequence

| Batch | Issue | Todo | Work | Notes |
|---|---|---|---|---|
| 1 | #1 AO | T73, T90 | distance fade, character not an occluder, history clamp, blue noise | Medium +1 ms max |
| 1 | #2 TAA | T91 | depth-guided history reject, velocity-adaptive blend | TAA history filter from the ranking (#3) only if ghosting survives |
| 1 | #11 shadows | T82, T75 | shadow-edge bias (ranking #1: normal offset, slope bias), texel-aligned noise | verification of the textureLod fix, including Ultra MapScale=4 |
| 1 | #2 tonemap check | (ranking #2) | A/B the tonemap against the current filmic curve | under 0.1 ms, optional in batch 1 |
| 2 | #5 water disco ball | C1, T74 | isolate hemisphere, specular and engine reflection | slot-2 pond scene |
| 2 | #6 High/Ultra flicker | T78, C2 | bisect GI vs contact shadows, Ultra extras at low-fps emulation | |
| 2 | #4 fog, #3 fog colour | T92 | alpha-tested and transparent draws fogged by depth; fog hue constant under yaw | |
| 2 | #7 High GI cost | perf | recover the +2.5 ms (mask and blur) to <= +1 ms | |
| 3 | #10 distance LOD | T76 | smooth falloff across effects | after batch 1 |
| 3 | #8 footprints | T77, C4 | luminance clamp on decal path | option C |
| 4 | #9 portal light | T56 | half-res additive spill | v0.4 unless trivial |
| 5 | #12 overlay menu, #13 clash guard, #14 AB hotkey | D1, D2 | tooling | |
| 5 | #16 portability audit | T50 | vendor-guard audit of GL calls | |
| 5 | #15 research | A1, A4 | world-space AO/GI/fog alternatives | ideas only |
| last | #17 release v0.3.5 | | G1 gate, release.ps1, zip verify | ask before shipping |

## Detail per item

Files, dependencies, what needs a game run, and acceptance per item: `docs/FIX_PLAN_20261016.md` (desktop, batches 1 to 3). This file is the sequence and the decisions; that file is the item-level plan.

## Status (2026-10-16)

- Issues #1 to #17: all open and current on GitHub (seeded 2026-10-16).
- Fix branches: `fix/issue-1-ao`, `fix/issue-2-taa`, `fix/issue-11-shadows` are not pushed yet. PRs open when each branch exists.
- Game runs: paused by the owner. Code edits and builds only until the owner says otherwise.
- Nothing is merged. Nothing ships without the owner's sign-off.
