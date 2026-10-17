# Material classification: one tag set drives glow, light emission, AO, shadows, TAA and tessellation

Status: plan only (2026-10-17). No code or game changes in this PR. First users: #8 (footprints glow) and #9 (portal light does not reach the ground). Owner: Yannic.

## 1. The list we already have

The owner's hand-labelled list is `data/texture_tags.csv` (1933 texture files). Columns: `path, name, emissive, transparent, character`. The owner's stated four categories are: not emissive, transparent, emissive, and character. Playtest 4 (`docs/recording/voice3.txt`, `voice4.txt`) says the list is imperfect and the renderer must still behave well when a texture is mislabelled.

Counts from the file as committed:

| Flag | yes | no |
|---|---|---|
| emissive | 222 | 1711 |
| transparent | 199 | 1734 |
| character | 258 | 1675 |

Overlaps: 52 textures are emissive and transparent (lava, portal, glow sprites, fire). Two are emissive and character (`act_94_fluffyburn.dds`, `act1_26_jellyfish.dds`). None are transparent and character.

Entries to re-check with the owner, because the names suggest props, not characters: `act1_12_speedboat.dds`, `landrover.dds`, `act1_18_speargun.dds`, `act_76_binoculars.dds`, `act1_37_birdflock.dds`, `cattails.dds`, `mushroom_01.dds`, `act_01_ty_01shadow.dds`. The owner's wording was "which vectors are characters". This plan reads that as textures, since the CSV is per texture file. Meshes that use these textures inherit the tag.

Stale number: `docs/FIX_PLAN_20261016.md` says "206 of 342 tagged". The CSV has 1933 rows, so that figure predates the full list and should be corrected when the FIX_PLAN is next edited.

Tag file lookup already exists in three places that must agree: `src/tex.inl` (runtime override by name from `tyrender\texture_tags.csv`), `src/tex_tags.inc` (generated, embedded), and `src/gen_tex_tags.ps1` (generator, currently hard-coded to `C:\CLAUDE\RESEARCH\...` paths). The repo copy of the generator must read paths relative to the repo.

## 2. What the renderer does with the flags today

