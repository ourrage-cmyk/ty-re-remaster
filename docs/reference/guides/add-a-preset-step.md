# Add a preset step

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), C (maintainer who signs off the ladder).
> Verification note: the walk-through is based on the launcher source, the preset script and the shipped ini as they are at 4db1d7d. The example change (a new value for one rung) is illustrative. No build or measurement was done by the writer.

## Purpose

A preset is a named set of ini values that the launcher writes when the user clicks a button. The four presets are Low, Medium, High and Ultra. This page explains where each preset is defined, what a change to one rung touches, which copies must be kept in step, and what evidence the owner expects before a preset changes. The Medium rung is also the shipped default, so a change there reaches every new install.

## How it works

1. **The four names and the count.** `NPRESET` is 4 (`src/launcher_ui.inc:5`). Names are in `kPresets` (`src/launcher_ui.inc:6`): Low, Medium, High, Ultra.
2. **The values, one row per preset.** `kPresetVals` holds one `Settings` row for each preset (`src/launcher_ui.inc:9-15`; rows at `:11` Low, `:12` Medium, `:13` High, `:14` Ultra). The fields are in the struct at `src/launcher_ui.inc:4`, in this order: `ao`, `aoSlices`, `aoSteps`, `atmos`, `atmosSteps`, `tess`, `tessAll`, `tessLevel`, `post`, `aa`, `shScale`, `shTaps`, `gi`, `giRays`, `contact`.
3. **Side values per rung.** `kPresetHaze` sets `[atmos] Intensity` (`src/launcher_ui.inc:7`). `kPresetGrade` sets `[post] Brightness` and `Saturation` (`src/launcher_ui.inc:8`). The click handler also writes `[atmos] ContactShadows` (0.35 or 0), `Desaturation` (0.8 for Low, 0.3 otherwise) and `HazeLift` (0.35 for Low, 0.15 otherwise) (`src/launcher_ui.inc:493`).
4. **Buttons.** Each preset button is created with id `ID_P0 + i` (`src/launcher_ui.inc:409`; enum at `:133`). The click handler copies the row into the working settings, updates the controls, writes the side values, saves all sliders and shows the help text (`src/launcher_ui.inc:492-493`).
5. **Save and live reload.** `SaveAll` writes the settings and all sliders to the ini, then sends a live reload if the game is running (`src/launcher_ui.inc:246`).
6. **Help text.** Each rung has a short note (`src/launcher_ui.inc:216-220`) and a title and long text (`src/launcher_ui.inc:269-274`). These are shown in the help panel.
7. **Matching.** `MatchPreset` compares the working settings with each row and returns the matching rung, or -1 for a custom mix (`src/launcher_ui.inc:209-214`). It compares 15 fields. A field added to `Settings` must also be added here, or the match ignores it.
8. **Reading and writing the struct.** `SaveSettings` and `LoadSettings` map the struct to ini keys (`src/launcher_ui.inc:61-79`). A new field needs an entry in both.
9. **Cost estimate.** `CostEstimate` adds a fitted cost for each effect (`src/launcher_ui.inc:44-56`). A change that alters an effect's cost should update the fitted constants. The comments at `src/launcher_ui.inc:10` and `:46` record the owner's measured totals per preset (see Discrepancies).
10. **Script copy.** `tools/presets.ps1` holds a second copy of the ladder: `$Presets` (`tools/presets.ps1:13-18`, Medium at `:15`) and `$BalancedCandidates` (`tools/presets.ps1:19-28`). `NewIni` builds an ini from a row (`tools/presets.ps1:3-12`). The script is used by the tuning tools (`tools/presets.ps1:1`).
11. **Shipped ini copy.** `src/default_tyrender.ini` is the Medium preset (header at `:1`). It is copied as `tyrender.ini` in the zip (`src/package.ps1:9`). Keys are at `:2-6` for `[ao]`, `:43-44` for `[tess]`, `:47-48` for `[shadows]`, and `:32-36` for `[atmos]`.
12. **Documentation copies.** The README has a presets section (`README.md:151`). The PERFORMANCE document has the measured ladder (`docs/PERFORMANCE.md`). A third settings page lists each preset's values (`reference/presets.md`).

```
launcher_ui.inc: kPresetVals (rows) + kPresetHaze + kPresetGrade + kPresetNote/Title/Text
        |  click -> SaveAll -> ini (game folder) -> live reload
        |
tools/presets.ps1: $Presets (same ladder, hand-copied)     <- used by tuning tools
default_tyrender.ini: Medium values (hand-copied)          <- shipped default, zip copy
README.md (presets section) and docs/PERFORMANCE.md        <- docs
```

