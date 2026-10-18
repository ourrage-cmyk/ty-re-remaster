# Material model

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B, C.

## Purpose

Each texture the game loads can carry a set of hand-made labels: whether it glows, whether it is see-through, and whether it belongs to a character. The mod turns those labels into one material class per texture, and each class has one row of allowed behaviour. This page explains the labels, the classes, the policy table, which code reads the table, and where the code differs from the planning document.

Background and the plan's phases: `docs/MATERIAL_CLASSIFICATION_PLAN.md`. This page reports what is in the code now.

## How it works

1. **Name.** The mod learns the file name of each texture when the game reads it (kernel32 file-read hooks plus the texture upload hooks, `src/tex.inl:2-4`). Names are lower-cased and stripped of their folder (`src/tex.inl:60-64`).
2. **Flags.** The name is looked up in an embedded table generated from the owner's list (`src/tex_tags.inc`, built by `src/gen_tex_tags.ps1`). Each entry holds a flag byte: known (1), emissive (2), transparent (4), character (8). The generator gives every DDS in the game archive a name entry, with flags 0 if it is not in the owner's list (`src/gen_tex_tags.ps1:11`, `:16`). The generated table has 2423 name entries (`src/tex_tags.inc`). Extra flag bits, decal (16) and water (32), are not in the list. Decal is set from an ini list of names; water is set when a name contains "water" or matches `[fog] WaterNames` (`src/tex.inl:12`, `src/tex.inl:190-191`).
3. **Override.** If `<game folder>\tyrender\texture_tags.csv` exists, its rows replace the embedded flags by name (`src/tex.inl:83-99`). The override takes the same five-column format as the owner's list.
4. **Class.** From the flags, each texture gets one class (`src/tex.inl:35`, `src/tex.inl:49-54`):
   - unknown (no known flag),
   - solid (tagged, nothing else),
   - transparent,
   - emitter (glows),
   - transparent emitter,
   - character,
   - emissive character (glows and is a character).
5. **Policy row.** The class indexes `kMatPolicy` (`src/tex.inl:38-48`). Each row has six columns: `giBits`, `noRelief`, `character`, `emissive`, `waterTag`, `seeThru`. Consumers ask the row, not the raw bits (`src/tex.inl:6`, `src/tex.inl:55-56`).
6. **Consumers.** Listed in the table below.

Precedence in the code matches the plan: emission follows the emissive flag alone, character only removes relief and GI colour bounce, and transparent only adds see-through behaviour (`src/tex.inl:33-34`).

### The owner's list (data)

`data/texture_tags.csv` has the header `path, name, emissive, transparent, character`, one row per texture, values `yes` or `no` (line 1 of the file). Counts from the committed file, checked with `awk` at this commit:

| Flag | yes | no |
|---|---|---|
| emissive | 222 | 1711 |
| transparent | 199 | 1734 |
| character | 258 | 1675 |

Overlaps: 52 textures are emissive and transparent, 2 are emissive and character, and none are transparent and character. Total data rows: 1933. These match `docs/MATERIAL_CLASSIFICATION_PLAN.md:11-17`.

The owner's note that the list is imperfect, and that the renderer must not depend on it, is in `docs/MATERIAL_CLASSIFICATION_PLAN.md:7`.

### Consumers of the policy table (Q157)

| Consumer | Column used | What it does | Source |
|---|---|---|---|
| GI source mask | `giBits` | Draws whose class has a bit set are not colour sources for GI. Bit 1 emissive, 2 transparent, 4 character | `src/gi2.inl:41`, `src/tex.inl:36`, `src/tex.inl:40-47` |
| Fog, see-through depth | `seeThru` | Gates the depth write for see-through draws and the water and foliage fog boosts | `src/atmos.inl:321`, `src/atmos.inl:339` |
| Fog row | `seeThru`, water flag | Chooses between the water boost and the foliage boost | `src/atmos.inl:332`, `src/atmos.inl:351`, `src/atmos.inl:339` |
| Emitter gate | `emissive` | Only emissive draws are considered for emitter light | `src/emit.inl:140` |
| Sky Fresnel skip for water | `waterTag` | Sets the `tyWater` uniform, used only when `[pbr] SkyFresnelSkipWater=1` | `src/tex.inl:208-215`, `src/tex.inl:16`, `default_tyrender.ini:63` |
| Relief per draw | `noRelief`, class | Sets the `tyTex` uniform: 1 means no bump, relief or specular for that draw | `src/tex.inl:230-231`, `src/pbr.inl:76` |
| Tessellation of characters | skinned program | Skinned programs are never tessellated; the flag is also read by the evaluation shader | `src/tess.inl:107`, `src/tess.inl:313`, `src/tess.inl:348` |
| Decal clamp | decal bit | Caps luminance of named decals when `[decal] Clamp=1` (default 0) | `src/tex.inl:216-223`, `src/tex.inl:227`, `src/tex.inl:21` |
| Counters and logs | class | Counts draws by character, known and unknown class, and logs the spill draws | `src/tex.inl:232-233`, `src/tex.inl:262-269`, `src/tex.inl:266` |

