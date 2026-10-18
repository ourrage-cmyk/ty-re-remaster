# Shader source rewriting (glShaderSource hook)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor) and B (tester who dumps or overrides shaders).

## Purpose

The game's own GLSL is not replaced. Instead, every shader the game compiles passes through our `glShaderSource` hook, which can:

1. write the original text to disk for reading (dump);
2. replace the text with a hand-made copy from disk (override);
3. or rewrite the text in memory with our PBR, shadow, light, decal and emitter edits, and test-compile the result before it is used.

If a rewrite does not compile, the original text is used. This page explains the path a shader takes, the order of edits, and the checks in between. It also covers how our own shaders are compiled and how uniform locations are recorded at link time. The feature-level details are in the shader pages, linked from [shaders/README.md](../shaders/README.md).

## How it works

1. **Hook.** `glShaderSource` is hooked (`src/tyrender.cpp:362`). The hook joins the strings of the call into one source text, with their lengths (`:363-364`). Its FNV-1a 32-bit hash is the file key (`:365`, hash function at `:132`).
2. **Type.** The shader type comes from a map filled by the `glCreateShader` hook (`src/tyrender.cpp:331`, lookup at `:366`). Unknown types get the extension `xs` and are not rewritten.
3. **Dump.** If `[debug] DumpShaders` is on (default 1, `src/tyrender.cpp:60`), the original text is written to `<game folder>\tyrender\dump\<hash>.<vs|fs>.glsl` if that file does not exist yet (`src/tyrender.cpp:369-371`). The dump is taken before any change, so it is the engine's text.
4. **Override check.** If `[debug] OverrideShaders` is on (default 1, `:61`), the file `<game folder>\tyrender\override\<hash>.glsl` is read if present (`:373-375`). Note the names differ: the dump has `.vs` or `.fs` in the middle, the override does not. If an override is found, the automatic rewrites are skipped for that shader (`!useOv` at `:377` and `:386`).
5. **Grass marker.** A vertex shader that contains `pushOutSpheres` or `grassTextureUVOffset` is remembered as a grass template (`src/tyrender.cpp:376`). Its strength is set separately (see [shadow-filter.md](../shaders/shadow-filter.md)).
6. **Vertex path.** Only if there is no override (`src/tyrender.cpp:377`):
   1. PBR vertex rewrite (`PbrRewriteVS`, `:379`). If it does not compile, the original is kept and a line `[pbr] VS ... rewrite failed to compile` is logged (`:380`).
   2. Tessellation vertex rewrite (`TessRewriteVS`, `:381`). It renames the outputs to `tcIn_*` and adds the world-position output. It also test-compiles its own result (`src/tess.inl:2`, `src/tyrender.cpp:566-571`).
7. **Fragment path.** Only if there is no override (`src/tyrender.cpp:386`). The order is fixed by the comment on the same line: PBR, then shadow patch, then light, then decal, then emitter. Decal and emitter both edit the stock output line, so they run in that order.
   - All five are run on a copy of the text (`:388`). If any changed it, the combined text is test-compiled (`:390`).
   - If the combined text fails, the shadow patch alone is tried (`:392-393`). Other edits are dropped for that shader.
   - The log line `glShaderSource shader=<n> hash=<h> [FS: ...]` names which edits applied (`:390`).
8. **Final check.** Any replacement text must compile, or the original is used (`src/tyrender.cpp:398`, log line `override ... does not compile -> original used`).
9. **Log and submit.** Every shader gets a log line `glShaderSource shader=<n> type=<ext> hash=<h> len=<n> [OVERRIDE]` if an override was used (`:399`). The chosen text is passed to the real `glShaderSource` (`:400`).
10. **Test compile.** `TestCompile` (`src/tyrender.cpp:471-482`) creates a scratch shader object, compiles the candidate text, and on failure logs the info log as `[compile test] FAILED` and calls `SessFailure` (`:479`). It compiles only. It does not link.
11. **Link.** The `glLinkProgram` hook (`src/tyrender.cpp:333`) runs `TessLink` (tessellation program relink, `:579-587`) and then `AfterLink` (`:561-565`). `AfterLink` records the uniform locations used by the shader feature pages: `tyShadowStrength` and `tyShLod`, and calls the per-feature link hooks for PBR, light, texture and emitter.
12. **Uniform names.** The `glGetUniformLocation` hook logs each `uniform prog=... '<name>' -> <loc>` once (`:336-337`). It records program-level facts: skinned programs (`boneMats`, `:338`), the model matrix (`:339`), the view-projection matrix (`:340`), and the engine names listed in `src/tyrender.cpp:341` (ids 1 to 8: shadow matrices, shadow direction, fog colour, shadow textures, fog coordinates, albedo sampler).
13. **Per-draw and per-frame writes.** `glUseProgram` pushes PBR and light values for the program (`src/tyrender.cpp:329`). The `glUniform*` hooks read the named engine values (`:349`, `:414`, `:423`), which feed camera, sun, fog and shadow matrices.

