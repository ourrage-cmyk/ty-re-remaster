# Legal, licensing and privacy
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone). Audience: C. This page is a technical record, not legal advice.

## Purpose
This page records the legal position the repository states or implies: whether the project's own code has a licence, how the fan-project status and the game's assets are handled, what third-party techniques are cited, what the research notes say about a shader pack's licence, and what personal data the repository contains. It answers Q225, Q226 and Q227 (with the privacy part of the session report).

## How it works
Four kinds of statement appear in the repository:
1. **Disclaimers** in the README: fan project, no game files shipped, the user needs their own copy, rights belong to the rights holders (`README.md:7`, `README.md:258`).
2. **Rules** in the research notes that restrict code reuse from a third-party shader pack (`docs/research/minecraft_shaderpack_port_ranking.md:9-13`, `docs/research/WORLD_SPACE_AO_GI_FOG.md:4`).
3. **Citations** of published techniques in code and plans.
4. **Privacy design** of the session report (`src/sesslog.inl:1-8`, `src/sesslog.inl:277-278`).

The repository has no LICENSE, COPYING or NOTICE file. `git ls-files` lists none, and the only commit in the clone is `4db1d7d`, so no earlier file can be checked.

## Code pointers

| What | file:line | Note |
|---|---|---|
| Fan project and no game files | `README.md:7` | Says "It includes no game files" and "you need your own copy" |
| Trademark and rights statement | `README.md:258` | "belong to their own rights holders" |
| Community credit and AI note | `README.md:259` | "A hobby project, made with Claude" |
| Shader pack licence and reuse rule | `docs/research/minecraft_shaderpack_port_ranking.md:9-13` | Summarised on [research-notes.md](research-notes.md) |
| Reuse rule for the world-space notes | `docs/research/WORLD_SPACE_AO_GI_FOG.md:4` | "Nothing from the shader pack is copied" |
| Published technique citation (tessellation) | `src/tess.inl:3` | Boubekeur and Alexa (2008) Phong tessellation |
| FXAA and CAS-style sharpen names | `src/fx.inl:4,63,8` | Named after published or public techniques; the code is the project's own text |
| Session report: local only | `src/sesslog.inl:1` | "never sent anywhere" |
| Session report: exclusions | `src/sesslog.inl:277-278` | No usernames, computer name, serials, addresses, programs, processes or personal paths |
| Session report: keep count and size | `src/sesslog.inl:372-375` | `KeepFiles` default 10, `MaxBytes` default 1 MB |
| Report button opens the folder | `src/launcher_ui.inc:500` | Opens the logs folder only; no upload |

## Settings
- `[report] Enable` (default 1), `CaptureSeconds` (300), `MaxBytes` (1048576), `KeepFiles` (10): `src/sesslog.inl:372-375`. `Enable=0` writes no file. Settings page: [../reference/settings-system-debug.md](../reference/settings-system-debug.md).

## Cost
Not applicable.

## Limits
- This review did not read the shader pack's licence text. The research notes say the licence was not reviewed by a lawyer (`docs/research/minecraft_shaderpack_port_ranking.md:11`).
- No audit of every shader for copied text was done. The repository asserts reuse was avoided; the claim is not verified here (NEEDS INVESTIGATION).
- The build toolchain (llvm-mingw, `src/build.ps1`) and its licence are not reviewed.

## Known issues

### Q225: game assets, fan project, shader pack licence
- **No game assets in the repository.** The README says so (`README.md:7`). Screenshots and frames are in the repository though: 32 files in `docs/ab/` and 14 entries in `docs/playtest_frames/`. They show the game's art and characters. The record does not state a right to publish them. This is a risk to review (NEEDS INVESTIGATION L-3).
- **Texture list.** `data/texture_tags.csv` contains the names of game texture files and three flags per file. The record calls it the owner's list (`docs/MATERIAL_CLASSIFICATION_PLAN.md:5-7`). Names are not asset content, but they identify game files. Plans say texture names are not to be pasted beyond examples (`docs/PLAN_NEXT.md` rules; plan rule 7).
- **Shader pack.** The research notes say the studied pack's licence (Complementary License 1.7, as named in the note) forbids redistributing parts of it and modifying its code outside normal use. The project's rule is ideas only, with its own implementations (`docs/research/minecraft_shaderpack_port_ranking.md:11-13`). Pack files are not in the repository (`src/light.inl:106` contains only a comment with the word "complementary").
- **Trademarks.** The game's name and character names appear in the README and docs. The README says rights belong to the rights holders (`README.md:258`).

### Q226: LICENSE for the project's own code
Not present. No LICENSE, COPYING, NOTICE or licence header in the tree (checked with `git ls-files`). The README does not state a licence (grep). Owner decision needed: decisions-log O-2 and N-16. Until then, no one may assume a permissive or copyleft licence. (Stated as a fact about the repository, not a legal conclusion.)