Not yet consumers: AO occluder, shadow caster and TAA reject columns from the plan (`docs/MATERIAL_CLASSIFICATION_PLAN.md:70-72`). The code comment says those columns are added with their phases (`src/tex.inl:37`). No AO, shadow or TAA code calls `MatPolicy` (search of the repo).

### Relief rules for unknown textures

The rule depends on whether the program is skinned (a character model) or world geometry (`src/tex.inl:202-203`, `src/tex.inl:229-231`):

- Skinned program. Unknown texture: no relief (`tyTex` = 1). Known, non-character: relief on.
- World program. Unknown texture: relief on (`tyTex` = 0). Character: no relief.

This means an unknown world texture **does get relief**. The planning document says unknown textures get stock behaviour (`docs/MATERIAL_CLASSIFICATION_PLAN.md:55`, `:108`). See Discrepancies.

## Settings

| Key | Default in code | Shipped (`src/default_tyrender.ini`) | Live or restart | Effect | Source |
|---|---|---|---|---|---|
| `[tex] Enable` | 1 | not set | restart (LoadTexTags runs at DLL start only, `src/tyrender.cpp:725`; the reload path does not call it, `src/tyrender.cpp:597-603`) | 0 makes every texture unknown | `src/tex.inl:67` |
| `[tex] Debug` | 0 | not set | restart | Extra debug output | `src/tex.inl:67` |
| `[tex] LogUnknown` | 0 | not set | restart | Logs draws per untagged texture, every 600 frames | `src/tex.inl:68`, `src/tex.inl:232` |
| `[tex] SkinnedByTag` | 1 | not set | restart | Skinned shaders get the detail code, switched per draw by tags | `src/tex.inl:70`, `src/pbr.inl:40` |
| `[decal] Clamp` | 0 | 0 | restart (same as `[tex]`; read in `LoadTexTags`) | Caps decal luminance | `src/tex.inl:72`, `default_tyrender.ini:69` |
| `[decal] Max` | 0.9 | 0.9 | restart | Cap value | `src/tex.inl:73`, `default_tyrender.ini:70` |
| `[decal] Names` | `ty_footprint.dds,ty_footprint2.dds` | same | live | File names counted as decals | `src/tex.inl:74`, `default_tyrender.ini:71` |
| `[fog] WaterNames` | empty | empty | restart (read in `LoadTexTags`) | Extra name substrings counted as water | `src/tex.inl:77-78`, `default_tyrender.ini:89` |
| `[spill] LogEmitters` | 0 | 0 | restart | Logs additive draws per texture | `src/tex.inl:69`, `default_tyrender.ini:66` |

Keys in the plan that are not in the code: a `[material]` section with `Enable`, `TagFile` and `OverrideFile`; a `[glow]` section; `[emit] TransparentWeight` (`docs/MATERIAL_CLASSIFICATION_PLAN.md:112-128`). The plan also proposes `data/texture_tags_override.csv`. The code uses `tyrender\texture_tags.csv` instead (`src/tex.inl:83`).

## Cost

Not measured for the material lookup on its own. The lookup is an array read per texture id (`src/tex.inl:27-28`, `src/tex.inl:55`), so the cost is expected to be negligible (inferred). Per-pass costs are on the shader pages.

## Limits

- Labels are the owner's hand list. A mislabelled texture gets the wrong behaviour everywhere it is used.
- Unknown textures are not stock: world ones get relief (see above).
- Names are the only key. Two textures with the same file name in different folders share one label.
- The mapping depends on the game reading files by name. Textures loaded in ways the file-read hooks do not see stay unknown (`docs/START_HERE.md:81` and the research note on unknown names, `[tex] LogUnknown`).
- The override file is per install, not per texture id.

## Known issues