```
glShaderSource(s, text)
 |-- dump text to tyrender\dump\<hash>.<vs|fs>.glsl        (DumpShaders)
 |-- override file tyrender\override\<hash>.glsl?  -> use it, skip rewrites
 |-- VS: PBR VS -> test -> tess VS (test inside)
 |-- FS: PBR -> shadow -> light -> decal -> emit -> test
 |        fail: shadow only -> test
 |-- final test; fail: original text
 `-- glShaderSource(s, chosen)

glLinkProgram(p) -> TessLink (relink with or without tess) -> AfterLink (locations)
glUseProgram(p)  -> PbrApply, LightApply (once per frame per program)
```

### Our own shaders

Our passes (AO, GI, atmosphere, post, emitter) are compiled by `CompileProg` (`src/ao.inl:439`). It calls the raw `o_glShaderSource` (`src/ao.inl:447`), not the hooked one. So our own shaders are never rewritten or dumped. Their text is held as raw string literals in the `.inl` files, for example `src/ao.inl:95`. The GLSL versions in use:

- `#version 330` for full-screen passes (`src/ao.inl:95`, `src/fx.inl:8`, `src/emit.inl:64`);
- `#version 400` for generated tessellation stages (`src/tess.inl:179`, `:207`, `:260`, `:310`);
- `#version 430` for the atmosphere compute pass (`src/atmos.inl:160`).

The engine's own shaders are documented as GLSL 120 and 150 in `docs/START_HERE.md:84`. That was not verified in the repo (see NEEDS INVESTIGATION).

### Session report probe

The session report records whether some GL extensions are present, for example compute, tessellation, image load-store and texture storage (`src/sesslog.inl:251-253`). This is a log, not a gate. The gates for each effect are in the feature pages.

### Engine names the rewrites rely on

These names are read from the engine's own text or from its uniform calls. The game's shader text is not in the repo, so the list is what the code expects. Confirming each name in the dumps is NEEDS INVESTIGATION H1 and H2.

| Name | Kind | Used for | Where |
|---|---|---|---|
| `VPMatrix` | uniform | Scene view-projection; camera position; TAA jitter | `src/ao.inl:58`; `src/tyrender.cpp:340`, `:355-357`; `src/pbr.inl:212-226` |
| `shadowVPMatrix1`, `shadowVPMatrix2` | uniform (ids 1, 2) | Cascade matrices, captured for AO and atmosphere | `src/tyrender.cpp:341`; `src/atmos.inl:510-514` |
| `shadowDir` | uniform (id 3) | Sun direction for PBR and atmosphere | `src/tyrender.cpp:341`; `src/atmos.inl:517`; `src/pbr.inl:227` |
| `fogColour` | uniform (id 4) | Shade colour and sky estimate | `src/tyrender.cpp:341`; `src/atmos.inl:518`; `src/light.inl:93-99` |
| `shadowTexture1`, `shadowTexture2` | sampler (ids 5, 6) | Cascade maps; unit captured at `glUniform1i`, drawn at the next draw | `src/tyrender.cpp:341`, `:269`, `:426` |
| `shadowTexture3` | sampler (text only) | Third cascade, used by the shadow patch | `src/tyrender.cpp:189-190` |
| `fogCoords` | uniform (id 7) | Engine fog; overridden by `FogOverride` | `src/tyrender.cpp:341`, `:404` |
| `diffuseTexture` | sampler (id 8) | Albedo unit per program | `src/tyrender.cpp:341`, `:426`; `src/tex.inl:226` |
| `boneMats` | uniform | Marks skinned programs | `src/tyrender.cpp:338` |
| `modelMatrix` | uniform | Character origins for GI | `src/tyrender.cpp:339`, `:347` |
| `pushOutSpheres`, `grassTextureUVOffset` | uniforms in the vertex text | Grass template | `src/tyrender.cpp:376` |
| `lightReceive`, `shadowAmount`, `diffuseColour`, `light`, `v_normal`, `v_colour`, `v_uv`, `worldPos`, `FINAL_COLOR` | engine text | Anchors for the edits | `src/light.inl:38-49`; `src/pbr.inl:50-61`; `src/emit.inl:47-59`; `src/tex.inl:248-257` |
| `LIT`, `TEX`, `SKIN`, `SHADOWNORMS`, `AREF`, `OMNI` | `#define` values in the text | Variant tests | `src/pbr.inl:34-47`; `src/light.inl:44-45` |