| Consumer | Uses | Current behaviour | Gap seen in playtests |
|---|---|---|---|
| `src/tex.inl` `TexDrawPrep` | character | skinned programs get tessellation off; world programs get it on | Owner reports the bag is not tessellated while other items are (R4-5); cause not yet traced to a tag |
| `src/gi2.inl` `GIMaskBegin` | emissive, transparent, character | `ExcludeSources` bits 1/2/4 (default 7) write alpha 0 so the draw is not a GI colour source | Emissive draws are removed as *sources* but nothing *emits* their light, so the portal lights the character and not the ground (#9) |
| `src/atmos.inl` | transparent, character | fog ignores non-known and character draws | Fog hazes emitters inconsistently (fire, Billabong trees) |
| `src/tex.inl` `TexSetWater` | emissive or transparent | sky Fresnel skip for water when `[pbr] SkyFresnelSkipWater=1` | Water flicker on Ty (R4-2) is only partly fixed |
| `src/gi2.inl` / `src/pbr.inl` | none | decals are shaded as unlit when near-white | Footprints glow (#8). The flags are not the cause; the decal shading path is |

The current design answers "is this a colour source?" in four places. It does not answer "is this a light emitter?" at all. That is the core gap.

## 3. Proposed model: one class per texture, one policy table

Resolve every texture to one class at bind time (cached per texture id, as `TexFlags` already is):

| Class | Tag rule | Meaning |
|---|---|---|
| Character | character = yes | Ty, NPCs, animals. Wins over emissive |
| Emitter | emissive = yes, character = no | Light source. Glows and lights the scene |
| Transparent emitter | emissive = yes, transparent = yes, character = no | As Emitter, and its light passes through fog and water |
| Transparent | transparent = yes only | See-through, not a colour or light source |
| Solid | none of the above, tagged | Stock behaviour |
| Unknown | not in the list | Stock behaviour, counted in debug (see section 6) |

Precedence rule (written down so every consumer agrees): character beats emissive beats transparent.

Policy table (what each class is allowed to do):

| Effect | Character | Emitter | Transparent emitter | Transparent | Solid | Unknown |
|---|---|---|---|---|---|---|
| Tessellation / detail layer | off | on | on | on | on | stock |
| Bloom / glow | off | on, `[glow] Strength` | on, alpha-weighted | off | off | off |
| Light emission into GI | off | on, `[emit] Strength` | on, weighted by alpha | off | off | off |
| GI colour source (bounce) | off | off (see below) | off | off | on | on |
| Receives GI / light | on | on | on | on | on | on |
| AO occluder | off | on | on, alpha-weighted | on | on | on |
| Shadow caster | on (as stock) | on | on, alpha-tested | on | on | on |
| TAA history reject | tighter clamp | stock | stock | on for grass/foliage | stock | stock |

Notes on the table:
- "GI colour source off" for emitters is deliberate. The emitter's own colour should not bounce, but its light should be injected. That is the change for #9.
- Character AO off is already the plan in `docs/FIX_PLAN_20261016.md` (T73). It becomes a row, not a special case.
- TAA row for transparent is a hypothesis for grass ghosting; it needs a playtest before it is relied on.

## 4. Light emission from emitters (first user: #9)

Today emitters are excluded from GI as colour sources, and nothing else adds their light. Plan:

1. Emitter injection pass: for each emitter draw, write its screen-space emission (colour × `[emit] Strength` × alpha) into a half-resolution additive buffer that the GI/light pass reads as direct light. This is the "half-res additive spill" design in `docs/agent-tasks/light*.md`.
2. Radius and falloff from `[emit] Radius` (world units) so a portal lights the ground around it, not the whole screen.
3. Emitter light is additive and must not be tinted by the emitter's own albedo (that is what caused the coloured spill complaints earlier).
4. Characters receive emitter light (the owner says the portal already lights Ty correctly); ground must now receive it too.

Coordination: the footprints/portal session (`session_011gkqH9XCkfw5zo4FVc5Rcr`, branch `claude/project-thread-jaj4q8`) is already building a short-term fix for #8 and #9. This plan defines the general mechanism. Their tag-based clamp for #8 and any portal fix should go through the same class lookup, so it is replaced rather than duplicated when the general pass lands.

## 5. Footprints (first user: #8)

The tag list is not the cause. `loading_footprint.dds` is tagged transparent only, and the footprint texture that actually draws on the ground is not yet identified in the CSV. Steps:

1. Identify the bound texture name for draws near Ty's feet (debug counter already exists: `g_texUntagCnt`, per texture id; add a name log when `TexDrawPrep` sees Ty's position within a radius).
2. Only then decide: a tag-based clamp (symptom fix, what the other session is doing) or a fix in the decal path (the decal is shaded as unlit, so near-white reads as emissive).
3. Acceptance: footprints shade like the ground at the same light level in an A/B pair, on Medium first.

## 6. Robust to imperfect labels

The owner expects mislabels, so the renderer must not depend on the list being right.

- Override file: `data/texture_tags_override.csv`, same columns, applied after the main list by both the generator and `tex.inl`. Owner can fix a label without regenerating the embedded table.
- Unknown textures get stock behaviour (section 3), never the emitter behaviour. Unknown is the safe default.
- Debug counters per class and a list of the most-drawn unknown texture names, written to the log at shutdown. Owner's playtest reports can then name the texture rather than the effect.
- Validation script (`tools/check_tags.py`, to write): lists duplicates, conflicting rows, names missing from the rkv index, and class counts. Runs in CI or before a build.

## 7. Ini keys (proposed; defaults keep current behaviour)

```ini
[material]
Enable=1                 ; 0 = all textures Unknown (stock)
TagFile=data\texture_tags.csv
OverrideFile=data\texture_tags_override.csv

[emit]
Enable=0                 ; phase 2 turns it on
Strength=1.0
Radius=6.0               ; world units
TransparentWeight=1.0

[glow]
Strength=0.0             ; phase 2
```

Existing keys are kept and become views of the policy table: `[gi] ExcludeSources` (default 7) maps to the GI column; `[tess] Scope` (default `env-detail`) maps to the tessellation column; `[pbr] SkyFresnelSkipWater` maps to the water row.

## 8. Phases

| Phase | Work | Gate |
|---|---|---|
| 0. Verify list | Owner confirms the 8 prop-like character entries and the 2 emissive+character entries. Identify footprint, water and portal texture names via debug log | Owner sign-off on the list; names recorded in this doc |
| 1. One classifier | Replace the separate bit tests in `tex.inl`, `gi2.inl`, `atmos.inl`, `pbr.inl` with one `MatPolicy(tex)` lookup. Behaviour unchanged | A/B screenshots identical on Medium; GPU timer unchanged |
| 2. Emitter injection (#9) | Section 4, behind `[emit] Enable` | Portal lights ground; no change to Ty's light; Medium under 12 ms |
| 3. Footprints (#8) | Section 5 | Footprints match ground in A/B |
| 4. Character and AO | T73 AO fade and character-off-AO as policy rows | No AO halo behind Ty; no AO flicker on far mountains |
| 5. Transparent and TAA | TAA reject row for foliage; water flicker check (R4-2) | Playtest on Medium, then High |

No game launches are part of this plan. Each gate is run by the owner in a playtest, or by the existing A/B and GPU-timer tools when the owner allows a run.

## 9. Open questions for the owner

1. Are the 8 prop-like entries in section 1 character textures, or should they be Solid?
2. For the two emissive+character textures (fluffy burn, jellyfish): should they glow or not? The plan currently says character wins, so they do not glow.
3. Should the portal light radius be fixed in world units (`[emit] Radius`) or a fraction of screen size?
4. Is the footprint texture one of the transparent list entries, or a texture not in the list at all?

## 10. Related

- `docs/FIX_PLAN_20261016.md` (T73, T75, T77)
- `docs/PLAN_CONSOLIDATED.md` (#8, #9)
- `docs/recording/FINDINGS_ROUND4.md` (R4-2, R4-5, R4-7)
- `docs/agent-tasks/light*.md` (emitter spill design)
