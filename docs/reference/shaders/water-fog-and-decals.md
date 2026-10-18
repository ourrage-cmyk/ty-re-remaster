# Water and decals (fog boosts, water mask, shading switches, footprint clamp)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who chases the blue flicker or the lily pads).

## Purpose

"Water" is not one thing in this renderer. The code has three different sets of draws that people call water, and each one feeds a different switch:

1. **Name-based water.** Textures whose file name contains "water", plus `[fog] WaterNames`. These get `WaterBoost` on the fog and the `TF_WATER` flag.
2. **Policy-based water.** Every emissive or transparent texture (the policy column `waterTag`). These get the stable-shading switches `WaterStable` and `SkyFresnelSkipWater`, which remove bump, relief, specular and sky reflection on those draws.
3. **GI source mask.** Every draw whose class is excluded from GI (emissive, transparent or character by default). The mask feeds `WaterFog`, `WaterMaskSoft` and the GI colour exclusion.

This page explains each set, the decal footprint clamp (issue #8), and the open problems that the playtests raised. The fog pass itself is on [atmosphere-fog.md](atmosphere-fog.md).

## How it works

1. **Tagging at upload (`src/tex.inl:187-191`).** When a texture is uploaded, its file name is looked up in the embedded tag table and in the optional `tyrender\texture_tags.csv` override (`:187`). Then:
   - a name containing "water" sets `TF_WATER` (`:190`);
   - each `[fog] WaterNames` substring (lower case) also sets `TF_WATER` (`:191`, parsed at `:77-78`);
   - if `[decal] Clamp=1` and the name is in `[decal] Names`, `TF_DECAL` is set (`:188`).
   The flags are saved per texture id (`:192`).
2. **Policy row.** The flags map to one of seven material classes (`src/tex.inl:49-56`). Each class has a policy row (`:39-48`). The columns that matter here are `giBits` (which GI exclusion bits apply), `waterTag` (emissive or transparent, not character), and `seeThru` (transparent, drawn with blending).
3. **Stable shading (`WaterStable`).** Before each draw, `TexDrawPrep` calls `TexSetWater`, which sets the per-program uniform `tyWater` to 1 for `waterTag` draws (`src/tex.inl:208-215`, `:234`). The gate is `g_texWaterNoFresnel`, which is `WaterStable | SkyFresnelSkipWater` (`src/pbr.inl:20`). In the PBR shader `tyOn = (1 - tyTex) * (1 - tyWater)` turns off bump, relief and specular (`src/pbr.inl:76`), and the sky Fresnel term is multiplied by `(1 - tyWater)` (`:157`).
4. **Water fog boost (late draws, `WaterBoost`).** For draws that come after the fog pass, `LateFogBegin` uses `[fog] WaterBoost` for see-through draws with `TF_WATER`, and `FoliageBoost` for other see-through draws (`src/atmos.inl:335-340`). The boost is applied only when it is above 1.0.
5. **GI source mask (`src/gi2.inl:32-55`).** Before a draw whose class has `giBits` that match `[gi] ExcludeSources` (default 7, all three bits), `GIMaskBegin` redraws the same geometry with colour writes off except alpha, and sets alpha to 0 (`:38-50`). `GIMaskEnd` restores the state (`:52-55`). The mask is copied from the default framebuffer into a half-resolution texture in the Hi-Z GI path (`:411-427`, with `g_giMaskRan` set at `:427`).
6. **Water fog from the mask (`src/atmos.inl:92-102`, `:476-477`).** In composite mode 3, a pixel whose mask alpha is below 0.25 gets a stronger fog: transmittance is raised to `1 + WaterFog` (`:99-102`). With `WaterMaskSoft=1`, the mask is averaged over 3 by 3 texels and smoothed (`:93-98`). The mask is bound only when GI ran this frame and drew at least one mask draw (`:476`).
7. **Water depth (`SeeThruDepth`).** Optionally, see-through draws drawn before the depth copy write depth, so the fog pass reads the surface behind them (`src/atmos.inl:346-356`). Default 0.
8. **Decal clamp (footprints, #8).** With `[decal] Clamp=1`, a draw whose albedo has `TF_DECAL` sets `tyDecal` to 1 (`src/tex.inl:216-223`, `:227`). Programs whose output line is the stock `FINAL_COLOR = diffuseColour;` are rewritten so that, for decal draws, luminance above `Max` (0.9) is scaled down to `Max` (`:247-257`). The uniform location is cached at link (`:258-261`).

```
texture upload (name) --> flags: TF_WATER (name), TF_DECAL (name, if Clamp=1)
        |                   |
        v                   v
policy row (class)      decal rewrite of output line
  |        |      |          |
waterTag  seeThru giBits   tyDecal per draw
  |        |      |
tyWater   late   GI source mask (alpha 0 under excluded draws)
(WaterStable)  boost        |
               (WaterBoost  v
                for TF_WATER) water fog in composite mode 3 (WaterFog, WaterMaskSoft)
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Name flags at upload | `src/tex.inl:187-192` | `TF_WATER`, `TF_DECAL`, class |
| Flag bits | `src/tex.inl:12` | `TF_WATER = 32`, `TF_DECAL = 16` |
| Policy table | `src/tex.inl:39-48` | `waterTag`, `seeThru`, `giBits` |
| Class from flags | `src/tex.inl:49-56` | `MatClassOfFlags` |
| Config (decal, water names) | `src/tex.inl:72-78` | `LoadTexTags` (startup only) |
| Per-draw water uniform | `src/tex.inl:208-215` | `TexSetWater` |
| Per-draw decal uniform | `src/tex.inl:216-223` | `TexSetDecal` |
| Draw prep | `src/tex.inl:224-239` | calls both setters |
| Decal shader rewrite | `src/tex.inl:247-257` | output line rewrite and clamp |
| Uniform locations | `src/tex.inl:258-261` | `TexOnLink` |
| PBR water config | `src/pbr.inl:19-25` | `WaterStable`, `SkyFresnelSkipWater`, `DebugWater` |
| PBR water terms | `src/pbr.inl:76`, `:157`, `:163` | `tyOn`, sky Fresnel, bit 1 |
| Late water boost | `src/atmos.inl:329-345` | `WaterBoost`, `FoliageBoost` at `:339` |
| Water fog in composite | `src/atmos.inl:92-102`, `:476-477` | mask, soft mask, `WaterFog` |
| GI mask draws | `src/gi2.inl:32-55` | `GIMaskBegin` / `GIMaskEnd` |
| GI mask copy | `src/gi2.inl:411-427` | half-resolution blit |
| Exclusion key | `src/gi.inl:93` | `[gi] ExcludeSources`, default 7 |
| GI enable | `src/gi.inl:91`, `:105` | default 1 in code |

## Settings

Full tables are in [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md) and [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md). Live means re-read by the `reload` trigger (`src/tyrender.cpp:598`). Restart is needed for keys read in `LoadTexTags`, which is not on the reload list.

| Key | Default in code | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[fog] WaterBoost` | 1.0 (`src/atmos.inl:300`) | 1.0 (`default_tyrender.ini:81`) | live | engine fog multiplier for water-named see-through late draws. 1.0 = off |
| `[fog] WaterNames` | empty (`src/tex.inl:77`) | empty (`:89`) | restart (`LoadTexTags`) | extra name substrings that count as water |
| `[fog] WaterMaskSoft` | 0 (`src/atmos.inl:303`) | 0 (`:87`) | live | 3 by 3 averaged mask with smooth edge |
| `[atmos] WaterFog` | 1.0 (`src/atmos.inl:297`) | not set | live | extra transmittance power under the mask. 1.0 = on, but only where the mask exists |
| `[gi] Enable` | 1 (`src/gi.inl:91`) | 0 (`default_tyrender.ini:8`) | live | GI off means no mask, so `WaterFog` has nothing to read |
| `[gi] ExcludeSources` | 7 (`src/gi.inl:93`) | not set | live | bit 1 emissive, bit 2 transparent, bit 4 character |
| `[pbr] WaterStable` | 1 (`src/pbr.inl:20`) | 1 (`default_tyrender.ini:59`) | live (uniform per draw) | drops bump, relief, specular and sky Fresnel on waterTag draws |
| `[pbr] SkyFresnelSkipWater` | 0 (`src/pbr.inl:20`) | 0 (`:63`) | live | sky Fresnel only, same draws |
| `[pbr] DebugWater` | 0 (`src/pbr.inl:23`) | 0 (`:61`) | bit 1 and 2 restart (compile-time edits, `:134`, `:163`); bit 4 live (`src/atmos.inl:476`) | bisect bits: 1 spec and Fresnel, 2 bump and relief, 4 water fog, 8 tint and hemisphere |
| `[decal] Clamp` | 0 (`src/tex.inl:72`) | 0 (`default_tyrender.ini:69`) | restart | caps luminance of footprint draws |
| `[decal] Max` | 0.9 (`src/tex.inl:73`) | 0.9 (`:70`) | restart | the cap |
| `[decal] Names` | `ty_footprint.dds, ty_footprint2.dds` (`src/tex.inl:74`) | same (`:71`) | restart | file names that count as decals |
| `[tex] LogUnknown` | 0 (`src/tex.inl:68`) | not set | restart | logs untagged draws by name every 600 frames |
| `[tex] Debug` | 0 (`src/tex.inl:67`) | not set | restart | logs each texture upload with its flags (`src/tex.inl:195`) |

## Cost

- **Not measured separately.** `WaterFog`, `WaterMaskSoft` and the water boosts are part of the atmosphere composite. Their cost is not recorded.
- **GI mask (inferred from its place).** The mask is a second draw of each excluded draw plus a half-resolution blit. `docs/PERFORMANCE.md` (test build v0.3.3) says the High preset gained about 2.5 ms from the bounce-light changes "(source mask, blur and noise)" and that "the split between the parts is not measured yet, and the cost of the mask is to be optimised".
- **Decal clamp.** The rewrite adds a few instructions only to programs that end in the stock output line (`src/tex.inl:250-251`). Not measured.

## Limits

- **Only textures named "water" get the name-based boost.** A water surface with another name is not boosted unless it is listed in `WaterNames` (`src/tex.inl:190-191`). The lily-pad names are not known yet (see NEEDS INVESTIGATION).
- **`WaterFog` depends on the GI mask.** It cannot work with `[gi] Enable=0`, which is the shipped Medium setting (`default_tyrender.ini:8`). It also needs the Hi-Z path, because only that path copies the mask (`src/gi2.inl:411-427`). This is inferred from the gating; it has not been run.
- **The mask is not water-only.** By default it covers emissive, transparent and character draws (`src/gi2.inl:33`, `src/tex.inl:46-47`). `WaterFog` therefore also applies at character pixels and at transparent draws such as foliage cards and particles (inferred). The comment at `src/atmos.inl:92` describes the mask as water, glass and glow only.
- **Shading switches remove terms, they do not add reflection.** `WaterStable` and `SkyFresnelSkipWater` turn terms off (`src/pbr.inl:76`, `:157`). The mod does not add reflection or refraction to water.
- **Transparent draws after the fog pass are not fogged by the volume.** Only the engine fog and the late boosts reach them (`src/atmos.inl:325-327`).
- **Decal clamp is by file name and by shader shape.** Programs that do not end in the stock output line are skipped (`src/tex.inl:250-251`). Decals that use other file names are not clamped.
- **Mask resolution and threshold.** The mask is half resolution and sampled with nearest filtering, and the cut is at alpha 0.25 (`src/atmos.inl:92`, `src/gi2.inl:418`). Edges can shimmer without `WaterMaskSoft`.

## Known issues

- **#5, blue flicker on Ty in water.** History: `WaterStable` (default on, merged, "Untested" in CHANGELOG), `SkyFresnelSkipWater` (v0.3.5), the `DebugWater` bisect bits (#62, `src/pbr.inl:21-25`), the GI exclusion for water (v0.3.3 per `docs/PLAN_CONSOLIDATED.md:38`), and `WaterMaskSoft` (#5 and #92, `CHANGELOG.md`). The owner's playtest of build 4a2ef95 (`CHANGELOG.md`, "Playtest of 4a2ef95", dated 2026-10-18; `README.md:213`) says the water brightness on the character ramps up and down slowly, with the GI bounce from the blue texture as the suspected source, "not a frame flicker". The 2026-10-17 findings file did not find the flicker itself: the nearest moment, the water entry at 1:57, is a scene transition (`docs/PLAYTEST_2026-10-17_FINDINGS.md:28-35`, `:116-120`). `docs/FIX_PLAN_20261016.md` T74 (reflection pulse, `ExcludeSources`) is marked "game (confirm)".
- **#4, lilies and rocks brighter than the water under them.** The attempts are `WaterBoost` (#74, `CHANGELOG.md`), `WaterNames` (#106), and the note that lily-pad names are still unconfirmed (`src/default_tyrender.ini:88-89`). A colour mask for lily pads at 0:10 to 0:25 found no pad pixels because the fog tint moves them out of the green range (`docs/PLAYTEST_2026-10-17_FINDINGS.md:169-174`). Not confirmed.
- **#104, water outline from above.** `README.md:213` lists it. The title is not in the repository (N-19).
- **#92 and the soft mask.** `WaterMaskSoft` (#92) is off by default and untested (`CHANGELOG.md`).
- **#8, footprints glow.** The decal clamp is the attempt, default off. The footprints moment was not located in the 2026-10-17 playtest (`docs/PLAYTEST_2026-10-17_FINDINGS.md:29`, `:94`). Status conflicts (see Discrepancies). Q181 is answered here.
- **Q175 and Q176, summary.** Water brightness and blue flicker: #5, #62 and #92 (above). Fog inconsistencies on water: #4, #74 and #106. The causes tried are listed above; none is confirmed.

## How to verify

1. **Name flags.** Set `[tex] Debug=1`, restart and load the level. Each upload logs `[tex] upload id=... name=... flags=...` (`src/tex.inl:195`, for the first 500 uploads). Flag 32 is `TF_WATER`, 16 is `TF_DECAL`, 2 is emissive, 4 is transparent (`src/tex.inl:12`).
2. **Untagged draws.** Set `[tex] LogUnknown=1` and restart. Every 600 frames the log lists the most-drawn untagged textures by name (`src/tex.inl:270-276`). Stand at the lily pond and read that list for the pad names (N-27).
3. **WaterBoost.** Set `[fog] WaterBoost=2.0` and reload, then look at a water-named see-through surface seen from above. It should take more fog than before. The boost only acts on draws after the fog pass (`src/atmos.inl:330`).
4. **Bisect the flicker.** One bit at a time in `[pbr] DebugWater`, restarting between bits 1 and 2 (compile-time). Bit 4 switches off the water fog, and the `[dbgwater] term OFF` line at start (`src/pbr.inl:25`) confirms the bit is set. The plan is to run the bisect at the 2:05 pond spot (`docs/PLAYTEST_2026-10-17_FINDINGS.md:67`).
5. **GI mask view.** Set `[gi] DebugView=3` to see the mask (`src/gi2.inl:33`). Use it to check whether Ty and foliage are in the mask.
6. **Decal clamp.** Set `[decal] Clamp=1`, restart, and compare footprints in a bright area. Expect the footprints to lose the glow above `Max`.
7. **WaterStable off.** `[pbr] WaterStable=0` and reload. The water surface should show bump, specular and sky reflection again. This is a per-draw uniform (`src/tex.inl:209`).

## NEEDS INVESTIGATION

- **NI, lily-pad texture names (N-27).** Which file names belong to the lily pads? Method: How to verify 2, at the 0:15 pond view; then add them to `[fog] WaterNames` and restart.
- **NI, does `WaterFog` reach character pixels?** (inferred from the mask defaults). Method: put Ty in water, compare `[pbr] DebugWater=0` with `=4`, and view `[gi] DebugView=3`.
- **NI, `WaterFog` with the shipped Medium (GI off).** (inferred to be inert). Method: compare `[atmos] WaterFog` at 1.0 and 3.0 with `[gi] Enable` at 0 and 1, at the same water view.
- **NI, cause of #5.** Is the ramp the GI bounce, the specular term or the fog? Method: DebugWater bisect as in How to verify 4, one bit per run, on the same spot, with the frame brightness measured on the character box as the playtest did.
- **NI, footprint cause (#8).** The docs disagree on the cause: `docs/PLAN_CONSOLIDATED.md:50` suggests decals blended after the depth copy; `docs/MATERIAL_CLASSIFICATION_PLAN.md:39` says the flags are not the cause but the decal shading is. Method: find the footprint moment in the recording first (not located, `docs/PLAYTEST_2026-10-17_FINDINGS.md:94`).
- **NI, issue titles #62, #92, #104, #106 (N-19).** Method: `gh issue view` by the reviewer.

## Discrepancies

- `src/atmos.inl:92` says the GI source mask marks "water/glass/glow". By default it also marks character draws (`src/gi2.inl:33`, `src/tex.inl:46-47`). Code wins.
- Three meanings of "water" in the repository: name rule (`src/tex.inl:190`), policy `waterTag` (`src/tex.inl:43-47`, `src/pbr.inl:19`), GI mask (`src/gi2.inl:33`). `src/default_tyrender.ini:79` describes `WaterBoost` as "texture name contains water". `src/default_tyrender.ini:58` describes `WaterStable` as "water-tagged draws (emissive/transparent texture tags)". Both are right for their key, but the sets differ.
- `CHANGELOG.md` (Unreleased, Added) says `WaterMaskSoft` "softens the water-fog mask edge". With `[gi] Enable=0` (shipped Medium) there is no mask, so the key does nothing in the shipped build (inferred).
- `src/default_tyrender.ini:80` says water "is drawn after the volumetric fog pass, so it is otherwise unfogged". Only the water drawn after the depth copy is unfogged by the volume. Water drawn before it is in the volume.
- Footprint status: `docs/PLAN_NEXT.md:16` says "Footprints are just bright white, not a light (#8, closed)". the first-pass `KNOWN_ISSUES_AND_HISTORY.md` says "open or closed depending on source". The decal clamp is default off (`src/tex.inl:21`, `default_tyrender.ini:69`), and the playtest did not find the footprints moment.
- `docs/PLAN_CONSOLIDATED.md:50` says the footprint hypothesis is "decals blended after the depth copy", and `docs/MATERIAL_CLASSIFICATION_PLAN.md:39` says the decal shading path. Both are hypotheses in the repository, not findings.

## See also

- [atmosphere-fog.md](atmosphere-fog.md): the composite, the late boosts and the engine fog hook
- [sky-estimate-and-haze-color.md](sky-estimate-and-haze-color.md): the haze colour the water fog mixes with
- [emitter-light.md](emitter-light.md): emissive draws, which are also in the GI mask
- [../how-it-works/07-texture-names-and-tags.md](../how-it-works/07-texture-names-and-tags.md): how a texture gets its name and flags
- [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md): `[fog]`, `[atmos]` and `[decal]` keys