The named-uniform ids are set in the table at `src/tyrender.cpp:341`. The handlers are at `src/tyrender.cpp:349`, `:414`, `:423` and in `src/atmos.inl:510-519`.

## Code pointers

| What | File:line | Note |
|---|---|---|
| Hash function | `src/tyrender.cpp:132` | FNV-1a 32-bit over the full text |
| Read file helper | `src/tyrender.cpp:134-138` | Used for overrides and the texture tag file |
| Config keys | `src/tyrender.cpp:56-64` | DumpShaders and OverrideShaders read here |
| `glCreateShader` hook | `src/tyrender.cpp:331` | Records shader types |
| `glLinkProgram` hook | `src/tyrender.cpp:333` | `TessLink`, then `AfterLink` |
| Uniform-location hook | `src/tyrender.cpp:334-345` | Program facts and named engine uniforms |
| `glUseProgram` hook | `src/tyrender.cpp:329` | PBR and light pushes |
| `glShaderSource` hook | `src/tyrender.cpp:362-402` | Dump, override, rewrite, test, submit |
| Dump path | `src/tyrender.cpp:369-371` | Written before any change |
| Override path | `src/tyrender.cpp:373-375` | Skips rewrites when used |
| Grass marker | `src/tyrender.cpp:376` | Vertex template |
| VS rewrite | `src/tyrender.cpp:377-385` | PBR, then tess |
| FS rewrite | `src/tyrender.cpp:386-397` | PBR, shadow, light, decal, emit |
| Final check and submit | `src/tyrender.cpp:398-400` | Original on failure |
| `TestCompile` | `src/tyrender.cpp:471-482` | Compile only, no link |
| Named-uniform handlers | `src/tyrender.cpp:414`, `:423`; `src/atmos.inl:510-519` | Shadow matrices, shadow direction, fog colour |
| Link hook of features | `src/tyrender.cpp:561-565` | `AfterLink` |
| Tess relink fallback | `src/tyrender.cpp:579-587` | Original vertex shader put back |
| Our compile helper | `src/ao.inl:439-447` | Raw `o_glShaderSource` |
| Extension log | `src/sesslog.inl:251-253` | Session report only |

## Settings

Code defaults are the values passed to `GetPrivateProfile*` (file:line). Keys are not in `src/default_tyrender.ini`, so the code default applies. Full tables: [settings-system-debug.md](../reference/settings-system-debug.md).

