# Texture names and tags

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who relabels textures).

## Purpose

The renderer decides many things per draw from the texture the draw uses: whether it is a character, whether it glows, whether it is see-through, whether it is water. The game does not give its textures a material type. The mod gets a file name for each GL texture by watching how the game reads its files, and then looks the name up in a hand-made list.

This page covers four things:

1. how a GL texture gets a file name;
2. where the list (the tags) comes from and how it is built into the DLL;
3. how a draw's texture becomes a material class and a shader input;
4. how to relabel a texture, and what the limits are.

Each consumer of the classes has its own page. The list of consumers is in [material-model.md](../concepts/material-model.md).

## How it works

### Part 1. Getting a name for a GL texture

1. **File handles.** The kernel32 imports `CreateFileW`, `CloseHandle`, `SetFilePointerEx` and `ReadFile` are patched in the game's import table by `TexInstallHooks` (`src/tex.inl:166-171`), which `Install` calls (`src/tyrender.cpp:670`). `CreateFileW` records a handle only if the file is opened for reading and its base name is `data_pc.rkv` (kind 1) or ends in `.dds` (kind 2) (`src/tex.inl:116-127`, kind rule at `:121-122`).
2. **Read buffers.** Each `ReadFile` on a recorded handle adds an entry to a ring of 8192 entries (`src/tex.inl:108`, `:140`, `:142-165`):
   - a loose `.dds` file: the whole read buffer is recorded with that file's base name (`:153`);
   - `Data_PC.rkv`: the byte range of the read is matched against the table of DDS entries (`kRkvDds`, sorted by offset). Each DDS that overlaps the range gets its piece of the buffer recorded with its name (`:154-161`). The search is a binary search by offset (`:156-157`).
   - `SetFilePointerEx` keeps the file position up to date for reads that do not use an explicit offset (`:132-139`).
3. **Upload.** When the game uploads texture data, the buffer pointer is looked up in the ring. The newest entry whose range contains the pointer gives the name (`src/tex.inl:174-180`). Two upload hooks call this: `glCompressedTexImage2D` (`src/tex.inl:197-200`) and `glTexImage2D` (`src/tyrender.cpp:278`). Only mip level 0 is named (`src/tex.inl:175`).
4. **Flags.** The base name is looked up in the tag table (`src/tex.inl:187`). Names are already lower-case when recorded (`src/tex.inl:60-64`, `:121`). The DECAL and WATER name rules are added (`:188-191`, see Part 3). The result is stored per GL texture id, both as flags and as a class (`:192`).

### Part 2. Where the tags come from

1. **The list.** `data/texture_tags.csv` has one header line and 1933 rows. The columns are `path`, `name`, `emissive`, `transparent`, `character`. Each of the last three is `yes` or `no`. Counts in the shipped file: emissive 222, transparent 199, character 258. There are 52 rows that are both emissive and transparent, and 2 rows that are emissive and character. The `path` column holds file names with no folder part in the shipped file (checked with a text scan). The names are unique (1933 unique names). `docs/SHADERS.md` section 9 gives the same counts.
2. **The build.** `src/gen_tex_tags.ps1` reads the CSV and an index of the game's `Data_PC.rkv` (both from paths outside the repo, `src/gen_tex_tags.ps1:4`, `:10`). It writes `tex_tags.inc` to a hard-coded owner path (`:21`), not to this repo's `src/`. The repo copy is `src/tex_tags.inc`, and the two must be kept in step by hand (see Limits). That file holds:
   - `kTagNames`: 2423 entries, each a lower-case base name and a flag byte (`src/tex_tags.inc:3`, end at `:2427`);
   - `kRkvDds`: 2423 entries, each an offset, a size and an index into `kTagNames` (`src/tex_tags.inc:2428`, end at `:4852`).
