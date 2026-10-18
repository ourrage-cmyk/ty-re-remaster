# Failed and abandoned approaches
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone). Audience: A, C.

## Purpose
This page lists what was tried and did not work, or was dropped, and why. It is here so that a contributor does not repeat an approach the project already measured or rejected. Each entry has the evidence (a measured number, a playtest, or a rule in the repository) and the current state. Entries marked "reason not recorded" are NEEDS INVESTIGATION items; the repository says what was done but not why.

## How it works
Failures are recorded in four kinds of source:
1. **Measured rejections** in the verdict and checkpoint memos (`docs/results/VERDICTS.md`, `docs/results/DIRECTION_2026-10-13_checkpoint1..3.md`). Each rejection has a number.
2. **Playtest rejections** in the findings files (`docs/recording/FINDINGS_*.md`).
3. **Operating rules** written after a mistake (`docs/START_HERE.md` section 6, "Rules and traps (learned the hard way)", lines 67-80).
4. **Code comments** that state a rejected behaviour (for example "no legacy fallback" at `src/gi.inl:130`).

The entries below are grouped by type. "Current state" describes the code at 4db1d7d, checked by grep where that was possible.

## Code pointers
| What | file:line | Note |
|---|---|---|
| Fallback to a legacy GI path is refused | `src/gi.inl:130` | Logs "no legacy fallback" |
| AO v2 has no legacy fallback | `src/ao.inl:546` | Logs "AO OFF" |
| Atmosphere has no ray-march fallback | `src/atmos.inl:371` | Logs "no heavy ray-march fallback" |
| Stale log text for the removed ray-march | `src/atmos.inl:257,260` | Says "ray-march path" |
| Stale header of the GI file | `src/gi2.inl:1` | Says it falls back to the legacy path. Contradicts `src/gi.inl:130` |
| Temporal shadow option (off) | `src/light.inl:108` | Stable noise cannot be integrated by TAA, so full taps are kept |
| Procedural noise is not used for relief | `src/tess.inl:9` | "No procedural noise" |
| Character tessellation excluded | `src/tess.inl:107` | |

## Settings
Settings that record a rejected approach (the default now is the safe one):

| Key | Code default | Shipped default | Note |
|---|---|---|---|
| `[shadows] Temporal` | see [../reference/settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md) | 0 (README says "not recommended" to set 1, `README.md:303`) | Per-frame shadow noise |
| `[gi] Mode` | 1 (Hi-Z), `src/gi2.inl:11` | 1 | Mode 0 is the legacy GI path, still selectable, not an automatic fallback |
| `[fog] SeeThruDepth` | 0, `src/atmos.inl:299` | 0, `src/default_tyrender.ini:77` | Depth-write experiment, off |
| `[fog] LateEmitBoost` | 1.0, `src/atmos.inl:301` | 1.0, `src/default_tyrender.ini:83` | Fire-haze experiment, off |
| `[shadows] CloseCracks` | see settings page | 0 (turned off in v0.3.4) | `docs/RESUME.md:68` |
| `[debug] ForceFallback` | 0, `src/ao.inl:491` | not set | Test bits, see [../reference/settings-system-debug.md](../reference/settings-system-debug.md) |

## Cost
Measured numbers for the rejected options (from the v0.3 merged build unless stated; see `docs/results/VERDICTS.md`):

| Option | Measured | Verdict |
|---|---|---|
| Legacy AO | +5.76 ms total frame over stock (`docs/results/VERDICTS.md:22`) | Not in any preset |
| Legacy GI, 4 rays | +16.94 ms (`docs/results/VERDICTS.md:24`) | Not in any preset |
| Hi-Z GI, 1 ray | +4.68 ms (`docs/results/VERDICTS.md:23`) | High and Ultra only |
| Ray-march atmosphere | Frame delta +8.8 ms, pass 4.3 ms (`docs/results/VERDICTS.md:5`) | Dropped |
| Environment tessellation (Scope=all) | +6.22 ms (`docs/STATUS_2026-10-13.md:34`) | Ultra only |
| Forced-fallback frame, before the OFF policy | 30.72 ms (`docs/results/DIRECTION_2026-10-13_checkpoint2.md:10`) | Replaced by the OFF policy (6.96 ms after) |

## How to verify
- Legacy paths are not used in any preset: open `src/launcher_ui.inc` and check the preset values, or `tools/presets.ps1`.
- The fallback policy: set `[debug] ForceFallback=31` in a test ini, run the game, and check that the log has "[fallback] ... OFF" lines and no "legacy" lines (`src/ao.inl:546`, `src/gi.inl:130`, `src/atmos.inl:371`).
- The ray-march is gone: `grep -rn "raymarch\|ray-march" src` should return only log strings and comments (`src/atmos.inl:257,260,371,448`).