### Q227: privacy of the session report
- **Written:** build id and version, preset, report settings, Windows version and build, CPU model and core count, RAM size, display resolution and refresh rate, GPU vendor, model and driver, selected OpenGL limits and extensions, window state, the game's `settings.ini` and the mod's `tyrender.ini` (comments removed), frame times, GL errors, shader failures, settings changes, screenshots, window changes and a session-end line (`src/sesslog.inl:3-6,270-300`).
- **Excluded by the file's own statement:** usernames, computer name, serial numbers, IP and MAC addresses, installed programs, running processes, file listings and personal folder paths (`src/sesslog.inl:277-278`, `README.md:244`). The code calls `SHGetFolderPathA` for the Documents folder only to open the game's settings file (`src/sesslog.inl:296-298`); it does not write that path into the report.
- **Sent anywhere?** No. The code has no network call. A grep of `src` for socket, WinHTTP, WinINet and URL-download APIs found none. The report is written to the logs folder next to the game (`src/sesslog.inl:259-266`). The user sends it by hand.
- **Caveat:** the `settings.ini` contents are copied in full apart from comments. A user who edited that file to include personal data would have it in the report. (inferred)

### Personal data in the repository
The repository itself holds personal data, despite the rules in the plans. Counts only; the values are not reproduced here:
- A Windows user-folder path with a user name appears in `docs/PLAYTEST_2026-10-17_FINDINGS.md` (two mentions).
- The owner's first name appears in `docs/MATERIAL_CLASSIFICATION_PLAN.md` and `docs/PLAYTEST_2026-10-17_FINDINGS.md` (about two mentions).
- Six voice-transcript files are in `docs/recording/` (`mp3.txt`, `mp3_2.txt`, `transcript.txt`, `voice3.txt`, `voice4.txt`, `voiceover.txt`). The plan forbids quoting them, but the findings files quote the owner directly (for example `docs/recording/FINDINGS_ROUND4.md:3`). The rule is not enforced in the docs themselves.
- A memory-service key variable name and host are described in `docs/START_HERE.md` section 5 and 6. The key itself is not in the repository. This page does not reproduce the name or host.
- No e-mail address was found in the repository (grep).
- No secret-like strings were found in source, tools or docs. The matches for "secret" and "token" are a texture name, a repo-sync guard and a lock file's token field (`tools/repo_sync.ps1:20-22`, `tools/playtest/common.ps1:92-114`).
- The IP-like match in `src/installer/uninstall.c:55` is the loopback address used for a delay ping. It is not a host.

Recommendation for the owner (not a decision on this page): scrub the user-folder path and first names from the docs before publishing, and decide the status of the transcripts (decisions-log O-3 related).

## How to verify
- Licence: `git ls-files | grep -i -E 'licen|copying|notice'` returns nothing at 4db1d7d.
- Session report egress: `grep -rn -i 'socket\|winhttp\|wininet\|internetopen\|urldownload' src` returns nothing relevant at 4db1d7d.
- Session report contents: run the game with the report enabled, open the newest `TYHD_report_*.txt` in the logs folder, and check the header block against `src/sesslog.inl:270-278`.
- Personal data: search the docs folder for a Windows user-folder path pattern, and the whole tree for an e-mail address pattern (exact patterns not reproduced here).

## NEEDS INVESTIGATION
- **N-16 Licence of the project's own code** (Q226). Method: owner decision; check the GitHub repository licence field (`gh api repos/<owner>/<repo>/license`) once the repository is public; check whether any LICENSE existed in earlier commits (the clone has one commit).
- **L-1 Redistribution of the compiled DLL and launcher.** Method: legal review after N-16; the README asks users to supply their own game copy (`README.md:7`).
- **L-2 Whether the shader-pack reuse rule was kept.** Method: a line-by-line comparison of the shaders in `src/` against the pack, by someone with access to the pack archive. The pack is not in the repository.
- **L-3 Right to publish game screenshots and frames** (`docs/ab/`, `docs/playtest_frames/`). Method: owner decision; consider removing or reducing them before publishing.
- **L-4 Voice transcripts in the repository** (`docs/recording/*.txt`). Method: owner decision on whether they stay in the repository.
- **L-5 Trademark use in the project name.** Method: legal review.
- **Session report contents against a real `settings.ini`.** Method: open a real generated report with a `settings.ini` containing a non-default value and check that no personal data is in it.

## Discrepancies
1. **"No personal paths" rule vs. repository content.** `docs/START_HERE.md` and `docs/RESUME.md` describe the rule for future docs; the current docs contain the paths and names listed above. The first-pass note in `KNOWN_ISSUES_AND_HISTORY.md` says the docs "do not reproduce" these; that is a claim about this documentation set, not the repository.
2. **Report claims vs. settings copy.** `src/sesslog.inl:277-278` says the report has no personal folder paths. It includes the game's `settings.ini` in full (minus comments), and the folder name appears in the file name of the report only as `TYHD_report_<time>_<build>`. Not a contradiction in practice, but the caveat above applies.
3. **Shader pack licence.** `docs/research/minecraft_shaderpack_port_ranking.md:11` says the agreement treats redistribution as outside normal use. `docs/PLAN_EXEC.md:78` says the agent recorded "all rights reserved, no code reuse" in a status line. The two agree on the rule; the record of the licence name differs in detail between the two notes. Use the licence text, not either summary.

## See also
- [decisions-log.md](decisions-log.md) for the owner decisions O-2 and O-3
- [research-notes.md](research-notes.md) for the research notes and their reuse rule
- [roadmap-and-open-work.md](roadmap-and-open-work.md) for the licence blocker in the release list
- [../reference/settings-system-debug.md](../reference/settings-system-debug.md) for the `[report]` keys