3. **Flag bits.** The generator writes `1 + 2 (emissive) + 4 (transparent) + 8 (character)` (`src/gen_tex_tags.ps1:7`). The runtime enum uses the same values, and adds `16` for decal and `32` for water, which exist only at run time (`src/tex.inl:12`).
4. **Names not in the CSV.** Every `.dds` in the game's RKV index that is not in the CSV gets flag 0 (`src/gen_tex_tags.ps1:11`). In the embedded table, 490 of the 2423 names have flag 0. The other flag values are: 1 (known) 1308, 3 (known, emissive) 168, 5 (known, transparent) 147, 7 (known, emissive, transparent) 52, 9 (known, character) 256, 11 (known, emissive, character) 2 (counted from `src/tex_tags.inc`).
5. **Runtime override.** At start-up, `tyrender\texture_tags.csv` in the game folder is read if present (`src/tex.inl:83-100`). It has the same format. Each row replaces the flags of the named texture only; other names keep the embedded flags (`:98`). The first line is skipped if it starts with `path` (`:93`). Rows with fewer than five fields are ignored (`:93`). The values are compared with lower-case `yes` (`:95-97`).
6. **Load time.** `LoadTexTags` runs once at start-up (`src/tyrender.cpp:725`). It is not in the reload path (`src/tyrender.cpp:598`).

### Part 3. From a draw to a class and a shader input

1. **Classes.** Flags are turned into one class per texture (`src/tex.inl:35`, `:49-54`). Priority is:
   - `UNKNOWN` (no known flag), `SOLID`, `TRANSPARENT`, `EMITTER`, `TRANS_EMITTER`, `CHARACTER`, `EMIT_CHARACTER`.
   - A character flag decides the class first. Emission follows the emissive flag alone (`src/tex.inl:33-34`).
2. **Policy table.** Each class has one row in `kMatPolicy` (`src/tex.inl:38-48`). The columns are `giBits`, `noRelief`, `character`, `emissive`, `waterTag` and `seeThru`. Consumers ask the row, not the raw bits (`src/tex.inl:6`).
3. **Per draw.** `TexDrawPrep` runs before each `glDrawElements` and `glDrawArrays` (`src/tyrender.cpp:267`, `:270`). It takes the albedo texture of the current program (`src/tex.inl:226`) and sets three uniforms:
   - `tyTex` (`src/tex.inl:228-238`). The value is 1 when the row says `noRelief` (characters). On skinned programs (those with `boneMats`, `src/tyrender.cpp:338`, `src/tex.inl:229`) it is 1 for `UNKNOWN` textures. On world programs it is 0. The uniform is only sent when the value changes (`:235-238`).
   - `tyWater` (`src/tex.inl:208-215`). Set from the row's `waterTag` (see Part 4).
   - `tyDecal` (`src/tex.inl:216-223`), only when decal clamping is on.