## Known issues
- The log strings in `src/atmos.inl:257,260` still say "ray-march path" although that code is removed. A reader grepping the log will be misled.
- `src/gi2.inl:1` describes a legacy fallback that the code no longer has.

## Failed and abandoned approaches

### A. Rendering approaches

| ID | Approach | What happened | Why dropped or changed | Evidence | Current state |
|---|---|---|---|---|---|
| F-A1 | Engine's own SSAO (`SSAO=true` in the game's settings) | Stalls the menu. Cause not found. | Not used. Clash guard warns if it is on | `docs/START_HERE.md:88`, `src/tyrender.cpp:657` | Not used. Warning only. Cause: NEEDS INVESTIGATION (N-5) |
| F-A2 | Proxy DLL (replacing a system DLL) to inject the renderer | Not used. The DLL is injected through a patched copy of the exe | Reason not recorded in the repository | `docs/START_HERE.md:90` | Not used. Reason: NEEDS INVESTIGATION |
| F-A3 | Starting the game in windowed `-level z1` for tests | No longer works in windowed mode | The harness loads a save instead | `docs/START_HERE.md:87` | Not used |
| F-A4 | Ray-march atmosphere (per-pixel march) | Posterised blotches on far rock. Cost 4 to 9 ms | Dropped; froxel kept | `docs/results/VERDICTS.md:5`, `docs/results/DIRECTION_2026-10-13_checkpoint1.md:22` | Removed. Only log text remains (`src/atmos.inl:257,260`) |
| F-A5 | Legacy AO (`[ao] Legacy=1`) as the default or as automatic fallback | +5.8 ms. Not better | Dropped from presets; no automatic fallback | `docs/results/DIRECTION_2026-10-13_checkpoint1.md:23`, `docs/results/DIRECTION_2026-10-13_checkpoint2.md:20` | Kept as an option (`README.md:282`), not in presets |
| F-A6 | Legacy GI (linear march) as the automatic fallback when Hi-Z fails | Forcing every effect onto its fallback path gave a frame of 30.72 ms (the figure includes the legacy AO and ray-march paths too) | Replaced by "GI OFF" | `docs/results/DIRECTION_2026-10-13_checkpoint2.md:10,20`, `src/gi.inl:130` | Legacy GI is still selectable (`[gi] Mode=0`, `src/gi2.inl:20-22` sweep table). Not an automatic fallback. The plan wording "legacy GI removed" is wrong (see Discrepancies) |
| F-A7 | Hi-Z GI with 2 or 4 rays | Changes the image by only 1 to 3 out of 255. Costs +5.5 and +7.1 ms | Default off for Balanced; 1 ray for High and Ultra | `docs/results/VERDICTS.md:7`, `docs/results/DIRECTION_2026-10-13_checkpoint1.md:21` | Kept as options (`README.md:285`) |
| F-A8 | Per-frame shadow noise rotation (IGN rotation changes each frame) | Stipple, and a visible "shadow pattern changes per frame" | Off by default; stable noise kept | `docs/results/DIRECTION_2026-10-13_checkpoint1.md:11`, `README.md:303`, `src/light.inl:108` | Off by default |
| F-A9 | Tessellation of the environment (Scope=all) with the first relief version | Visible change almost nil (MAD 1.4 to 2.9 out of 255). Root cause: the level textures are very low-resolution | Replaced by the albedo detail layer; environment tessellation only for Ultra | `docs/results/VERDICTS.md:31`, `docs/results/VERDICTS.md:43` | Replaced. Scope=all still exists (Ultra only) |
| F-A10 | Procedural noise as surface relief | Noise unrelated to the texture. Playtest 1: "deep fried" look on props and characters | Relief now comes from the albedo | `docs/recording/FINDINGS_AND_PLAN.md:8,18,22`, `src/tess.inl:9` | Replaced |
| F-A11 | Tessellating characters (Scope=skin for all) | Ridges and aliasing on Ty | Characters excluded | `docs/recording/FINDINGS_AND_PLAN.md:17`, `src/tess.inl:107` | Replaced |
| F-A12 | Violet shade tint for shadows | Colour cast on the bridge, trees, Ty | Neutral warm tint | `docs/recording/FINDINGS_AND_PLAN.md:10` (F4), `docs/todo.json` T54 | Replaced |
| F-A13 | Light from shadow maps through divergent shader branches (sampling the texture with implicit LOD) | Dotted bright line in the shadows | Fixed with an explicit LOD of 0 | `docs/PLAN_EXEC.md:8`, `src/tyrender.cpp:202` (`textureLod(t,p,0.0)`) | Fixed in v0.3.4 (confirmed on Medium only) |
| F-A14 | New shadow filter (Vogel disc) at first measured as slower | Slower than the old filter (+5.7 vs +1.6 ms) | Later measurement: new filter cheaper (+1.06 vs +1.66 ms) | `docs/results/VERDICTS.md:8`, `docs/results/VERDICTS.md:33` | Replaced by the measured result |