## Procedure for one changed rung

1. Decide the change and the evidence first. The owner's rule is measured cost and a playtest, not a guess (`docs/PLAN_CONSOLIDATED.md:24`).
2. Edit the row in `kPresetVals` (`src/launcher_ui.inc:9-15`). Edit the side values if they change (`:7-8`).
3. If a new field is needed, add it to the struct (`:4`), `SaveSettings`, `LoadSettings`, `MatchPreset`, and the code that applies the value.
4. Make the same change in `tools/presets.ps1` (`:13-18`).
5. If the change is to Medium, update `src/default_tyrender.ini` (the keys listed in step 11 above). Its header says "Medium" and gives a rough frame rate. Keep the header true (see Discrepancies).
6. Update the cost constants if the effect's cost changed (`src/launcher_ui.inc:44-56`).
7. Update the help text if the rung's description changed (`src/launcher_ui.inc:216-220`, `:269-274`).
8. Update the README presets section and the reference presets page.
9. Re-measure the whole ladder with the GPU timers, not only the changed rung (`docs/PLAN_CONSOLIDATED.md:56` says the owner re-measures the full ladder before release). Update `docs/PERFORMANCE.md`.
10. Run the release gate on the merged build (`docs/PLAN_CONSOLIDATED.md:55`; see the deploy page) and take new A/B shots (`docs/PLAN_CONSOLIDATED.md:58`).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Preset count | `src/launcher_ui.inc:5` | `NPRESET 4`. |
| Preset names | `src/launcher_ui.inc:6` | Low, Medium, High, Ultra. |
| Haze per rung | `src/launcher_ui.inc:7` | Writes `[atmos] Intensity`. |
| Brightness and saturation per rung | `src/launcher_ui.inc:8` | Writes `[post]`. |
| Ladder values | `src/launcher_ui.inc:9-15` | One row per rung. |
| Measured totals comment | `src/launcher_ui.inc:10` | Medium 11.1 ms total in this comment. |
| Settings struct | `src/launcher_ui.inc:4` | Field order used by the table. |
| Button ids | `src/launcher_ui.inc:133` | `ID_P0`. |
| Cost constants | `src/launcher_ui.inc:44-56` | `CostEstimate`. |
| Cost model note | `src/launcher_ui.inc:46` | "Preset totals measured" (Medium 6.8 over stock). |
| Save and load mapping | `src/launcher_ui.inc:61-79` | `SaveSettings`, `LoadSettings`. |
| Matching | `src/launcher_ui.inc:209-214` | `MatchPreset`. |
| Help text note | `src/launcher_ui.inc:216-220` | `kPresetNote`. |
| Help title and text | `src/launcher_ui.inc:269-274` | `kPresetTitle`, `kPresetText`. |
| Save all and live reload | `src/launcher_ui.inc:246` | `SaveAll`. |
| Button creation | `src/launcher_ui.inc:409` | Position from the index. |
| Click handler | `src/launcher_ui.inc:492-493` | Applies one rung. |
| Script ladder | `tools/presets.ps1:13-18` | `$Presets`. |
| Script Medium row | `tools/presets.ps1:15` | Differs from the launcher (see Discrepancies). |
| Script candidates | `tools/presets.ps1:19-28` | `$BalancedCandidates`, older name. |
| Script ini builder | `tools/presets.ps1:3-12` | `NewIni`. |
| Shipped Medium header | `src/default_tyrender.ini:1` | Says Medium and a frame rate. |
| Shipped `[ao]` | `src/default_tyrender.ini:2-6` | `Slices=3`, `Steps=6`. |
| Shipped `[shadows]` | `src/default_tyrender.ini:47-48` | `MapScale=2`, `Taps=16`. |
| Packaged copy | `src/package.ps1:9` | Copied as `tyrender.ini`. |
| README presets section | `README.md:151` | Heading. |

## Settings

A preset writes these keys. The table part comes from `SaveSettings` (`src/launcher_ui.inc:61-68`): `[ao] Enable, Legacy, Slices, Steps`; `[atmos] Enable, Froxel, Steps`; `[tess] Enable, Scope, MaxLevel, Debug`; `[post] Enable, AA`; `[shadows] Patch, MapScale, Taps`; `[gi] Enable, Mode, Rays`; and the debug counters `[debug] CaptureFrame` and `CaptureFrame2`, which are reset to 0. The side values come from the click handler (`src/launcher_ui.inc:493`): `[atmos] Intensity, ContactShadows, Desaturation, HazeLift` and `[post] Brightness, Saturation`. The settings reference lists every key.

## Cost

