# TY HD Renderer: reference docs

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B, C.

TY HD Renderer is a community graphics mod for *TY the Tasmanian Tiger* on PC. It adds shadows, surface relief, ambient occlusion, haze, bounce light and a final anti-aliasing and grade pass. The game executable is not changed. The mod runs from a patched copy of the game that loads `tyrender.dll`.

This is an unofficial fan project. It is not made by, or connected to, the game's developers or publishers. It ships no game files. You need your own copy of the game (`../../README.md:5-7`).

**Status:** community beta. The README calls v0.3.5 the latest release, and the CHANGELOG lists later changes on `main` (`../../README.md:9`, `../../CHANGELOG.md:5`). Tested on one graphics card, AMD Radeon RX 7600 XT on Windows 11 (`../../README.md:11`). NVIDIA, Intel and Linux/Proton are untested.

## Who reads what

Pick your path.

- **Tester or player** (tunes `tyrender.ini`, reports bugs): [concepts/what-is-this.md](concepts/what-is-this.md), [guides/tester-guide.md](guides/tester-guide.md), [reference/presets.md](reference/presets.md), [reference/hotkeys-and-files.md](reference/hotkeys-and-files.md), [guides/session-report.md](guides/session-report.md), [faq.md](faq.md).
- **Modder** (changes the look): [concepts/material-model.md](concepts/material-model.md), [how-it-works/08-settings-pipeline-and-live-reload.md](how-it-works/08-settings-pipeline-and-live-reload.md), the `shaders/` pages, [guides/add-a-setting.md](guides/add-a-setting.md).
- **Contributor** (must continue the project cold): [guides/contributor-onboarding.md](guides/contributor-onboarding.md), [how-it-works/01-architecture-overview.md](how-it-works/01-architecture-overview.md), [concepts/game-and-engine-facts.md](concepts/game-and-engine-facts.md), [concepts/constraints-and-limits.md](concepts/constraints-and-limits.md), [guides/build-from-scratch.md](guides/build-from-scratch.md).
- **Maintainer or reviewer** (release, decisions, risks): [guides/deploy-package-release.md](guides/deploy-package-release.md), [guides/branching-and-process.md](guides/branching-and-process.md), [decisions/known-issues.md](decisions/known-issues.md), [decisions/decisions-log.md](decisions/decisions-log.md), [decisions/legal-and-licensing.md](decisions/legal-and-licensing.md), [decisions/history-and-timeline.md](decisions/history-and-timeline.md).

## Page list

Status column: **written** means the page exists in this draft set; **planned** means another chapter writes it. Final paths are under `docs/reference/`.

