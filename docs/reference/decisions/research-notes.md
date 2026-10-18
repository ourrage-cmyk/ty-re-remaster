# Research notes (ideas only)
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone; no GPU timing run). Audience: C.

## Purpose
This page summarises the two research notes in `docs/research/`. Both notes propose ideas. Neither one is implemented in the renderer. Use this page to see what was studied, what each idea would cost (as far as the notes say), and what rule applies before any idea becomes code. The planned items are listed on [roadmap-and-open-work.md](roadmap-and-open-work.md). The licence rule is on [legal-and-licensing.md](legal-and-licensing.md).

It answers Q160 (what alternatives were studied for the lack of world-space data), gives the research half of Q087 (why the renderer stays screen-space), and gives the research part of Q225 (the shader-pack reuse rule).

## How it works
There are two notes:
1. **World-space alternatives for AO, GI and fog colour** (`docs/research/WORLD_SPACE_AO_GI_FOG.md`, 44 lines, issue #15). It asks whether data that does not depend on the camera could remove the flicker and drift seen in playtests.
2. **Shader pack port ranking** (`docs/research/minecraft_shaderpack_port_ranking.md`, 51 lines). It ranks techniques from a third-party Minecraft shader pack by value against cost for TY.

Working method stated in the notes:
- Every cost figure marked "est." is an estimate from reading code. None was measured (`minecraft_shaderpack_port_ranking.md:7`; `WORLD_SPACE_AO_GI_FOG.md:3`).
- The budget context is a Medium frame of 12.08 ms against a 16.7 ms budget, about 4.6 ms of headroom (`minecraft_shaderpack_port_ranking.md:5`; `WORLD_SPACE_AO_GI_FOG.md:34`). The 12.08 ms figure is a measurement from `docs/STATUS_2026-10-13.md:23` on build v0.3.2 (G1 gate). It is not a number for the current build.
- The notes say each step needs a GPU-timer run and an A/B shot from the same camera before it goes into a preset (`WORLD_SPACE_AO_GI_FOG.md:44`).

Common cause named in the world-space note: AO, GI and the colour part of fog are all computed from the current screen or camera state. Samples disappear at screen edges and behind occluders, and the result changes as the camera turns (`WORLD_SPACE_AO_GI_FOG.md:12`).

## Code pointers
The notes cite current code paths. These are the lines that the ideas would touch. Each was checked against the clone at 4db1d7d.

| What | file:line | Note |
|---|---|---|
| AO distance fade (AO goes to 1 between two view distances) | `src/ao.inl:418` | Active only when the end distance is above 0 |
| AO fade keys `FadeStart`, `FadeEnd` | `src/ao.inl:507-508` | Defaults 0 and 0, so the fade is off in code |
| AO fog fade keys `FogFade`, `FogFadePower` | `src/ao.inl:514`, `src/ao.inl:424` | Defaults 1.0 and 2.0 |
| AO character capsule `CharRadius` | `src/ao.inl:509` | Default 40 |
| Froxel atmosphere switch `Froxel`, `FroxelPixels` | `src/atmos.inl:304` | Defaults 1 and 12 |
| GI mode `Mode` | `src/gi.inl:95`, `src/gi.inl:130` | Default 1. Mode 0 is the legacy path; any other value uses Hi-Z |
| Baked vertex light used by the PBR surface code | `src/pbr.inl:132`, `src/pbr.inl:154` | The engine's vertex colour carries baked light for some surfaces |

No code for a baked world AO field, world probes or an irradiance grid exists. A search of `src/` for `bake`, `probe` and `irradiance` finds only comments about shadow filtering (`src/tyrender.cpp:166,193,201`). (inferred from the search; the ideas are new work.)

## Settings
These keys are the ones the notes touch. Live or restart status is on the settings pages.

| Key | Default in code | Effect | Link |
|---|---|---|---|
| `[ao] FadeStart` | 0 (`src/ao.inl:507`) | View distance where AO starts to fade | [settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md) |
| `[ao] FadeEnd` | 0 (`src/ao.inl:508`) | View distance where the fade ends; 0 turns it off | [settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md) |
| `[ao] FogFade`, `FogFadePower` | 1.0, 2.0 (`src/ao.inl:514`) | How far AO follows fog | [settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md) |
| `[atmos] Froxel`, `FroxelPixels` | 1, 12 (`src/atmos.inl:304`) | Froxel haze volume on or off, and its tile size | [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md) |
| `[gi] Mode` | 1 (`src/gi.inl:95`) | 0 legacy GI, otherwise Hi-Z | [settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md) |

Note: the "distance-based AO fade" idea uses keys that already exist. It needs a value change, not new code (`WORLD_SPACE_AO_GI_FOG.md:30`).

## Cost
Figures are from the notes. None is measured by this page.

| Idea | Cost as stated | Source |
|---|---|---|
| Fog colour anchored to sun and height | No added cost (est.) | `WORLD_SPACE_AO_GI_FOG.md:28` |
| Distance-based AO fade | No new cost (est.) | `WORLD_SPACE_AO_GI_FOG.md:30` |
| Baked world AO field | Under 0.3 ms per frame (est.), plus memory and a bake step | `WORLD_SPACE_AO_GI_FOG.md:24` |
| Irradiance probes | 0.5 to 1.5 ms (est.), depending on update rate | `WORLD_SPACE_AO_GI_FOG.md:26` |
| Catmull-Rom TAA history filter | 0.2 to 0.4 ms (est.) | `minecraft_shaderpack_port_ranking.md:21` |
| Tonemap change | Under 0.1 ms (est.) | `minecraft_shaderpack_port_ranking.md:20` |
| Shadow-edge bias | About 0.05 ms (est.) | `minecraft_shaderpack_port_ranking.md:19` |
| Few-tap jittered shadows (4 to 8 taps) | About 1 ms (est.), compared with 32 taps | `minecraft_shaderpack_port_ranking.md:22` |
| Low-tier AO (4 samples) | 0.2 to 0.4 ms (est.) | `minecraft_shaderpack_port_ranking.md:23` |
| Bloom pyramid at half resolution | 0.3 to 0.6 ms (est.) | `minecraft_shaderpack_port_ranking.md:24` |

Measured reference costs quoted by the ranking note (from `docs/STATUS_2026-10-13.md:31-36`, the build named in that file's table):
- Shadow maps 8192 with 32 taps: +5.12 ms.
- GI Hi-Z with 1 ray: +4.42 ms.
- Contact shadows at 0.35: +1.71 ms.
- AO off: saves 1.15 ms.
- Fog and atmosphere off: saves 2.09 ms.

## Ideas from the world-space note
1. **Baked world AO field.** Bake an AO value per world position at load time or offline, and read it with one texture fetch per pixel. Moving characters keep the existing screen-space or capsule AO (`WORLD_SPACE_AO_GI_FOG.md:24`).
2. **Irradiance probes on a world grid.** Store low-order light per probe, update probes slowly, and blend the nearest probes when shading. The note says this removes the blue GI flicker from water and characters. It is the largest change and needs its own issue (`WORLD_SPACE_AO_GI_FOG.md:26`).
3. **World-anchored fog colour.** Compute the haze colour from sun direction, sun height and world height, with a fixed sky tint, and drop the per-pixel view term. The note calls it the easiest idea to test (`WORLD_SPACE_AO_GI_FOG.md:28`).
4. **Distance-based AO fade.** Fade AO by distance from the camera rather than by screen position, using the existing fade keys. This stops far haze from pulsing (`WORLD_SPACE_AO_GI_FOG.md:30`).

Rejected in the same note: full ray-traced world GI or voxel cone tracing (cost far above the 4.6 ms headroom), and per-pixel volumetric light shafts (the froxel haze already covers the effect more cheaply) (`WORLD_SPACE_AO_GI_FOG.md:34-35`).

Suggested order: fog colour (idea 3), then the AO distance fade (idea 4), then the baked AO field behind a `[ao]` key that is off by default, then probes as a separate issue after the water fix (`WORLD_SPACE_AO_GI_FOG.md:39-42`).

## Ideas from the shader pack ranking
The ranking note lists six techniques to consider, a skip list, and an order. The pack's code is not in this repository. The note's own file list is not reproduced here.

| Rank | Idea (own words) | Verdict in the note |
|---|---|---|
| 1 | Shadow-edge bias: normal offset and slope bias, plus a penumbra tweak | Do first. Only the offset and slope parts transfer. The penumbra tweak needs baked vertex AO on terrain, which may not exist (`minecraft_shaderpack_port_ranking.md:19`) |
| 2 | Tonemap: lift shadows, compress highlights, keep blacks from going grey | Cheap. A/B against the current curve (`:20`) |
| 3 | TAA history filter: smoother history sampling and clipping | Only if ghosting remains after ranks 1 and 2 (`:21`) |
| 4 | Few-tap jittered shadows with TAA averaging | Prototype as a tier option, not the default. It adds per-pixel noise (`:22`) |
| 5 | Low-tier AO with 4 depth samples | Optional, for the Low preset only (`:23`) |
| 6 | Bloom pyramid | Optional, off by default. Not in the goal list (`:24`) |

Skipped in the note (`minecraft_shaderpack_port_ranking.md:28-32`): volumetric light shafts, voxel block light, screen-reflection and world reflections (cost 1 to 3 ms or more; revisit only for the water pulse), cloud shadows, and depth of field, lens flare, chromatic aberration, puddles and rain.

Suggested order: rank 2, rank 1, rank 3 if needed, rank 4 behind a tier flag, then ranks 5 and 6 if budget allows. The note projects under 0.5 ms (est.) for ranks 1 to 3 (`minecraft_shaderpack_port_ranking.md:36-42`).

## Licence and reuse rule
- The pack's licence treats redistributing parts of the pack, or modifying its code, as outside normal use. The note did not get a legal review (`minecraft_shaderpack_port_ranking.md:11`).
- Rule for this project: ideas only. Re-implement from the described behaviour in the project's own code. Do not paste or adapt the pack's shader code, and do not ship any of its files. Ask the owner before any port beyond an idea (`minecraft_shaderpack_port_ranking.md:13`).
- The world-space note repeats the rule: nothing from the pack is copied (`WORLD_SPACE_AO_GI_FOG.md:4`).
- This documentation set follows the same rule. The text on this page describes the ideas in our own words.

## Limits
- No cost in either note is measured. Every idea needs a GPU-timer run first (`[gpu]` lines, see [../how-it-works/10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md)).
- The ranking note read only parts of the pack. Most material paths and the water shaders were not read (`minecraft_shaderpack_port_ranking.md:44-51`). Water ideas are therefore incomplete.
- The notes are from a read of the pack, not a test of TY. Their claims about TY's surfaces (for example, vertex AO on terrain) need a capture to confirm.
- The world-space note calls the fog-colour cause "inferred from the code path, not yet measured" (`WORLD_SPACE_AO_GI_FOG.md:28`).

## Known issues
The ideas target these open defects (IDs from [known-issues.md](known-issues.md)):
- K-07 AO halo and flicker (#1): ideas 1 and 4 (world AO, distance fade).
- K-06 High and Ultra flicker (#6): ideas 1 and 2. The cause is disputed in the known-issues page.
- K-09 fog colour shift (#3): idea 3.
- K-08 TAA trails (#2): ranking rank 3.
- K-01 water pulse (#5): idea 2 (probes, after the water fix) and the screen-reflection skip item.
- K-15 shadow bright lines (#11): ranking rank 1. The known-issues page says this is closed on Medium. See the discrepancy below.

## How to verify
- Estimate versus measurement: run the game with the GPU timers on, then take the A/B pair from the same camera (see [../guides/ab-captures.md](../guides/ab-captures.md)). Compare `[gpu]` totals with and without the change.
- Check a cited line: `sed -n '418p' src/ao.inl` should show the distance fade (`src/ao.inl:418`).
- Check a key default: `grep -n 'FadeEnd' src/ao.inl` shows the default 0 at line 508.
- Check that no baked-field code exists: `grep -rn -i 'bake\|irradiance' src` should show no world-field code.

## NEEDS INVESTIGATION
- **R-1 Terrain vertex AO.** The ranking's rank 1 depends on whether level geometry carries baked AO in vertex colour alpha. The PBR code uses baked vertex colour (`src/pbr.inl:154`), but the alpha channel is not confirmed. Method: capture a level surface and read the vertex colour format from the engine's draw calls (`capframe` tooling; see the test-harness guide).
- **R-2 Cause of the fog colour shift.** The world-space note says view direction (inferred). The known-issues page lists HazeSky and GI as candidates. Method: hold the sun and the height fixed, turn the camera, and read the haze colour with the atmosphere debug view.
- **R-3 Source of the High and Ultra blue GI flicker.** The world-space note says water. The known-issues page says the cause is disputed. Method: A/B with the water masks (see `[pbr] DebugWater`, and the GI `ExcludeSources` key at `src/gi.inl:93`) on the same camera path.
- **R-4 Measured cost of each idea.** No measurement exists. Method: one GPU-timer run per idea before any preset change.
- **R-5 Water and foliage ideas.** The ranking did not read the pack's water shaders. Method: a second read of the pack by someone with the archive, under the rule above.

## Discrepancies
1. **Shadow cost attribution.** `minecraft_shaderpack_port_ranking.md:5,22` says "32-tap shadows +5.12 ms". The measured row in `docs/STATUS_2026-10-13.md:32` is "shadow maps 8192 + 32 taps". The +5.12 ms includes the 8192 map, not the tap count alone. Medium uses 4096 with 16 taps (`docs/STATUS_2026-10-13.md:10`).
2. **Build of the 12.08 ms figure.** The ranking note says "Medium measures 12.08 ms" without a build. `docs/STATUS_2026-10-13.md:23` shows it was measured on v0.3.2. The current build is not measured in these notes.
3. **Shadow line defect status.** `minecraft_shaderpack_port_ranking.md:19` calls the bright lines on Ty's face and bag open. `known-issues.md` (K-15) says the #11 shadow lines are closed on Medium. Both may be true for different presets. Check the preset before using either claim.
4. **Cause of High and Ultra flicker.** `WORLD_SPACE_AO_GI_FOG.md:8-10` names AO for one flicker and water for the blue GI flicker. `known-issues.md` (K-06) gives the cause as disputed between GI with contact shadows and AO.
5. **Cause of the fog colour shift.** `WORLD_SPACE_AO_GI_FOG.md:28` says view direction (inferred). `known-issues.md` (K-09) says HazeSky or GI, disputed.
6. **Source of playtest evidence.** The world-space note cites `docs/recording/FINDINGS_ROUND4.md` with video timestamps (3:18 and 3:48). These timestamps were not checked here. Transcript content is not quoted (see [legal-and-licensing.md](legal-and-licensing.md), L-4).

## See also
- [roadmap-and-open-work.md](roadmap-and-open-work.md) for the planned ideas and their order
- [known-issues.md](known-issues.md) for the defects these ideas target
- [legal-and-licensing.md](legal-and-licensing.md) for the licence position and the reuse rule
- [../concepts/constraints-and-limits.md](../concepts/constraints-and-limits.md) for why the renderer is screen-space
- [../shaders/ao-gtao.md](../shaders/ao-gtao.md) and [../shaders/atmosphere-fog.md](../shaders/atmosphere-fog.md) for the current AO and fog code