Measured in the owner's records, not by this page. The launcher comment gives Low 6.0, Medium 11.1, High 17.2 and Ultra 32.1 ms total frame time at 2560x1440 on an AMD RX 7600 XT, with stock at 5.7 ms (`src/launcher_ui.inc:10`). The cost model comment gives a different set of totals (`src/launcher_ui.inc:46`). See Discrepancies. The owner's PERFORMANCE document is the place for the measured ladder (`docs/PERFORMANCE.md`).

## Limits

- **Several copies by hand.** The launcher table, the script table, the shipped ini and the documents are kept in step by hand. There is no generator. A change to one does not change the others.
- **Four fixed buttons.** The button positions come from the index (`src/launcher_ui.inc:409`). A fifth rung needs a layout check that this page did not do.
- **Matching is exact.** A value changed by hand in the ini makes `MatchPreset` return -1, so the launcher shows the custom mix (`src/launcher_ui.inc:209-214`).
- **Cost is one GPU.** Measured numbers are for the AMD RX 7600 XT only (`docs/START_HERE.md:7`).
- **Control ids.** The preset buttons use ids 100 to 103 from the same enum as the other controls, which start at 110 (`src/launcher_ui.inc:133`). A fifth rung would take 104 (inferred free; check the enum before using it).

## Known issues

- **Medium differs between copies.** The launcher and the shipped ini agree with each other. The script copy does not (see Discrepancies). The owner notes already flag the script's older naming (`docs/PLAN_CONSOLIDATED.md:84`).
- **Cost numbers disagree inside the launcher file.** See Discrepancies.

## How to verify

Not run by the documenter. Reader's checks:

1. Compare the Medium row in `src/launcher_ui.inc:12` with `tools/presets.ps1:15` and `src/default_tyrender.ini:2-6` and `:47-48`. The first and third should match. The script will not, unless it has been updated.
2. In a copy of the game folder, click Medium in the launcher and read the written ini. Expect `[ao] Slices=3` and `Steps=6`, `[shadows] Taps=16`, `[atmos] Steps=24` (from `src/launcher_ui.inc:12`). Checked against the code only.
3. Click Low and check that `[post] AA=1` (`src/launcher_ui.inc:11`).

## NEEDS INVESTIGATION

- **R1. Which Medium the owner's last playtests used.** The script copy and the launcher disagree. Method: ask the owner which ini was used in the last playtest, or read the playtest folder's ini if it is in the owner's results.
- **R2. The cost numbers.** The launcher file gives two sets of totals (`src/launcher_ui.inc:10`, `:46`). Method: re-measure the ladder with the GPU timers, as the owner's process requires, and record the build id and GPU.
- **R3. Whether the fifth-rung layout fits.** Method: run the launcher with five rungs in a scratch copy and check the window. Not attempted.

## Discrepancies

- **Medium rung, script versus launcher.**
  - `tools/presets.ps1:15`: `ao` slices 2, steps 4; `atmosSteps` 16; `shTaps` 12.
  - `src/launcher_ui.inc:12`: slices 3, steps 6; `atmosSteps` 24; `shTaps` 16.
  - `src/default_tyrender.ini:2-6, 47-48` and the atmosphere step default (`src/atmos.inl:292`, 24): agree with the launcher.
  The script copy is the odd one out. The other rungs match between the script and the launcher.
- **Cost totals in one file.** `src/launcher_ui.inc:10` says Medium 11.1 ms total (stock 5.7). `src/launcher_ui.inc:46` says Medium 6.8 ms over stock (and Balanced 5.4). These are different quantities (total frame time versus added cost) and the file mixes them. Report total and added costs with their labels.
- **Shipped header.** `src/default_tyrender.ini:1` says "about 80 fps". `docs/PLAN_CONSOLIDATED.md:82` says the header says "about 90 fps" and calls it an old number. The repo file says 80. The consolidated note is out of date, or the file changed after the note was written.
- **Balanced name.** `src/package.ps1:9` and `tools/presets.ps1:19` use the older name "Balanced". `docs/START_HERE.md:6` says Balanced is retired and Medium is the default.
- **Stale launcher copy.** `src/ui_part_a.inc:7` gives Medium a haze value of 2.2. `src/launcher_ui.inc:7` gives 1.2. The first file is not included by any source file. Use `launcher_ui.inc` as the live copy.

## See also

- [Add a setting](add-a-setting.md)
- [Presets](../reference/presets.md)
- [Launcher](../reference/launcher.md)
- [A/B captures](../guides/ab-captures.md)
- [Deploy, package and release](deploy-package-release.md)
- [Settings pipeline and live reload](../how-it-works/08-settings-pipeline-and-live-reload.md)