### B. Water and fog

| ID | Approach | What happened | Why dropped or changed | Evidence | Current state |
|---|---|---|---|---|---|
| F-B1 | Water flicker fixed by removing water from GI sources (`ExcludeSources` bit 2) | Helped the GI side. On Medium, where GI is OFF, the blue pulse on Ty still happens | The cause is not GI on Medium. Suspect: hemisphere, specular or the engine's water reflection | `docs/PLAN_EXEC.md:11`, `docs/results/R6_config_diff.md:5`, `docs/RESUME.md:19` | Open (#5). Bisect bits in `[pbr] DebugWater` |
| F-B3 | Fog over trees by depth write (`SeeThruDepth=1`) | Default is 0. Trees still "through fog" in the 2026-10-17 frames (not confirmed) | Not yet tested in game | `src/atmos.inl:346-357`, `docs/PLAN_NEXT.md:32` | Off. A/B test planned |
| F-B4 | Fire haze by `LateEmitBoost` | Not tested | Default 1.0 (off) | `src/default_tyrender.ini:82-83`, `docs/PLAN_NEXT.md:24` | Off, untested |

### C. Process and tooling

| ID | Approach | What happened | Why dropped or changed | Evidence | Current state |
|---|---|---|---|---|---|
| F-C1 | Taking the ini as an argument (`-IniFile`) to the game-start harness | The restart-only keys (`MapScale`, `Taps`, `[tess] Enable/Scope/MaxLevel`, `[pbr] Enable`) were not read. An Ultra shot run was invalid | Only the `-AB` variants apply their ini. Copy the ini into the game folder first | `docs/START_HERE.md:70`, `docs/RESUME.md:82` | Rule in force |
| F-C2 | Automatic merge with `merge_agent.ps1` for new files | Cannot copy new files. CRLF conflicts fail | Copy new files by hand; use `diff -u` and `patch` | `docs/START_HERE.md:71`, `docs/RESUME.md:54` | Rule in force |
| F-C3 | RenderDoc for GL state work | Cannot replay the compatibility profile | Use the state guard and bisect instead | `docs/START_HERE.md:93` | Rule in force |
| F-C4 | Per-pass GPU timers as the cost measure | Under-count: full-resolution copies and resolves are hidden. Hidden cost 1.5 to 2 ms per pass | Use frame totals and in-session A/B | `docs/results/VERDICTS.md:12-16`, `docs/START_HERE.md:76` | Rule in force |
| F-C5 | Static and slow-yaw motion tests to find the user's flicker | Did not reproduce the High and Ultra flicker at high fps | Use `sleepMs` low-fps emulation in `motion.req` | `docs/STATUS_2026-10-13.md:64`, `docs/START_HERE.md:76` | Partly; see roadmap T78 |
| F-C6 | Hardlinked agent DLL or ini in the game folder | Stale DLL could be tested | Never hardlink | `docs/START_HERE.md:68` | Rule in force (reason partly inferred) |
| F-C7 | Community A/B grids made from Ultra captures | Invalid: the MapScale change needs a restart and the captures used the wrong settings. Also rug streaks and fan smear | Re-capture after the Ultra change; drafts not for sharing | `docs/RESUME.md:82,84`, `docs/PLAN_R6.md:2` | Draft only |
| F-C8 | Failed capture attempts kept in the community folder | Names end in `_DO_NOT_SHARE` | Not published | `docs/RESUME.md:11` | Not published |
| F-C9 | Visual verdicts from cross-session images | Images from different sessions are not pixel-comparable | Use in-session A/B only | `docs/START_HERE.md:76`, `docs/results/VERDICTS.md:13` | Rule in force |
| F-C10 | Screen-based check of the game window (computer-use screenshots and key presses) | Cannot see or drive the GL window | Use the harness snap and key scripts | `docs/START_HERE.md:73` | Rule in force |
| F-C11 | Shipping the test zip with an old ini (Medium header, Scope=skin) | The zip ini did not match the intended test preset | Check the ini before release. Zips are not edited | `docs/RESUME.md:58` | Open (release blocker, see roadmap) |
| F-C12 | Unattended game test while the owner is playing | Would take the game window | One game at a time; announce first | `docs/PLAN_NEXT.md:43`, `docs/PLAN_CONSOLIDATED.md:24` | Rule in force |

