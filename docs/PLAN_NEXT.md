# Plan for the next build

Updated 2026-10-18, after the playtest of build 4a2ef95.

## Status

- Last full release: v0.3.5 (c255ae8).
- Merged since v0.3.5: #36, #38, #40, #42, #43, #45, #46, #47, #48, #49, #50, #51, #52, #54, #58, #60, #74, #75, #78, #87, #89, #91, #92, #93, #96, #98, #100, #102, #106 (WaterNames), #107, #108, #109 (README).
- Playtest of 4a2ef95 (owner, Ultra):
  - Good: sky, red rock and horizon fade smoothly; fog looks good; trees fine; no crashes; F11 screenshots work (four on/off pairs written); tessellation floor lift on.
  - Open: F10 does nothing (default was Key=0, now F10 via #102; verify in a level, #79); slow water brightness ramp on the character (GI off the blue texture, #5); lilies and rocks brighter than the water under them and heavier top-down fog (#4, #82); sharp shadow line at the bridge by the fire (#103); fire not fogged (#4); water outline from above (#104); portal emit fix (#100) untested.
- Playtest of cda9364:
  - Fog colour looked steady (#3). #3 is closed; the yaw sweep is no longer tracked here.
  - Fog: level-two trees poke through, sky and red rock not fogged, water and lilies wrong (#4).
  - Ty still flickers blue in water (#5).
  - Footprints are just bright white, not a light (#8, closed).
  - Portal light did not light the ground; unclear whether [emit] Enable=1 was set (#9).
  - LOD options showed no difference (#10).
  - F10 overlay regression: opening it breaks all geometry until restart and cannot be closed (#12). Default is now F10 (#102); older inis need Key=F10.
  - Launcher guard (#13) untested; GPU portability (#15, #16) awaits other GPUs.

## Open work by owner thread (priority order)

1. Unblock desktop permissions and install the newest main.
2. F10 overlay (#12, #79): verify in a level with Key=F10.
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
