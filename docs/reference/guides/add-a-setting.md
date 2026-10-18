# Add a setting

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor).
> Verification note: the walk-through uses a real existing key, `[post] Sharpen`, to show where each piece lives. The new key, `[post] Grain`, is hypothetical. Code shown for `Grain` is illustrative and is not in the repo. No build was run by the writer.

## Purpose

This page shows every place a setting touches, in order, so that a new ini key is added once, read correctly, reloaded correctly, and shown to users where it should be. It uses `[post] Sharpen` as the model because it goes through all the layers: the config loader, a shader uniform, the reload path, the in-game overlay and the launcher.

## How it works

The steps below apply to a key in an existing module. A key in a new module needs one more step, the loader hookup (step 8).

1. **Decide the key.** Name, section, default, and whether it is live (changes apply while the game runs) or restart-only. `[post]` keys are live because `LoadFXConfig` runs on the reload path (`src/tyrender.cpp:598`) and from the overlay (`src/overlay.inl:52`). Restart-only keys are the ones read only at startup (see the list in `docs/START_HERE.md:70`). For the example, `Grain` is live.
2. **Declare the global.** Add a file-scope variable next to the other `[post]` globals (`src/fx.inl:144` holds `g_fxSharpen`, `g_fxVignette` and others). Illustrative:
   ```
   static float g_fxGrain = 0.0f;   // next to g_fxSharpen (fx.inl:144)
   ```
3. **Read it in the loader.** `LoadFXConfig` reads every `[post]` key with the `F` macro. The macro takes the key, the code default as a string, and the variable (`src/fx.inl:152-153`). Illustrative line, inside the `F` block:
   ```
   F("Grain", "0.0", g_fxGrain)
   ```
   The default string is the code default. A key missing from the ini gets this value (`src/fx.inl:153` for `Sharpen`, default `"0.25"`).
4. **Pass it to the shader.** Two places. First, the shader declares a uniform (`src/fx.inl:60`, in the `kGrade` string that runs from `src/fx.inl:57` to `:95`). Second, the uniform is set in `RunFX` with the `U` helper (`src/fx.inl:250`, `U("u_sharpen", g_fxSharpen)`). A new uniform needs both. Keep the uniform in the same program that uses it.
5. **Use the uniform.** The shader code for `Sharpen` is guarded by `if (u_sharpen > 0.0)` (`src/fx.inl:64`). Copy that style: a zero value should skip the work, so the default costs nothing.
6. **Ship the default in the ini (only if needed).** The shipped file has a `[post]` section at `src/default_tyrender.ini:49-53`. `Sharpen` is not in it, so `Sharpen` uses the code default. Add a key to the shipped file only if the shipped value must differ from the code default. The file's header says it is the Medium preset (`src/default_tyrender.ini:1`).
7. **Optional: the overlay and the launcher.**
   - Overlay: add a row to `g_ovItems` (`src/overlay.inl:22-31`; `Sharpen` is at `:25`, with min, max and step). Overlay changes call `OvReload`, which calls `LoadFXConfig` for `[post]` (`src/overlay.inl:51-57`).
   - Launcher: add a row to `g_sl[]` (`src/launcher_ui.inc:17-33`; `Sharpen` is at `:32`, with section, key, label, min, max, default, and a tip). `SaveAll` writes every slider back to the ini (`src/launcher_ui.inc:246`), so the launcher's default is written as soon as the user changes anything.
8. **Startup and reload hookup (new module only).** A key in a new section needs its loader called in two places: the startup list (`src/tyrender.cpp:714-725`, where `LoadFXConfig` is at `:717`) and the reload list (`src/tyrender.cpp:598`). Reload works by creating the file `tyrender\reload` (`src/tyrender.cpp:597`). The DLL logs `[cfg] reloaded all` after a reload (`src/tyrender.cpp:603`).
9. **Document.** Add a row to the settings reference page for the section (see See also). Add a line under "Unreleased" in `CHANGELOG.md` (`CHANGELOG.md:5`). Use the same wording style as the lines already there, which say "Untested" when the owner has not played the change.
10. **Test.** Build (`guides/build-from-scratch.md`). Set the key in a copy of the ini. For a live key, trigger a reload and compare two runs with the in-session A/B tool (`tools/suite.ps1`). Only live keys can differ between A/B variants (`docs/agent-tasks/BRIEFING.md:64-65`).

