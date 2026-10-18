# PBR surface detail (bump, relief, sun specular)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (tester, tunes `[pbr]`) and A (contributor).

## Purpose

The game ships only a diffuse texture per material. This rewrite adds surface detail that is derived from that texture, with no new assets:

- a per-pixel bumped normal, so the engine's own lights pick up fine relief;
- a short parallax relief near the camera, which shifts the albedo lookup;
- a sun specular highlight with a roughness that depends on the texture;
- a faint sky sheen at grazing angles.

It applies to the engine's lit world and prop variants. Characters are excluded unless the texture tags say otherwise. Water is held steady on purpose (see Known issues).

The rewrite is done on the engine's fragment shader text when it is compiled. Nothing is read from disk at run time for this feature. See [05-shader-source-rewriting.md](../how-it-works/05-shader-source-rewriting.md) for the mechanism.

## How it works

1. **Vertex shader.** When the program is eligible, a varying with the world position is added after the stock clip-position line (`src/pbr.inl:48-55`). The check requires the anchor text `gl_Position = VPMatrix * worldPos;` and the `v_normal` varying.
2. **Eligibility.** The fragment shader must define `LIT 1` and `TEX 1` (`src/pbr.inl:34-42`). `#define` values are matched exactly, so `LIT 10` does not match. `SKIN 1` programs are eligible only when `[tex] SkinnedByTag` is on (`src/pbr.inl:40`). With `[pbr] LitProps=0`, `LIT 1` programs are excluded (`:41`).
3. **Lite variant.** A program with `LIT 0`, `SHADOWNORMS 1`, `TEX 1` and `AREF 0` gets only the detail layer, not the specular. This covers ground, fences and rocks (`src/pbr.inl:44-47`, `:132-136`).
4. **Uniforms.** The fragment header declares `tyCam`, `tySunDir`, `tyPbr`, `tyPbr2`, `tyPbr3`, `tyRel`, `tyRel2`, `tyTex` and `tyWater` (`src/pbr.inl:63-68`). The header comment defines each component (`:65-66`). The height function `kTyHeightGLSL` from `src/tess.inl:163` is inserted here (`src/pbr.inl:70`).
5. **Tangent frame.** Tangent and bitangent are built per pixel from screen derivatives of the world position and of the UV (`src/pbr.inl:84-95`). No tangents are stored in the assets. The tangents are normalised per pixel (`:124`).
6. **Relief strength.** Three factors multiply the relief depth (`src/pbr.inl:83`, `:95`):
   - a distance fade from `tyRel2.x` to `tyRel2.y` (the relief fade range, set by the tessellation keys);
   - the ground or prop multiplier, blended by how upward the normal faces (`:94-95`);
   - the per-draw relief strength `tyPbr3.x` (from `[pbr] DetailStrength` times the tessellation live flag, `src/pbr.inl:199`).
   Relief is also faded out at grazing view angles (`:98`) and disabled when the depth is tiny (`:99`).
7. **Parallax search.** A linear search over at most 12 steps finds where the view ray crosses the height field (`src/pbr.inl:103-112`). The step count is between 3 and `tyRel2.z`, chosen by view angle (`:102`). The hit is refined by interpolation (`:108-110`). The albedo is then sampled at the shifted UV (`:115`).
8. **Bumped normal.** The height field is differenced over one texel of the sampled footprint (`src/pbr.inl:118-122`). The slope is clamped (`:123`). The normal is tilted by the slope times the strength (`:125`). The same strength is multiplied by a distance fade and a per-draw switch `tyOn` (`:76-77`).
9. **Cavities.** The diffuse colour is darkened in valleys by `Cavity` (`src/pbr.inl:126`, `:129`).
10. **Lighting.** The engine's light block uses the bumped normal `tyN` in place of `normalize(v_normal)` (`src/pbr.inl:141-142`).
11. **Sun specular.** Using the sun direction and the view direction (`:147-148`):
    - a GGX-style distribution (`:151`);
    - a height-correlated visibility term approximated with mixed terms (`:152`);
    - a Schlick Fresnel term (`:153`);
    - the highlight is scaled by a vertex-occlusion term from the vertex colour (`:154`), the distance fade and the switch `tyOn` (`:155`), and capped at 4.0 (`:156`).