| Key | Code default (file:line) | Shipped | Live or restart | Effect |
|---|---|---|---|---|
| `[debug] DumpShaders` | 1 (`src/tyrender.cpp:60`) | not set | Restart (read only in `LoadConfig`, `:56-64`, which the reload path does not call, `:598`) | Writes the engine text to `tyrender\dump\` |
| `[debug] OverrideShaders` | 1 (`src/tyrender.cpp:61`) | not set | Restart (as above) | Uses `tyrender\override\<hash>.glsl` when present |
| `[debug] ForceFallback` bit 16 | 0 (`src/ao.inl:491`) | not set | Restart for the light bit (compile-time text, `src/light.inl:36`). The value is read in `LoadAOConfig` (`src/ao.inl:491`), which the reload path calls | Makes the light rewrite fail on purpose |
| `[pbr] DebugWater`, `[pbr] Debug`, `[light]` gates | see feature pages | see feature pages | Restart for compile-time text edits | Change the text these edits produce |

Note that `DumpShaders` writes a file for every shader the first time its hash is seen, so the dump folder grows on every run with new engine text. Dumps are not deleted by the mod, and the folder is created at start-up (`src/tyrender.cpp:710`).

## Cost

- **Not measured** in the repo. The rewrite runs once per shader object the game creates. Each candidate is test-compiled one to three times (`src/tyrender.cpp:380`, `:390`, `:398`). There is no shader cache between runs (see Limits).
- **What scales it:** the number of shader objects the game creates, and the size of their text. Dump writes happen only once per hash.

## Limits

- **Text matching.** Every edit is a literal search and replace on the engine's text (for example `src/tyrender.cpp:180-191`, `src/pbr.inl:60-61`). If the game's text differs, the edit is silently skipped, or the whole edit is dropped at the test-compile stage. Nothing reports which anchor failed.
- **Compile test only.** `TestCompile` compiles a fragment or vertex shader alone. It does not link. A vertex and fragment pair can still fail at link time (see NEEDS INVESTIGATION).
- **Partial fallback.** When the combined fragment rewrite fails, only the shadow patch is retried (`src/tyrender.cpp:392-393`). PBR, light, decal and emitter edits are dropped for that shader. The log line at `src/tyrender.cpp:392` lists which of PBR, shadow and light were in the failed attempt, but not which one caused the failure.
- **No cache.** Rewrites are recomputed each time a shader is created (no in-memory or on-disk cache for rewritten text, inferred from `src/tyrender.cpp:362-402`). No `glGetProgramBinary` use was found (grep of `src`).
- **Overrides skip all rewrites** for that shader (`src/tyrender.cpp:377`, `:386`). An override therefore turns off PBR, shadow, light and emitter edits for that one shader.
- **Engine text is not in the repo.** The engine's shader files are only known from dumps and from the code's assumptions.

## Known issues

- No open issue in `README.md` or `CHANGELOG.md` names this path. `README.md:265` describes the fallback policy: an effect that cannot compile switches itself off. `docs/START_HERE.md:84` says engine compile errors go to `TyOutputLog.txt` and then to an int3 trap in the engine.
- Shadow patch and light patch problems are in the feature pages: [shadow-filter.md](../shaders/shadow-filter.md) and [natural-light.md](../shaders/natural-light.md).

## How to verify

1. **Dumps exist.** Run the game once with defaults. `<game folder>\tyrender\dump\` should contain `<8 hex digits>.vs.glsl` and `<8 hex digits>.fs.glsl` files. (The log `glShaderSource shader=... hash=...` gives the same name.)
2. **Hash to shader.** In `tyrender.log`, find `glShaderSource shader=<n> type=fs hash=<h> len=<n>`. The `<h>` matches the dump file name. The `len` value is the byte length of the engine text (`src/tyrender.cpp:399`).
3. **Rewrite applied.** A line with `[FS: PBR SHADOW LIGHT ...]` (any subset of those tags) means the rewrite was accepted (`src/tyrender.cpp:390`).
4. **Override.** Copy a dumped file to `<game folder>\tyrender\override\<hash>.glsl` (note: no `.fs` in the name), change one comment in it, and restart. The log should show `[OVERRIDE]` on that shader's line (`src/tyrender.cpp:399`). The rewrites for that shader should not appear.
5. **Failure path.** Put a broken override in place. The log should show `override for <hash> does not compile -> original used` (`src/tyrender.cpp:398`). Remove it again afterwards.
6. **Compile-test failures.** `[compile test] FAILED` followed by the info log means a candidate was rejected (`src/tyrender.cpp:479`). The session report also records it (`SessFailure`, `:479`).

## NEEDS INVESTIGATION

- **H1. The 17 `.shader` files and their variants.** The engine ships 17 shader files in `Override_PC.rkv` (`docs/START_HERE.md:84`). Their names and the variant defines are not in the repo. Method: run with the default dump, list `tyrender\dump\`, and list `Override_PC.rkv` with `tools/rkvtool.ps1`. See N-3 in the master plan.
- **H2. Engine GLSL version.** `docs/START_HERE.md:84` says GLSL 120 and 150. Method: `grep -h "#version" <game folder>\tyrender\dump\*.glsl`.
- **H3. Link-time mismatch between vertex and fragment rewrites.** See [pbr-surface-detail.md](../shaders/pbr-surface-detail.md), item P4. Method: count link failures in the log after a run with the vertex rewrite disabled for one program (a code change), or check the dumps for programs that have a fragment rewrite but no vertex rewrite.
- **H4. Shader creation count.** How many shader objects are created per level, and how long the rewrite takes. Method: timing around the hook with `GProf`-style marks (a code change). Not measured.
- **H5. Whether the engine ever recompiles a shader after the first run.** This matters for live changes to `Patch` and `Enable`. Method: count `glShaderSource` lines after a level change in the log.

## Discrepancies

- `src/tyrender.cpp:1-7` (file header) describes a "Stage 1" GL logger, and names the `[debug]` keys. `docs/ARCHITECTURE.md` (first pass, around line 33) already says the header is stale. Code wins.
- The dump file name has `.vs` or `.fs` in it (`src/tyrender.cpp:370`). The override file name does not (`src/tyrender.cpp:374`). `tyrender.cpp:5-6` says overrides are read from `tyrender\override\<hash>.glsl`, which matches the code.
- `docs/ARCHITECTURE.md` (first pass, line 24) gives the `glShaderSource` range as lines 362-402. That matches `src/tyrender.cpp:362-401`.

## Question map (chapter questions answered on this page)

| Question | Answer location |
|---|---|
| Q069 shaders embedded as text | "Our own shaders" above (`src/ao.inl:95`) |
| Q070 GLSL versions | "Our own shaders" above |
| Q071 capture and rewrite order | "How it works" 1-9 |
| Q072 dump and override, hash | "How it works" 3-4; `src/tyrender.cpp:132` |
| Q073 `TestCompile` | "How it works" 10 |
| Q074 patch detection and failure | [shadow-filter.md](../shaders/shadow-filter.md), "How it works" 1-3 |
| Q075 PBR targets | [pbr-surface-detail.md](../shaders/pbr-surface-detail.md), "How it works" 1-2 |
| Q076, Q077 light rewrite and `tyLt` | [natural-light.md](../shaders/natural-light.md) |
| Q078 tessellation rewrite | "How it works" 6.2 (`src/tess.inl:98-157`, rename at `:154`); detail in [tessellation-and-relief.md](../shaders/tessellation-and-relief.md) |
| Q079 decal and emitter rewrites | "How it works" 7 (`src/tex.inl:248-257`; `src/emit.inl:47-59`, `tyEmit`) |
| Q080 failure path | "How it works" 7-9; Limits |
| Q081 uniform locations and refresh | "How it works" 11, 13 (`src/tyrender.cpp:561-565`; `src/light.inl:83`, `:120`; `src/pbr.inl:169`, `:189`) |
| Q082 `glUseProgram` pushes | "How it works" 13 (`src/tyrender.cpp:329`; `src/pbr.inl:189-203`) |
| Q083 shadow map size | [shadow-filter.md](../shaders/shadow-filter.md), item 11 (`src/tyrender.cpp:255`, `:274`) |
| Q084 variants | Engine names table; NI H1 |
| Q085 caching | Limits ("No cache") |

## See also

- [03-gl-hooking.md](03-gl-hooking.md): how the hooks are installed.
- [11-fallbacks-and-failure-handling.md](11-fallbacks-and-failure-handling.md): what the user sees when an effect is turned off.
- [07-texture-names-and-tags.md](07-texture-names-and-tags.md): the texture side of the same draws.
- [shadow-filter.md](../shaders/shadow-filter.md), [pbr-surface-detail.md](../shaders/pbr-surface-detail.md), [natural-light.md](../shaders/natural-light.md): the edits this path applies.
- [hotkeys-and-files.md](../reference/hotkeys-and-files.md): the dump and override folders.