```
config file -> LoadFXConfig (F macro) -> global -> RunFX (U helper) -> shader uniform -> shader code
                    ^                                  |
                    |                                  +-- overlay row (overlay.inl) and launcher row (launcher_ui.inc)
   startup (tyrender.cpp:717) and reload (tyrender.cpp:598)
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Model key `Sharpen`: header comment | `src/fx.inl:5` | Says `Sharpen=0.35`, which differs from the code default (see Discrepancies). |
| Model key: global | `src/fx.inl:144` | `g_fxSharpen = 0.25f`. |
| Model key: loader | `src/fx.inl:146-157` | `LoadFXConfig`. |
| Model key: `F` macro line | `src/fx.inl:153` | Default `"0.25"`. |
| Loader end (`Tonemap`, `#undef F`) | `src/fx.inl:155-156` | Non-`F` keys and cleanup. |
| Uniform declaration | `src/fx.inl:60` | Inside `kGrade`. |
| Shader use | `src/fx.inl:64-69` | Guarded by `u_sharpen > 0.0`. |
| Uniform setter | `src/fx.inl:250` | `U("u_sharpen", ...)`. |
| Shipped `[post]` section | `src/default_tyrender.ini:49-53` | No `Sharpen` line. |
| Overlay item table | `src/overlay.inl:22-31` | `Sharpen` row at `:25`. |
| Overlay reload | `src/overlay.inl:51-57` | Calls `LoadFXConfig` for `[post]` (`:52`). |
| Launcher slider table | `src/launcher_ui.inc:17-33` | `Sharpen` row at `:32`, default `0.35`. |
| Launcher save of all sliders | `src/launcher_ui.inc:246` | Loops over every slider. |
| Launcher slider write | `src/launcher_ui.inc:182-184` | Writes the value as a fixed-point string. |
| Unused copy of the launcher table | `src/ui_part_a.inc:31` | Not included by any file. Edit `launcher_ui.inc`. |
| Startup loader call | `src/tyrender.cpp:717` | `LoadFXConfig()` in `DllMain`. |
| Reload trigger | `src/tyrender.cpp:597-598` | Reads `tyrender\reload`, deletes it, reloads. |
| Reload log line | `src/tyrender.cpp:603` | `[cfg] reloaded all`. |
| Config file path | `src/fx.inl:147` | Built from the game folder and `tyrender.ini`. |

## Settings

For the walk-through, the new key is `[post] Grain` (hypothetical). Its row in the settings reference would look like this (illustrative):

| Key | Type and range | Code default | Shipped | Live or restart | Effect |
|---|---|---|---|---|---|
| `Grain` | float, 0 to 1 (illustrative) | 0.0 | not set | live (illustrative) | Film grain strength. |

The real key `Sharpen` is listed in `reference/settings-post-shadows-light-pbr-tess.md`.

## Cost

Not measured. A zero value must cost nothing, so guard the shader work with the uniform, as `Sharpen` does at `src/fx.inl:64`. Measure any non-zero cost with the GPU timer lines (`docs/START_HERE.md:76`).

## Limits

- **No range check.** `F` reads the value as a string and converts it with `atof` (`src/fx.inl:152`). A typo gives 0. An out-of-range value is not clamped. Clamp in the shader or in the loader if the range matters.
- **Three copies of the default.** The code default, the launcher default and the header comment can disagree (see Discrepancies). Pick one value and make the other two match, or say why they differ.
- **The launcher writes every slider.** After one save from the launcher, the ini holds every slider value, including the launcher default. When the launcher opens, each slider takes its position from the ini (`src/launcher_ui.inc:185-188`). On the next save, every slider is written back, rounded to the slider's 100 steps. A hand-set value can therefore change slightly, or be replaced by the launcher default, after one save.
- **Overlay ranges are separate.** The overlay table has its own min, max and step (`src/overlay.inl:25`), independent of the launcher.

## Known issues

- No open issue names this procedure. Related: issue #13 proposes `tyrender.ini` as the single source of truth for settings, next to the game's own settings file (`docs/ISSUES_SEED.md:16`).

## How to verify

Not run by the documenter. For the real key `Sharpen`:

1. Find the value in the code (`src/fx.inl:153`), in the launcher (`src/launcher_ui.inc:32`), and in the header (`src/fx.inl:5`). They should match. They do not (see Discrepancies).
2. Set `Sharpen=0` in a copy of the ini, trigger a reload with `tyrender\reload`, and compare a frame with the same frame at `Sharpen=0.35`. The image should change. Use the A/B tool for a fixed camera (`tools/suite.ps1`).
3. Read `tyrender.log` for `[cfg] reloaded all` after the reload (`src/tyrender.cpp:603`).

For a new key, the same three checks apply after the key is added.

## NEEDS INVESTIGATION

- **S1. Which keys are restart-only.** `docs/START_HERE.md:70` gives examples. The full list is in the settings reference (Ch08). Method: for each key, check whether its loader is on the reload path (`src/tyrender.cpp:598`).
- **S2. Whether the overlay table and the launcher table agree for every key.** Method: compare `g_ovItems` (`src/overlay.inl:22-31`) with `g_sl[]` (`src/launcher_ui.inc:17-33`) for each key.

## Discrepancies

- **Sharpen default, three values.**
  - Code default: `0.25` (`src/fx.inl:144` and `:153`).
  - Header comment: `Sharpen=0.35` (`src/fx.inl:5`).
  - Launcher default: `0.35` (`src/launcher_ui.inc:32`), written to the ini on the first launcher save (`src/launcher_ui.inc:246`).
  - Shipped ini: no `Sharpen` line (`src/default_tyrender.ini:49-53`).
  A user who never opens the launcher gets 0.25. A user who saves in the launcher gets 0.35. The docs should say which one applies.
- **Stale copy of the launcher table.** `src/ui_part_a.inc:31` has the same row, and no file includes that file. Editing it does nothing. The first pass did not note this.

## See also

- [Add a shader pass](add-a-shader-pass.md)
- [Add a preset step](add-a-preset-step.md)
- [Settings pipeline and live reload](../how-it-works/08-settings-pipeline-and-live-reload.md)
- [Settings: post, shadows, light, PBR and tessellation](../reference/settings-post-shadows-light-pbr-tess.md)
- [Overlay and hotkeys](../how-it-works/09-overlay-input-and-hotkeys.md)
- [Contributor onboarding](contributor-onboarding.md)
