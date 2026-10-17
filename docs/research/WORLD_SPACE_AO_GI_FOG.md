# World-space alternatives for AO, GI and fog colour (issue #15)

Status: research only. No code changed for this doc. All cost numbers are estimates until a GPU-timer run confirms them.
Licence rule (from `docs/research/minecraft_shaderpack_port_ranking.md`): techniques are described, not code. Re-implement from the description in our own code. Nothing from the shader pack is copied.

## What the playtests say (2026-10-13 to 2026-10-17)

- AO flickers on High and Ultra, and TY's feet show grain on Medium (`docs/recording/FINDINGS_ROUND4.md`, playtest 2026-10-17 in project memory).
- Fog colour shifts as the camera turns, so the same distant haze looks a different colour from a different heading.
- GI on High and Ultra flickers blue on the character. Water was the source (`FINDINGS_ROUND4.md`, 3:18 and 3:48).

Common cause: AO, GI and the colour part of fog are all computed from the current screen or camera state. Anything that depends on the view changes when the camera moves, and screen-space samples disappear or reappear at the screen edges and behind occluders. That is the flicker and the drift.

## Current paths (read from source, not re-measured)

| Effect | Where | Depends on view? |
|---|---|---|
| Screen-space AO, temporal accumulation, fog fade (`[ao] FogFade`) | `src/ao.inl` | Yes: depth buffer and previous-frame reprojection |
| Hi-Z screen-space GI (`[gi] Mode`, `Rays`) | `src/gi.inl`, `src/gi2.inl` | Yes |
| Froxel atmosphere, haze colour (`[atmos] Froxel`) | `src/atmos.inl` | Yes: froxel grid is camera-aligned |

## Candidate world-space alternatives

1. **Baked world AO field (static scene).** TY's level geometry does not move. Bake an ambient-occlusion value per world position into a 3D texture or per-vertex colour at load time or offline. Runtime cost is one texture fetch per shaded pixel, and nothing depends on the depth buffer, so there is nothing to flicker. Costs: memory for the volume and a bake step. Moving characters still need the screen-space or capsule AO that already exists (`[ao] CharRadius`). Expected to remove most of the static-geometry AO flicker. Estimate: under 0.3 ms runtime (est.).

2. **Irradiance probes on a world grid for GI.** Place a sparse grid of probes over the play area. Each probe stores low-order irradiance (for example a few SH or cube coefficients) and is updated slowly, not every frame. Shaded surfaces blend the nearest probes. Water and the character do not feed back into the probes directly, which removes the blue flicker source. Estimate: 0.5 to 1.5 ms (est.) depending on update rate. This is the largest change and needs its own issue.

3. **World-anchored fog colour.** The shift with camera heading is likely a colour that comes from view direction (inferred from the code path, not yet measured). Compute the haze colour from the sun direction, sun elevation and world height, then apply a fixed sky-average tint. No per-pixel view term is needed. This is a cheap experiment (a uniform change in `atmos.inl`) and does not add cost. It is the easiest of the four to test. It belongs to the fog work, so it is not in the PR for #16.

4. **Distance-based AO fade instead of screen fade.** Replace the screen-space fade with a fade on world distance from the camera (`[ao] FadeStart` / `FadeEnd` already exist as the hook). Keeps the far haze from pulsing. No new cost.

## Not recommended for this project

- Full world-space ray-traced GI or voxel cone tracing: cost well above the 4.6 ms headroom at Medium (`docs/research/minecraft_shaderpack_port_ranking.md`).
- Per-pixel volumetric lightshafts: the froxel atmosphere already gives the same effect more cheaply.

## Suggested order

1. Fog colour anchored to sun and height (idea 3). Cheapest test, directly addresses the playtest note.
2. Distance-based AO fade (idea 4). Config-only change.
3. Baked world AO field (idea 1), behind a `[ao]` key, default off until measured.
4. Irradiance probes (idea 2), as a separate issue, after the water fix so the probe input is clean.

Each step needs a GPU-timer run and an A/B shot from the same camera before it goes into a preset. The playtest is the acceptance test, as with the other v0.3.5 items.