- Footprints glow (#8). The flags are not the cause; the decal shading path is (`docs/MATERIAL_CLASSIFICATION_PLAN.md:39`, `:95-101`). The footprint texture that draws on the ground was not identified in the plan.
- The portal light does not reach the ground (#9). Emitters are excluded from GI as colour sources, and nothing injects their light (`docs/MATERIAL_CLASSIFICATION_PLAN.md:36`). `[emit] Enable=0` is the shipped default (`default_tyrender.ini:107`).
- The bag on the character is not tessellated while other items are (`docs/MATERIAL_CLASSIFICATION_PLAN.md:35`, playtest finding R4-5). Cause not traced to a tag.
- Water flicker on Ty (R4-2) is only partly fixed (`docs/MATERIAL_CLASSIFICATION_PLAN.md:38`).
- Seven prop-like entries listed in the plan are not yet confirmed as character or solid by the owner (`docs/MATERIAL_CLASSIFICATION_PLAN.md:21`, `:147`).

## How to verify

1. Set `[tex] LogUnknown=1` in `tyrender.ini` and restart. Read the `[tex] stats` lines in `tyrender.log` (`src/tex.inl:269`). They show the share of character, known and unknown draws.
2. Put a row in `<game folder>\tyrender\texture_tags.csv` for one texture, restart, and watch the effect on that surface. Compare with the `[pbr] Debug` view, see [guides/debugging.md](../guides/debugging.md).
3. Set `[fog] WaterNames` to a substring of a texture name and check the fog on that draw.

## NEEDS INVESTIGATION

- The owner's confirmation of the remaining prop-like entries (`docs/MATERIAL_CLASSIFICATION_PLAN.md:147`). Method: ask the owner.
- The name of the footprint texture that draws on the ground. Method: enable the name log proposed in `docs/MATERIAL_CLASSIFICATION_PLAN.md:99`, which is not in the code yet; or read the `[tex]` debug output for draws near the character.
- Whether a texture's file name can repeat across folders in the game archive. Method: list `Data_PC.rkv` with `tools/rkvtool.ps1`.
- Count of textures the file-read hooks never name (`[tex] LogUnknown`). Method: run a level and read the counters (N-27).

## Discrepancies

- **Unknown textures.** The plan says unknown textures get stock behaviour (`docs/MATERIAL_CLASSIFICATION_PLAN.md:55`, `:108`). The code gives unknown textures relief on world geometry (`src/tex.inl:231`, `src/tex.inl:202-203`). The code comment at `src/tex.inl:202` describes the same rule. The plan is not updated.
- **Override file.** The plan proposes `data/texture_tags_override.csv` and a `[material]` section (`docs/MATERIAL_CLASSIFICATION_PLAN.md:107`, `:115-118`). The code reads `<game folder>\tyrender\texture_tags.csv` (`src/tex.inl:83`) and has no `[material]` section.
- **Phase status.** The plan's phase 1 says "behaviour unchanged" (`docs/MATERIAL_CLASSIFICATION_PLAN.md:137`). The code has the classifier and the policy table (`src/tex.inl:32-58`) and calls it phase 1 (`src/tex.inl:7`). The later columns are not in the code.
- **Generator paths.** `src/gen_tex_tags.ps1:4`, `:10`, `:21` use fixed owner locations, and the plan notes the same (`docs/MATERIAL_CLASSIFICATION_PLAN.md:29`). The plan asks that the repo copy read paths from the repo. This is still open in the code.
- **Water on transparent rows.** The policy gives transparent textures `waterTag=1` (`src/tex.inl:43`), so every transparent texture counts as water for the sky Fresnel skip. The name-based water flag (`src/tex.inl:190`) is a separate path for fog. The plan describes the water row as the emissive-or-transparent case, which matches (`docs/MATERIAL_CLASSIFICATION_PLAN.md:38`). Not a contradiction, but the two water rules are not the same thing.

## See also

- [what-is-this.md](what-is-this.md)
- [constraints-and-limits.md](constraints-and-limits.md)
- [../how-it-works/07-texture-names-and-tags.md](../how-it-works/07-texture-names-and-tags.md)
- [../how-it-works/04-frame-anatomy-and-pass-order.md](../how-it-works/04-frame-anatomy-and-pass-order.md)
- [../shaders/pbr-surface-detail.md](../shaders/pbr-surface-detail.md)
- [../reference/data-files.md](../reference/data-files.md)