### D. Recent attempts that did not fully fix the problem

| ID | Target | Attempt | Result | Evidence | Current state |
|---|---|---|---|---|---|
| F-D1 | High and Ultra flicker (#6) | GI blur, noise cycle, temporal blend, source mask (v0.3.3) | Contact shadows alone only +0.035 flicker. High and Ultra still flicker in the 2026-10-13 playtest | `docs/recording/INDEX.md` R4-1, `docs/STATUS_2026-10-13.md:62` | Partly. 2026-10-17 record now blames AO flicker (#1), see Discrepancies in [known-issues.md](known-issues.md) |
| F-D2 | Red tint on the ground under Ty (GI bleed) | Character GI exclusion (v0.3.4) | Culprit not isolated. Red remains in sand samples in all variants | `docs/RESUME.md:66`, `docs/todo.json` T84 | Open |
| F-D3 | Grass ghosting | `[post] TAAReject=1.0` (v0.3.4) | Marked "unproven" | `docs/RESUME.md:68` | Open (roadmap #2) |
| F-D4 | Footprints glow | Texture-name tag system (v0.3.3) | Footprints still glow | `docs/recording/INDEX.md` E1, R4-7 | Open (#8). Clamp is off by default (`src/tex.inl:72`) |
| F-D5 | Portal light on the ground | Half-resolution spill design | Not built. "v0.4 unless trivial" | `docs/FIX_PLAN_20261016.md:37`, `docs/PLAN_CONSOLIDATED.md:51` | Deferred |
| F-D6 | Water reflection pulse on Ty | Water-stable mode (v0.3.3) | Not confirmed by the owner | `docs/recording/INDEX.md` W1, `docs/RESUME.md:29` | Open (#5) |
| F-D7 | Distance-based relief fade for all effects | Only the relief fade has keys (`[tess] ReliefFadeStart/End`) | Other effects not started | `docs/RESUME.md:32`, `src/tess.inl:57-58` | Open (#10) |
| F-D8 | Overlay F10 | In build `cda9364` opening the panel broke all geometry until restart | Key default changed to F10; regression still open | `docs/PLAN_NEXT.md:19` | Open (#12, #79) |

## NEEDS INVESTIGATION
- **N-5 Why engine `SSAO=true` stalls the menu.** Method: the owner's `TyOutputLog.txt` and the game's settings with SSAO on and off. RE of the SSAO render target (`docs/START_HERE.md:88`).
- **F-A2 reason for not using a proxy DLL.** Method: ask the owner; check `docs/START_HERE.md` section 7 for a fuller note in the owner's archive (not in repo).
- **F-A6 whether any user still depends on legacy GI.** Method: check `[gi] Mode` values in shared ini files; ask testers.
- **F-C11 zip ini contents.** Method: open the zip for the release and compare with the intended preset (`docs/RESUME.md:58`). The zip is not in the repository.
- **F-D2 which pass makes the red tint.** Method: `[debug]` pass bisect with the in-game measurement, one term per run.

## Discrepancies
1. **"Legacy GI slow path removed".** `docs/PLAN_EXEC.md` and the plan text describe legacy GI as removed. The code keeps it as `[gi] Mode=0` (`src/gi2.inl:20-22`, `src/gi.inl:131-140`). What was removed is the automatic fallback.
2. **Ray-march.** `docs/results/DIRECTION_2026-10-13_checkpoint1.md:22` says "keep as GL < 4.3 fallback only". The later policy (`docs/START_HERE.md:79`, `src/atmos.inl:371`) turns atmosphere off instead. The code has no ray-march path left; only log text (`src/atmos.inl:257,260`).
3. **Legacy fallback in the header.** `src/gi2.inl:1` says it "falls back to the legacy linear-march path in gi.inl on any failure". `src/gi.inl:130` says there is no legacy fallback. The code is correct; the header is stale.
4. **README on fallbacks.** `README.md:265` says effects switch off and the renderer "does not fall back to a slower path". This matches the code.
5. **Hardlinks and tests.** `docs/START_HERE.md:68` gives a reason only as a rule. The reason is inferred: a hardlinked file would silently test an old build.

## See also
- [decisions-log.md](decisions-log.md) for the decisions that followed these failures
- [known-issues.md](known-issues.md) for the open defects these attempts were aimed at
- [history-and-timeline.md](history-and-timeline.md) for the dates
- [../reference/settings-system-debug.md](../reference/settings-system-debug.md) for the test keys
- [../how-it-works/11-fallbacks-and-failure-handling.md](../how-it-works/11-fallbacks-and-failure-handling.md) for how fallbacks work now