4. **Draw classification questions** (the chapter's questions):
   - *Characters (Q049).* Skinned programs are found by the uniform `boneMats` (`src/tyrender.cpp:338`). A texture is a character when its flag has `character` (`src/tex.inl:51`). The `[tex] SkinnedByTag` key lets tagged skinned programs get the shader rewrite (`src/tex.inl:70`, `src/pbr.inl:40`). The shadow patch disables its receiver bias on skinned programs (`src/tyrender.cpp:245`). The light rewrite skips them with `SkipCharacters` (`src/light.inl:45`).
   - *Sky (Q050).* Not a texture rule. A pixel is sky when its scene depth is at or beyond `0.99999` (`src/atmos.inl:79`; the same test is in `src/ao.inl:132`). Painted sky and rock are handled by the atmosphere pass (`src/atmos.inl:104-107`) and the `[atmos] SkyFogDist` key.
   - *Water (Q051).* Two rules, used by different consumers. See Part 4.
   - *Foliage (Q052).* Not a texture tag. Grass is detected from the shader text (`src/light.inl:37`) and from the grass vertex template (`src/tyrender.cpp:376`). Tree cards are see-through through the `transparent` column (`src/tex.inl:43`, `seeThru`), which lets fog treat them as see-through (`src/atmos.inl:339`).
   - *UI and HUD (Q053).* No per-draw HUD tag exists in the code. The post pass runs at the first 64x64 viewport after the scene depth copy, which is the point where the 3D image is complete and the HUD is not yet drawn (`src/tyrender.cpp:253-254`). Draws after that point are not classified as HUD (inferred).
   - *Emitters (Q054).* An emitter is a draw whose texture class has the `emissive` column (`src/emit.inl:140`) in a program that has a `tyEmit` uniform (`src/emit.inl:177`). Emitter ranking and caps are on [emitter-light.md](../shaders/emitter-light.md).
5. **Other consumers.** The class also drives:
   - GI source mask: `giBits` (`src/gi2.inl:41`);
   - see-through and fog: `seeThru` (`src/atmos.inl:321`, `:332`);
   - the additive-draw log: `[spill]` (`src/tex.inl:242-246`, `:266`);
   - decal footprints: the `DECAL` bit, from `[decal] Names` (`src/tex.inl:188`, `:248-257`).

```
GL texture id  <-  upload (glTexImage2D / glCompressedTexImage2D, level 0)
                     <-  ring lookup of the buffer pointer
                           <-  ReadFile of tyrender-tracked .dds or Data_PC.rkv
                                 name -> tag table -> flags -> class -> kMatPolicy row

draw  -> albedo texture -> class -> tyTex, tyWater, tyDecal (uniforms)
```

### Part 4. The two water rules

- **Name rule (fog).** A file name that contains `water`, or a substring listed in `[fog] WaterNames`, gets the WATER bit (`src/tex.inl:190-191`). It is used by the atmosphere pass for `[fog] WaterBoost` (`src/atmos.inl:339`).
- **Class rule (PBR).** `tyWater` comes from the row's `waterTag` column, which is set for the TRANSPARENT, EMITTER and TRANS_EMITTER classes only (`src/tex.inl:41-47`). It is used by the water-stability switch (`src/tex.inl:208-215`, `src/pbr.inl:20`).

So a texture named `*water*` that is tagged opaque gets the fog boost but not the PBR water stability (inferred from the table and the two uses).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Patch of kernel32 imports | `src/tex.inl:166-171` | Four imports |
| Handle kind rule | `src/tex.inl:116-127` | `data_pc.rkv` and `.dds` |
| Read hook | `src/tex.inl:142-165` | Ring entries from loose and RKV reads |
| RKV range match | `src/tex.inl:154-161` | Binary search on offset |
| Ring | `src/tex.inl:108`, `:140` | 8192 entries |
| Upload lookup | `src/tex.inl:174-196` | Level 0 only |
| `glCompressedTexImage2D` hook | `src/tex.inl:197-200` | Compressed uploads |
| `glTexImage2D` upload | `src/tyrender.cpp:278` | Uncompressed uploads |
| Name to flags | `src/tex.inl:187-192` | Plus DECAL and WATER rules |
| Flag enum | `src/tex.inl:12` | Values 1, 2, 4, 8, 16, 32 |
| Classes | `src/tex.inl:35`, `:49-54` | One per texture |
| Policy table | `src/tex.inl:38-48` | Columns and rows |
| Policy lookup | `src/tex.inl:55-58` | `MatClass`, `MatPolicy`, `MatDecal` |
| Config and override load | `src/tex.inl:65-102` | `[tex]`, `[decal]`, `[fog] WaterNames`, override CSV |
| Per-draw prep | `src/tex.inl:224-239` | `tyTex` |
| Water uniform | `src/tex.inl:208-215` | `tyWater` |
| Decal rewrite | `src/tex.inl:248-257` | Only when `[decal] Clamp=1` |
| Spill log | `src/tex.inl:242-246`, `:262-280` | `[spill]`, `[tex] stats` |
| Generator | `src/gen_tex_tags.ps1:1-22` | Paths outside the repo; output path differs from `src/` (see Limits) |
| Embedded table | `src/tex_tags.inc:3-2427`, `:2428-4852` | Generated |
| Shipped CSV | `data/texture_tags.csv:1` | Header and 1933 rows |
| Draw calls | `src/tyrender.cpp:267`, `:270` | `TexDrawPrep` before each draw |
| Load call | `src/tyrender.cpp:725` | Start-up only |

## Settings

Code default (file:line) and shipped value from `src/default_tyrender.ini`. Full tables: [settings-system-debug.md](../reference/settings-system-debug.md) and [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[tex] Enable` | 1 (`src/tex.inl:67`) | not set | Restart (`LoadTexTags` not in reload path, `src/tyrender.cpp:598`) | Turns the whole tag system off. Everything is then unknown |
| `[tex] Debug` | 0 (`src/tex.inl:67`) | not set | Restart | Per-upload and per-miss log lines |
| `[tex] LogUnknown` | 0 (`src/tex.inl:68`) | not set | Restart | Logs the untagged draws by name, every 600 frames |
| `[tex] SkinnedByTag` | 1 (`src/tex.inl:70`) | not set | Restart | Tagged skinned programs get the shader rewrite |
| `[spill] LogEmitters` | 0 (`src/tex.inl:69`) | 0 (`default_tyrender.ini:66`) | Restart | Logs additive draws per texture |
| `[decal] Clamp` | 0 (`src/tex.inl:72`) | 0 (`default_tyrender.ini:69`) | Restart | Caps decal luminance (rewrite at compile) |
| `[decal] Max` | 0.9 (`src/tex.inl:73`) | 0.9 (`default_tyrender.ini:70`) | Restart | The cap |
| `[decal] Names` | `ty_footprint.dds,ty_footprint2.dds` (`src/tex.inl:74`) | same (`default_tyrender.ini:71`) | Restart | Names that count as decals |
| `[fog] WaterNames` | empty (`src/tex.inl:77`) | empty (`default_tyrender.ini:89`) | Restart | Extra substrings that count as water (fog only) |

Files: `tyrender\texture_tags.csv` is read at start-up only (`src/tex.inl:83`).

## Cost

- **Measured:** "The texture flags (character, glow, see-through) cost nothing measurable" (`docs/PERFORMANCE.md:29`). The sentence is in the v0.3.3 test-build note (`docs/PERFORMANCE.md:21`). It was not re-measured for later builds.
- **Not measured:** the upload-time lookup (a backwards scan of up to 8192 ring entries per upload, `src/tex.inl:180`) and the per-draw uniform checks.
- **Historical snapshot (not current):** `docs/RESUME.md:14` reports 279 of 342 uploads named and 206 tagged, from an earlier session. Its build is not stated on that line. Do not quote it as current.

## Limits

- **Name only.** Identity is the base name, lower-cased (`src/tex.inl:60-64`). Two textures with the same base name in different folders get the same tag (inferred).
- **No name, no tag.** A texture that does not pass through the hooked read calls, or whose buffer has been overwritten in the ring, gets no name and class `UNKNOWN` (`src/tex.inl:180`, `:187`). The ring holds 8192 reads (`:108`).
- **Level 0 only.** Lower mip levels are not named or tagged by this code (`src/tex.inl:175`).
- **Unknown is a special case.** On world programs an unknown texture gets relief (`tyTex` 0). On skinned programs an unknown texture gets no relief (`tyTex` 1, `src/tex.inl:231`).
- **Tags are opinions.** The `yes` and `no` values are the owner's labels in the CSV. The code trusts them.
- **Override is per name, and needs a restart.** A row in `tyrender\texture_tags.csv` replaces flags by name only (`src/tex.inl:98`). It is read at start-up only.
- **Generator paths are the owner's.** `src/gen_tex_tags.ps1` reads two input paths from the owner's workstation (`:4`, `:10`) and writes to a third path there (`:21`). In this page they are written as `<owner folder>\...`. The generator's output path is not `src/` in this repo. Copy the output by hand or change the path (see N-13 in the master plan). The generator reads the CSV with `-eq 'yes'` (`:7`). PowerShell comparison is case-insensitive, while the runtime comparison (`src/tex.inl:95-97`) is exact. A `YES` in the CSV would be tagged by the generator and not by the runtime override.
- **Source files outside the repo.** The generator header refers to `<owner folder>\RESEARCH\recording\texture_tags.csv` (`src/gen_tex_tags.ps1:1`). That file is not in the repo. The repo copy is `data/texture_tags.csv`.

## Known issues

- **Textures not emissive.** The playtest of 4a2ef95 lists "textures not emissive (known)" (`CHANGELOG.md:36`). Emission depends on the tag, so a wrong tag shows up as a missing glow.
- **Emitter light (#9, #61).** The emitter key is chosen from `[spill]` data (`src/tex.inl:240-241`). The emitter pass is off by default. The Known list in `CHANGELOG.md` says `[emit] Enable=1` renders a flat white band. See [emitter-light.md](../shaders/emitter-light.md).
- **Footprints (#8).** `[decal] Clamp` is off by default (`default_tyrender.ini:67-69`, where the comment calls 0 "unchanged (default, safe)").
- **Lily pads (#4, #106).** `[fog] WaterNames` is empty. The names are still to be confirmed from a playtest texture log (`default_tyrender.ini:88`; `CHANGELOG.md:26`).

## How to verify

1. **Startup count.** The log line `[tex] tags: <n> embedded, <m> overrides from file (enable=<e>)` (`src/tex.inl:101`) should show `2423` embedded with this build (the size of `kTagNames`). `m` is 0 when no override file exists.
2. **Per upload.** Set `[tex] Debug=1` and restart. Lines `[tex] upload id=<id> ... name=<file> flags=<n>` appear for the first 500 uploads (`src/tex.inl:195`). Lines `[tex] MISS id=...` appear for uploads with no name (`:185`).
3. **Per 600 frames.** `[tex] stats frame=... reads=... uploads(with data)=... named=... tagged=...` (`src/tex.inl:269`). The ratio named to uploads is the first thing to check.
4. **Unknown draws.** Set `[tex] LogUnknown=1` and restart. Lines `[tex] untagged draws=<n> id=<id> name=<file> ...` list the top 25 untagged textures (`src/tex.inl:275`).
5. **Relabel test.** Create `<game folder>\tyrender\texture_tags.csv` with a header line and one row for a known texture. Change `yes` or `no`, restart, and check the `[tex] tags:` line shows `1` override. The visual change depends on the consumer (for example `emissive` for glow, `character` for relief).
6. **Additive draws.** Set `[spill] LogEmitters=1` and restart. Lines `[spill] additive draws=<n> tex=<id> name=<file> emissive=<0|1>` appear every 600 frames (`src/tex.inl:266`).

## NEEDS INVESTIGATION

- **T1. Name coverage per level.** How many uploads and draws are untagged per level. Method: `[tex] LogUnknown=1` on each level and count names (N-27 in the master plan).
- **T2. Uploads the hooks miss.** Whether any texture is read by a path that is not `CreateFileW` and `ReadFile` (memory mapping, another read API). Method: `[tex] Debug=1` and count `MISS` lines against uploads.
- **T3. Ring eviction.** Whether a large read can push out the entry for an upload before the upload happens. Method: count `MISS` lines during level loads. Not checked.
- **T4. Owner copy of the CSV.** Whether `data/texture_tags.csv` matches the owner's `<owner folder>\RESEARCH\recording\texture_tags.csv`. Method: compare the two files. The owner copy is not in the repo.
- **T5. Lily-pad and other water names.** Method: a playtest with `[tex] LogUnknown=1` and the lily-pad textures in view; then add the names to `[fog] WaterNames`.
- **T6. Engine compressed formats.** Which compressed internal formats the game uses. Method: `ifmt` values in the `[tex] upload` lines with `[tex] Debug=1`.

## Discrepancies

- `docs/ARCHITECTURE.md` (first pass, line 102) and `docs/SHADERS.md` (first pass, section 9) say the runtime override "replaces the embedded table". The code replaces flags by name only (`src/tex.inl:98`). Code wins.
- `src/tex.inl:207` says `tyWater` is used "only when `[pbr] SkyFresnelSkipWater=1`". The code also uses it for the default `WaterStable=1` (`src/pbr.inl:20`, `src/tex.inl:209`). Code wins.
- `src/gen_tex_tags.ps1:7` uses a case-insensitive comparison and `src/tex.inl:95-97` uses an exact one (see Limits).
- `docs/START_HERE.md:26` and `:56` name the CSV source as `<owner folder>\RESEARCH\recording\texture_tags.csv` and the generator as living in `<owner folder>\SRC\tyrender`. In this repo the CSV is `data/texture_tags.csv` and the generator is `src/gen_tex_tags.ps1`. These are path differences, not content differences, and the content match is NI (T4).

## See also

- [05-shader-source-rewriting.md](05-shader-source-rewriting.md): where the per-draw uniforms are read by shader edits.
- [08-settings-pipeline-and-live-reload.md](08-settings-pipeline-and-live-reload.md): why `[tex]` keys need a restart.
- [material-model.md](../concepts/material-model.md): the classes and the consumers.
- [pbr-surface-detail.md](../shaders/pbr-surface-detail.md): `tyTex` and `tyWater` in the PBR rewrite.
- [data-files.md](../reference/data-files.md): the CSV and the generated table in detail.
- [log-and-error-messages.md](../reference/log-and-error-messages.md): the `[tex]` and `[spill]` lines.
