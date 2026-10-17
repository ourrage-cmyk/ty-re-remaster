# Plan for the next build

Updated 2026-10-17, after the playtest of build cda9364.

## Status

- Last full release: v0.3.5 (c255ae8).
- Merged since v0.3.5: #36, #38, #40, #42, #43, #45, #46, #47, #48, #49, #50, #51, #52, #54, #58, #60, #74, #75, #78.
- Playtest of cda9364:
  - Fog colour looked steady (#3). #3 is closed; the yaw sweep is no longer tracked here.
  - Fog: level-two trees poke through, sky and red rock not fogged, water and lilies wrong (#4).
  - Ty still flickers blue in water (#5).
  - Footprints are just bright white, not a light (#8, closed).
  - Portal light did not light the ground; unclear whether [emit] Enable=1 was set (#9).
  - LOD options showed no difference (#10).
  - F10 overlay regression: opening it breaks all geometry until restart and cannot be closed (#12). Now off by default ([overlay] Key=0).
  - Launcher guard (#13) untested; GPU portability (#15, #16) awaits other GPUs.

## Open work by owner thread (priority order)

1. Unblock desktop permissions and install the newest main.
2. F10 overlay regression fix (#12). Verify in game before re-enabling the key.
3. Fog (#4):
   - Sky: [atmos] SkyFogDist is already merged in #58 (default 0.45; untested in game). Verify in the playtest.
   - Trees: A/B test [fog] SeeThruDepth=1 against the default 0.
   - Water and lilies: WaterBoost fogs water draws (#74, merged, untested in game). Check whether lilies are covered.
4. Water blue flicker (#5): bisect with in-game measurement, no more guesses.
5. Portal light (#9): test via [emit] Enable=1 (forward-render light cap is 16).
6. LOD follow-ups (#10): tessellation (#60) and shadow (#78) distance LOD, both default off; measure in game.
7. Low priority:
   - Tessellation tuning: ReliefCurve and Knee seem to help low-res textures.
8. Next release v0.3.6, only after the next playtest.

## Rules

- One game launch at a time, and announce it first.
- Issues close only after the owner's playtest.
- Merge small PRs to main.
- The release artifact is TYLauncher.exe.