| Page | Purpose | Status |
|---|---|---|
| [README.md](README.md) | This index | written (Ch01) |
| [faq.md](faq.md) | Answers for testers and contributors | planned (Ch12) |
| [glossary.md](glossary.md) | Terms A to Z | written (Ch01) |
| [concepts/what-is-this.md](concepts/what-is-this.md) | What changes, what does not, status | written (Ch01) |
| [concepts/game-and-engine-facts.md](concepts/game-and-engine-facts.md) | Facts about the game and engine, with confidence | written (Ch01) |
| [concepts/constraints-and-limits.md](concepts/constraints-and-limits.md) | The "cannot" list, with causes | written (Ch01) |
| [concepts/material-model.md](concepts/material-model.md) | Texture tags, classes, policy table | written (Ch01) |
| [how-it-works/01-architecture-overview.md](how-it-works/01-architecture-overview.md) | Components, data flow, lifecycle | planned (Ch02) |
| [how-it-works/02-loading-and-injection.md](how-it-works/02-loading-and-injection.md) | How the DLL gets into the game | planned (Ch02) |
| [how-it-works/03-gl-hooking.md](how-it-works/03-gl-hooking.md) | Hook layers and hooked functions | planned (Ch02) |
| [how-it-works/04-frame-anatomy-and-pass-order.md](how-it-works/04-frame-anatomy-and-pass-order.md) | Numbered frame order with file:line | planned (Ch03) |
| [how-it-works/05-shader-source-rewriting.md](how-it-works/05-shader-source-rewriting.md) | Shader text rewrite order | planned (Ch04) |
| [how-it-works/06-render-targets-depth-and-state.md](how-it-works/06-render-targets-depth-and-state.md) | Targets, formats, state save and restore | planned (Ch03) |
| [how-it-works/07-texture-names-and-tags.md](how-it-works/07-texture-names-and-tags.md) | How textures get names and tags | planned (Ch04) |
| [how-it-works/08-settings-pipeline-and-live-reload.md](how-it-works/08-settings-pipeline-and-live-reload.md) | Where settings come from, live versus restart | planned (Ch08) |
| [how-it-works/09-overlay-input-and-hotkeys.md](how-it-works/09-overlay-input-and-hotkeys.md) | F10 overlay, F11 A/B, input | planned (Ch09) |
| [how-it-works/10-logging-timers-and-session-report.md](how-it-works/10-logging-timers-and-session-report.md) | Logs, GPU timers, session report | planned (Ch09) |
| [how-it-works/11-fallbacks-and-failure-handling.md](how-it-works/11-fallbacks-and-failure-handling.md) | What happens when an effect fails | planned (Ch03/Ch07) |
| [shaders/README.md](shaders/README.md) | Inventory of every shader and effect | planned (Ch03) |
| [shaders/ao-gtao.md](shaders/ao-gtao.md) | Ambient occlusion | planned (Ch05) |
| [shaders/gi-legacy.md](shaders/gi-legacy.md) | Legacy bounce light | planned (Ch05) |
| [shaders/gi-hiz.md](shaders/gi-hiz.md) | Hi-Z bounce light | planned (Ch05) |
| [shaders/atmosphere-fog.md](shaders/atmosphere-fog.md) | Haze and fog pass | planned (Ch06) |
| [shaders/atmosphere-froxel-compute.md](shaders/atmosphere-froxel-compute.md) | Compute froxel volume | planned (Ch06) |
| [shaders/sky-estimate-and-haze-color.md](shaders/sky-estimate-and-haze-color.md) | Sky colour estimate | planned (Ch06) |
| [shaders/emitter-light.md](shaders/emitter-light.md) | Emitter light (off by default) | planned (Ch06) |
| [shaders/taa-fxaa.md](shaders/taa-fxaa.md) | TAA and FXAA | planned (Ch07) |
| [shaders/grade-and-sharpen.md](shaders/grade-and-sharpen.md) | Sharpen, grade, tonemap | planned (Ch07) |
| [shaders/pbr-surface-detail.md](shaders/pbr-surface-detail.md) | Bump, specular, sheen | planned (Ch04) |
| [shaders/tessellation-and-relief.md](shaders/tessellation-and-relief.md) | Tessellation and relief | planned (Ch07) |
| [shaders/shadow-filter.md](shaders/shadow-filter.md) | Shadow filter patch | planned (Ch07) |
| [shaders/natural-light.md](shaders/natural-light.md) | Shadow tint and hemisphere light | planned (Ch04) |
| [shaders/water-fog-and-decals.md](shaders/water-fog-and-decals.md) | Water, fog on water, decals | planned (Ch06) |
| [shaders/distance-lod.md](shaders/distance-lod.md) | Distance reduction model | planned (Ch06) |
| [reference/settings-ao-gi-lod.md](reference/settings-ao-gi-lod.md) | Settings for AO, GI, LOD | planned (Ch08) |
| [reference/settings-atmos-fog-emit.md](reference/settings-atmos-fog-emit.md) | Settings for atmosphere, fog, emitter | planned (Ch08) |
| [reference/settings-post-shadows-light-pbr-tess.md](reference/settings-post-shadows-light-pbr-tess.md) | Settings for post, shadows, light, PBR, tessellation | planned (Ch08) |
| [reference/settings-system-debug.md](reference/settings-system-debug.md) | Settings for system, debug and environment variables | planned (Ch08) |
| [reference/presets.md](reference/presets.md) | Preset values and cost ladder | planned (Ch08) |
| [reference/hotkeys-and-files.md](reference/hotkeys-and-files.md) | Hotkeys, flag files, environment variables | planned (Ch09) |
| [reference/hooks-reference.md](reference/hooks-reference.md) | Every hooked function | planned (Ch02) |
| [reference/launcher.md](reference/launcher.md) | Launcher window, presets, Play | planned (Ch10) |
| [reference/installer-and-uninstaller.md](reference/installer-and-uninstaller.md) | Installer and uninstaller | planned (Ch10) |
| [reference/data-files.md](reference/data-files.md) | Data files and their formats | planned (Ch10) |
| [reference/tools-reference.md](reference/tools-reference.md) | Every script in `tools/` and `src/*.ps1` | planned (Ch10) |
| [reference/source-map.md](reference/source-map.md) | Every file in `src/` | planned (Ch10) |
| [reference/log-and-error-messages.md](reference/log-and-error-messages.md) | Log tags and their meaning | planned (Ch09) |
| [guides/build-from-scratch.md](guides/build-from-scratch.md) | Building the DLL, launcher and installer | planned (Ch11) |
| [guides/deploy-package-release.md](guides/deploy-package-release.md) | Install to a test folder, package, release | planned (Ch11) |
| [guides/branching-and-process.md](guides/branching-and-process.md) | Branches, phases, merge rules | planned (Ch11) |
| [guides/test-harness.md](guides/test-harness.md) | Automated test scripts | planned (Ch12) |
| [guides/ab-captures.md](guides/ab-captures.md) | A/B screenshots | planned (Ch12) |
| [guides/session-report.md](guides/session-report.md) | Reading a session report | planned (Ch09) |
| [guides/debugging.md](guides/debugging.md) | Debugging decision tree | planned (Ch12) |
| [guides/add-a-setting.md](guides/add-a-setting.md) | Worked example: adding a setting | planned (Ch11) |
| [guides/add-a-shader-pass.md](guides/add-a-shader-pass.md) | Worked example: adding a pass | planned (Ch11) |
| [guides/add-a-preset-step.md](guides/add-a-preset-step.md) | Changing the preset ladder | planned (Ch11) |
| [guides/tester-guide.md](guides/tester-guide.md) | Testers: install, check, report | planned (Ch12) |
| [guides/contributor-onboarding.md](guides/contributor-onboarding.md) | 60-minute path for contributors | planned (Ch11) |
| [decisions/history-and-timeline.md](decisions/history-and-timeline.md) | Dated timeline and version ladder | planned (Ch13) |
| [decisions/decisions-log.md](decisions/decisions-log.md) | Decisions, with dates and sources | planned (Ch13) |
| [decisions/failed-and-abandoned.md](decisions/failed-and-abandoned.md) | What was tried and dropped | planned (Ch13) |
| [decisions/known-issues.md](decisions/known-issues.md) | Issue table | planned (Ch13) |
| [decisions/roadmap-and-open-work.md](decisions/roadmap-and-open-work.md) | Open work | planned (Ch13) |
| [decisions/legal-and-licensing.md](decisions/legal-and-licensing.md) | Licences and legal status | planned (Ch13) |
| [decisions/research-notes.md](decisions/research-notes.md) | Research ideas, in our own words | planned (Ch13) |