12. **Roughness.** The base `Roughness` is lowered for bright albedo and raised on painted hue edges and in valleys. It is clamped to 0.25 to 1.0 (`src/pbr.inl:149`).
13. **Sky sheen.** A grazing Fresnel-like term multiplies a fixed sky tint (`src/pbr.inl:157-158`). A soft dust sheen at grazing angles is controlled by `Sheen` (`:160`).
14. **Switches.** `tyTex` is 1 for character textures and unknown skinned textures (`src/tex.inl:231`), and `tyWater` is 1 for water-tagged draws when `WaterStable` is on (`src/tex.inl:208-215`). Both set `tyOn` to zero (`src/pbr.inl:76`), which removes bump, relief, specular and sheen. Water also loses its sky Fresnel (`:157`).
15. **Uniform push.** `PbrUpdate` derives the camera position from the scene view-projection matrix (`src/pbr.inl:212-226`). It is called when the scene depth is copied (`src/tyrender.cpp:289`). `PbrApply` pushes the values on `glUseProgram` once per program per frame (`src/pbr.inl:189-203`, stamp at `:192`; hook at `src/tyrender.cpp:329`). The sun direction is the negated engine `shadowDir` (`src/pbr.inl:227`, captured at `src/atmos.inl:517`).

```
engine FS text (LIT 1, TEX 1)
  -> add v_tyWorld (VS), add header + relief helper (FS)
  -> bumped normal tyN (relief, parallax, cavity)
  -> engine light block uses tyN
  -> + sun specular (GGX-type) + sky sheen
  -> engine shadows and fog as before
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Config loader | `src/pbr.inl:16-32` | All `[pbr]` keys |
| Water and debug bits | `src/pbr.inl:19-25` | WaterStable, SkyFresnelSkipWater, DebugWater |
| Defines and eligibility | `src/pbr.inl:34-47` | `PbrDefine`, `PbrEligible`, `PbrLite` |
| VS rewrite | `src/pbr.inl:48-55` | World position varying |
| FS entry and anchors | `src/pbr.inl:57-61` | Required text |
| Header and uniforms | `src/pbr.inl:63-71` | Fade range is compiled into the text (`:68`) |
| Bumped normal and relief | `src/pbr.inl:73-130` | Tangent frame, parallax, height differences |
| Lite variant | `src/pbr.inl:132-136` | Relight by sun difference |
| Lighting hook | `src/pbr.inl:140-142` | `tyN` replaces `normalize(v_normal)` |
| Specular and sheen | `src/pbr.inl:144-161` | GGX-type term, Fresnel, sheen |
| Normal view debug | `src/pbr.inl:165` | `[pbr] Debug=1` |
| Program locations | `src/pbr.inl:169-174` | Called from `AfterLink` (`src/tyrender.cpp:564`) |
| Per-program push | `src/pbr.inl:189-203` | Once per frame per program |
| Camera and sun | `src/pbr.inl:212-230` | `PbrUpdate` |
| Tangent-frame call site | `src/pbr.inl:84-95` | Derivatives |
| Texture-tag inputs | `src/tex.inl:224-239` | `tyTex` and `tyWater` per draw |
| Fragment rewrite order | `src/tyrender.cpp:388` | PBR first |
| Vertex rewrite order | `src/tyrender.cpp:379-383` | PBR VS, then tessellation |

## Settings

Code default (file:line) and shipped value from `src/default_tyrender.ini`. Full tables: [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[pbr] Enable` | 1 (`src/pbr.inl:18`) | 1 (`default_tyrender.ini:55`) | Restart (rewrite happens at shader compile; inferred from `src/tyrender.cpp:388`) | Master switch for the rewrite |
| `[pbr] LitProps` | 1 (`:18`) | not set | Restart (compile-time eligibility) | 0 leaves dynamically lit props stock |
| `[pbr] Debug` | 0 (`:18`) | not set | Restart (text inserted at compile, `:165`) | 1 shows the normal as colour |
| `[pbr] WaterStable` | 1 (`:20`) | 1 (`:59`) | Live via reload (tyWater uniform, inferred from `src/tex.inl:208-215`) | Water-tagged draws drop bump, relief, specular, sky Fresnel |
| `[pbr] SkyFresnelSkipWater` | 0 (`:20`) | 0 (`:63`) | Live via reload (same uniform) | Water-tagged draws drop sky Fresnel only |
| `[pbr] DebugWater` | 0 (`:23`) | 0 (`:61`) | Restart for bits 1, 2 and 8 (compile-time text edits, `:134`, `:139`, `:163`, `light.inl:46`); bit 4 is atmosphere | Bitmask to switch terms off for bisecting (issue #5) |
| `[pbr] NormalStrength` | 1.0 (`:27`) | not set | Live via reload (`tyPbr.x`, `:198`) | Bump strength |
| `[pbr] Specular` | 0.5 (`:27`) | not set | Live via reload (`tyPbr.y`) | Sun specular amount. 0 switches the highlight off |
| `[pbr] Roughness` | 0.65 (`:27`) | not set | Live via reload (`tyPbr.z`) | Base roughness |
| `[pbr] HeightScale` | 3.0 (`:27`) | not set | Live via reload (`tyPbr.w`) | Height scale of the relief |
| `[pbr] LowFreqBump` | 0.6 (`:28`) | not set | Live via reload (`tyPbr2.x`) | Share of low-frequency detail kept in the bump |
| `[pbr] Cavity` | 0.30 (`:28`) | not set | Live via reload (`tyPbr2.y`) | Valley darkening |
| `[pbr] ValleyRough` | 0.30 (`:28`) | not set | Live via reload (`tyPbr2.z`) | Extra roughness in valleys |
| `[pbr] Sheen` | 0.12 (`:28`) | not set | Live via reload (`tyPbr2.w`) | Grazing dust sheen |
| `[pbr] DetailStrength` | 1.0 (`:29`) | 1.0 (`:56`) | Live via reload (`tyPbr3.x`) | Relief strength. Name is misleading (see Discrepancies) |
| `[pbr] DetailScale` | 1.0 (`:29`) | 1.0 (`:57`) | No effect (read, not used) | See Discrepancies |
| `[pbr] DetailTexture` | 1.0 (`:29`) | not set | No effect (read, not used) | See Discrepancies |
| `[pbr] DetailDisp` | 0.8 (`:29`) | not set | No effect (read, not used) | See Discrepancies |
| `[pbr] FadeStart` | 1500 (`:30`) | not set | Restart (baked into the text at `:68`, inferred) | Distance where bump and specular begin to fade |
| `[pbr] FadeEnd` | 3500 (`:30`) | not set | Restart (as above) | Distance where they are gone |
| `[tex] SkinnedByTag` | 1 (`src/tex.inl:70`) | not set | Restart (`LoadTexTags` is not in the reload path, `src/tyrender.cpp:598`) | Lets tagged skinned programs get the rewrite |

Relief keys (`[tess] ReliefDepth`, `ReliefFadeStart`, `ReliefFadeEnd`, `ReliefSteps`, `GroundRelief`, `PropRelief`) feed `tyRel` and `tyRel2`. They are documented with the tessellation page: [tessellation-and-relief.md](tessellation-and-relief.md).

## Cost

- **Not measured per term** in the repo. There is no row for PBR alone in `docs/PERFORMANCE.md`.
- **Related measured row (v0.3 measurement, RX 7600 XT, 2560x1440):** "Relief (skin/detail) off" about -0.05 ms, with the note that "the detail layer rides on it; cost is tiny" (`docs/PERFORMANCE.md:52`). Source: `docs/PERFORMANCE.md:46` (v0.3 label).
- **What scales it:** the number of pixels that pass the distance and grazing fades, the parallax step count (at most 12, set by `tyRel2.z`), and the number of programs that take the rewrite.

## Limits

- The height is a guess from colour. The game has no normal maps, so flat or pale textures get little relief. The README says rope and grass get little benefit for this reason (`README.md`, Known issues, "Thin objects").
- Parallax is limited to 12 steps and is faded at grazing angles to avoid streaking (`src/pbr.inl:98`, `:104`).
- Screen derivatives are used for the tangent frame. On very small or skinned triangles these are noisy. That is why skinned programs get the rewrite only through the texture tags (`src/pbr.inl:40`, `src/tyrender.cpp:245`).
- The specular and the sky sheen are approximations. The sky tint is a fixed constant (`src/pbr.inl:158`), not the level's sky.
- The distance fade is compiled in (`src/pbr.inl:68`). Changing `FadeStart` or `FadeEnd` needs a restart (inferred).
- The rewrite is applied only when the engine text matches the anchors. Any engine variant without them is left stock.

## Known issues

- **#5 (water brightness ramp, blue light on Ty).** Playtest of 4a2ef95 reports the light on Ty ramps slowly in water instead of flickering (`CHANGELOG.md:36`; `README.md`, Known issues, Water). `WaterStable=1` is the fix attempt, listed as untested (`CHANGELOG.md:12`). The DebugWater bits are the bisect tool (`CHANGELOG.md:21`).
- **#62.** Linked to #5 for the blue flicker; the bisect bits are for this (`CHANGELOG.md:21`).
- **#4 and water names.** Lily pads and rocks look brighter than the water under them. Related to the water fog and names, not this rewrite (`CHANGELOG.md:26`; see [water-fog-and-decals.md](water-fog-and-decals.md)).
- **Characters.** Characters are kept stock by design. The rule in the code is the texture tag (`src/tex.inl:202-203`). See [tessellation-and-relief.md](tessellation-and-relief.md) for the reasons given in the code.

## How to verify

1. **Normal view.** Set `[pbr] Debug=1` and restart. Textured walls and ground should show smooth colour that follows the relief (`src/pbr.inl:165`).
2. **Off compare.** Set `[pbr] Enable=0` and restart. The surfaces should look flatter and the highlight should go. Compare the same frame with the A/B hotkey (see [ab-captures.md](../guides/ab-captures.md)).
3. **Specular off.** Set `[pbr] Specular=0`, then write `tyrender\reload` (see [hotkeys-and-files.md](../reference/hotkeys-and-files.md)). The sun highlight should disappear (inferred from `src/pbr.inl:146`).
4. **Relief strength.** Set `[pbr] DetailStrength=0` and reload. Relief should flatten, while the diffuse lighting stays (inferred from `src/pbr.inl:199`).
5. **Log lines.** `tyrender.log` contains `[pbr] prog <n> locs cam.. sun.. pbr.. pbr2.. pbr3..` for the first six programs (`src/pbr.inl:173`). `[cam] pos=` appears every 300 frames (`:229`). `[dbgwater] term OFF: ...` appears at start-up if `DebugWater` is set (`:25`).
6. **Water stability.** Set `[pbr] WaterStable=0` and reload. Water should show the frame-to-frame change described in #5 (inferred).

## NEEDS INVESTIGATION

- **P1. Behaviour without `glProgramUniform*`.** The push functions return early if the entry points are missing (`src/pbr.inl:195`, `src/light.inl:124`, `src/tex.inl:212`). The effect on the rewritten shader (uniforms left at zero) has not been checked. Method: run on a GL context that lacks these entry points (for example a forced compatibility run) and look at the frame. See also Q023 in the master plan.
- **P2. Fade distances compiled or live.** `FadeStart` and `FadeEnd` are written into the shader text (`src/pbr.inl:68`). Method: change them in-game, write the reload file, and see whether the falloff moves. If it does not, the restart note is correct.
- **P3. Use of the dead detail keys.** `DetailScale`, `DetailTexture` and `DetailDisp` are read (`src/pbr.inl:29`) and have no other use in the source (grep). Method: `grep -n "g_tsDetailScale\|g_tsDetailTex\|g_tsDetailDisp" src/*.inl src/tyrender.cpp` should show only the declaration and the read. Decision needed on removing or wiring them. Not a docs task.
- **P4. Vertex and fragment rewrites can disagree.** The fragment rewrite needs an extra anchor (`src/pbr.inl:60-61`) that the vertex rewrite does not check. If the vertex rewrite is skipped while the fragment rewrite runs, the `v_tyWorld` input would be undeclared at link time. No link check exists (`src/tyrender.cpp:333`). Method: search the dumps and the log for link failures. Whether it happens in practice is unknown.
- **P5. Visual effect of the lite path.** The lite variant's relight factor is clamped to 0.5 to 1.6 (`src/pbr.inl:135`). Its visual effect has not been checked in captures.

## Discrepancies

- `src/pbr.inl:4` describes the bump as coming from "screen-space derivatives of the albedo luminance". The code builds the height field from the multi-scale height function `kTyHeightGLSL` (`src/pbr.inl:70`, `src/tess.inl:9`), and takes the tangent frame from screen derivatives (`src/pbr.inl:84-95`). Code wins.
- `docs/SHADERS.md` (first pass, section 6) lists `WaterStable` as a "debug bitmask". It is a plain switch. Only `DebugWater` is a bitmask (`src/pbr.inl:21-23`).
- `docs/PERFORMANCE.md:84` says `[pbr]` uniforms are live. This is true for the uniform-driven keys. `[pbr] Enable`, `Debug`, `DebugWater` (bits 1, 2, 8), `LitProps`, and possibly `FadeStart` and `FadeEnd` act at compile time (`docs/PERFORMANCE.md:85`, which lists `[pbr] Enable` as restart, agrees).
- `[pbr] DetailStrength` is named as if it controls a detail layer. The code uses it only as the relief strength (`tyPbr3.x`, `src/pbr.inl:199`), and the header comment says the same (`src/pbr.inl:66`). The name is kept for compatibility with the shipped ini.
- The comment at `src/tex.inl:207` says `tyWater` is set only when `SkyFresnelSkipWater=1`. The code also sets it for the default `WaterStable=1` (`src/pbr.inl:20`, `src/tex.inl:209`). Code wins.

## See also

- [shadow-filter.md](shadow-filter.md): the shadow rewrite that runs after this one.
- [natural-light.md](natural-light.md): the light colour rewrite that uses `tyN` on lit programs.
- [tessellation-and-relief.md](tessellation-and-relief.md): the height field and the relief keys.
- [05-shader-source-rewriting.md](../how-it-works/05-shader-source-rewriting.md): rewrite order and compile checks.
- [07-texture-names-and-tags.md](../how-it-works/07-texture-names-and-tags.md): where `tyTex` and `tyWater` come from.
- [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md): full key tables.