The tree in the plan lists 66 files: 3 root, 4 concepts, 11 how-it-works, 16 shaders, 13 reference, 12 guides and 7 decisions. The plan's own count line says 67 with 14 reference pages (the documentation plan, section 1). The reference count is off by one in the plan; the tree was followed here.

## How these docs are organised

- **Template.** Pages in `how-it-works/`, `shaders/` and `guides/` follow the same headings: Purpose, How it works, Code pointers, Settings, Cost, Limits, Known issues, How to verify, NEEDS INVESTIGATION, Discrepancies, See also. Reference and decision pages use the subset that fits.
- **Citations.** Every code claim has `path:line` (or `path:start-end`) at the commit named in the status line. The path is relative to the repo root.
- **Labels.** **(inferred)** marks a reasoned guess. **NEEDS INVESTIGATION** marks a question the repo cannot answer, with the method to answer it. Some pages use **doc** for a claim that appears only in a repo document.
- **Discrepancies.** Where the code, the README, the CHANGELOG or a planning document disagree, each page lists the disagreement in its Discrepancies section. The code wins for behaviour. The docs are not fixed silently.
- **Hardware and numbers.** Frame times name the card and the build they come from. Only one card has been tested.
- **Privacy.** Local paths are written as `<game folder>`, `<repo>` or `<tools folder>`. No personal paths, credentials or contact details.
- **Links.** Relative Markdown links. Every page ends with See also.
- **Commit documented.** All pages describe the repo at commit `4db1d7d` (merge of PR #122). Changes after that commit are not covered.

## Reporting a documentation error

Open an issue in the repo with the page name, the line, and the code you checked. Mention the commit in the status line. Corrections that change a number or a default should cite the file and line.

## Links to the repo

- [Repo README](../../README.md)
- [CHANGELOG](../../CHANGELOG.md)
- [Planning and status documents](../../docs/START_HERE.md) (historical snapshots; see the Discrepancies on each page)

## Discrepancies

- **Index scope.** The plan says the index should list status from the CHANGELOG head. The CHANGELOG head says "Unreleased (main after v0.3.5)" (`../../CHANGELOG.md:5`), and the README says the latest release is v0.3.5 (`../../README.md:9`). This index says both.
- **Build naming.** The README names build `3ee6ea7` for the A/B pairs (`../../README.md:33`) and `4a2ef95` elsewhere (`../../README.md:9`). The repo HEAD is `4db1d7d`. The reviewer should decide which build each figure belongs to.

## See also

- [concepts/what-is-this.md](concepts/what-is-this.md)
- [concepts/constraints-and-limits.md](concepts/constraints-and-limits.md)
- [glossary.md](glossary.md)
- [decisions/known-issues.md](decisions/known-issues.md)
- [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md)
