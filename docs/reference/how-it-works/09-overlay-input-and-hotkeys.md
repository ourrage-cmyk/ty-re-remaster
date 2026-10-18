# Overlay, input and hotkeys

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B.

## Purpose

The DLL reads two keys while the game runs. F10 opens a small settings panel (the in-game overlay) that changes eight `tyrender.ini` values live. F11 writes a pair of screenshots, one with the renderer's extras on and one with them off. Both features read the keyboard by polling, not by hooking window messages. The panel draws over the finished frame and restores the GL state it touched, so the game's own rendering is not changed.

This page explains how the two features work and where their limits are. The full key and file list is in [hotkeys-and-files.md](../reference/hotkeys-and-files.md). The launcher's own keyboard and gamepad handling is in [launcher.md](../reference/launcher.md).

## How it works

The per-frame order is set in `h_SwapBuffers` (`src/tyrender.cpp:591`). The relevant part, with the frame's draws already done:

```
  game draws the frame (renderer passes run inside)
        |
  h_SwapBuffers (tyrender.cpp:591)
        |-- every 8 frames: poll flag files, reload trigger (594-604)
        |-- GProf(9) end-of-frame GPU mark (606)
        |-- ABShotFrame: F11 read-back and PNG (607)   <- reads the frame BEFORE the panel
        |-- OverlayFrame: F10 keys, panel draw (608)
        |-- o_SwapBuffers: the frame is presented (609)
        `-- SessFrame, GProfNextFrame, g_frame++ (610-612)
```

Because the F11 read-back runs before the panel is drawn, the screenshot does not contain the panel (comment at `src/tyrender.cpp:608`).

### F10 settings panel

1. **Configuration.** `LoadOverlayConfig` (`src/overlay.inl:41-45`) reads `[overlay] Key`, default text `"0"` in code. `OvParseVk` (`overlay.inl:37-40`) accepts `F1` to `F24` (virtual keys `0x70` to `0x87`) or a decimal virtual-key number from 1 to 255. Anything else, including hex text such as `0x79`, becomes 0, which turns the feature off.
2. **Key detection.** `OverlayFrame` (`overlay.inl:233-240`) uses `GetAsyncKeyState` with edge detection (a key counts once per press). Only the toggle key is read whether or not the game window has focus (`overlay.inl:239`). The arrow keys, Enter and Esc are read only while the game window is the foreground window (`overlay.inl:236-237`).
3. **Open.** `OvOpen` (`overlay.inl:74-84`) stores the raw text of each of the eight keys as it is in the ini, using the sentinel `"\x01"` for a missing key (`overlay.inl:35`). It then logs `[overlay] opened`.
4. **Change.** Up and Down move the selection (`overlay.inl:246-247`). Left and Right call `OvSetVal` (`overlay.inl:249-250`, `64-73`), which clamps the value to the item's range, rounds integer items, and writes the ini at once (`OvWriteIni`, `overlay.inl:59-63`). Then `OvReload` (`overlay.inl:51-58`) calls the matching loader: `LoadFXConfig` for `post`, `LoadAtmosConfig` for `atmos`, `LoadAOConfig` for `ao`, `LoadTessConfig` for `tess`, `LoadShadowConfig` for `shadows`. It also clears the PBR and tess stamps.
5. **Keep.** Enter calls `OvKeep` (`overlay.inl:95`). This only closes the panel. The values were already written in step 4.
6. **Discard.** Esc or the toggle key calls `OvRevertAndClose` (`overlay.inl:85-94`, call at `243`). For each item whose ini text changed since the panel opened, the original text is written back (the key is deleted if it was missing), and the loader runs again.
7. **Draw.** While the panel is open, `OvDraw` runs every frame (`overlay.inl:253-256`). `OvRender` (`overlay.inl:98-137`) draws the text with GDI into a 470 by 200 pixel DIB (`OV_W`, `OV_H`, `overlay.inl:18-19`) using Consolas 15 px (`overlay.inl:105`). It turns the GDI luminance into alpha and fills the background with a dark colour. `OvDraw` (`overlay.inl:161-230`) then:
   - pushes all GL attribute and client-attribute state (`overlay.inl:206-207`);
   - saves the program, framebuffer, VAO, buffers and active texture by hand, because the attribute stack does not cover them (`overlay.inl:203-205`);
   - unbinds, turns off depth, stencil, scissor and alpha test, turns on blend, and sets the colour mask, the pixel store and the blend function (`overlay.inl:208-218`);
   - calls `glWindowPos2i` at the lower-left area of the window and `glDrawPixels` (`overlay.inl:220-221`);
   - restores the saved bindings and the attribute stacks (`overlay.inl:223-228`).
   The raw entry points are used on purpose (`overlay.inl:171-172`). The `glUseProgram` and `glActiveTexture` wrappers would run the renderer's own state code and change the cached state. The code comment says so directly.
8. **Diagnostics.** For the first three draws, `OvDumpState` logs the GL state before and after the draw as `[overlay-state]` lines (`overlay.inl:197-199`, `229`, `142-159`). The first and the 120th panel frame since game start log `[overlay] drawn frame N` (`overlay.inl:255`; the counter is static and is not reset when the panel is reopened).

The eight items the panel changes (`g_ovItems`, `src/overlay.inl:22-31`):

| Label on panel | ini key | range | step | type |
|---|---|---|---|---|
| Brightness | `[post] Brightness` | 0.50 to 1.50 | 0.02 | float |
| Saturation | `[post] Saturation` | 0.00 to 2.00 | 0.05 | float |
| Sharpen | `[post] Sharpen` | 0.00 to 1.00 | 0.05 | float |
| Vignette | `[post] Vignette` | 0.00 to 1.00 | 0.05 | float |
| Atmos intensity | `[atmos] Intensity` | 0.00 to 4.00 | 0.10 | float |
| AO enable | `[ao] Enable` | 0 to 1 | 1 | integer |
| Tess max level | `[tess] MaxLevel` | 1 to 32 | 1 | integer |
| Shadow taps | `[shadows] Taps` | 4 to 32 | 4 | integer |

The panel's code comment (`overlay.inl:2-6`) describes the same behaviour. The one exception is the key-focus rule, see Discrepancies.

### F11 A/B capture

1. **Configuration.** `LoadShotConfig` (`src/abshot.inl:20-24`) reads `[shots] Key`, default `"F11"` in code. The same parser rules as the overlay apply (`abshot.inl:16-19`).
2. **Trigger.** `ABShotFrame` (`abshot.inl:110-132`) fires on the rising edge of the key (`abshot.inl:114`), and only when the game window is in front (`abshot.inl:116`). The second half of a pair is taken on the next frame (phase 1).
3. **First frame (phase 0).** The timestamp is stored (`abshot.inl:117-118`), the current value of `g_tsLive` is saved (`abshot.inl:120`), and the frame is written with the label `shaders-on` or `shaders-off` from `g_tsLive` (`abshot.inl:121`). Then `g_tsLive` is flipped, `g_abOff` is set when the first frame was "on", and `ABRepush` pushes the new values to every known program (`abshot.inl:122-123`, `102-108`).
4. **Second frame (phase 1).** The frame is written again with its label (`abshot.inl:128`). `g_tsLive` and `g_abOff` go back to their saved values and the programs are updated again (`abshot.inl:129-130`).
5. **What "off" means.** When the first frame was "on", the second frame has `g_abOff` set. Those passes skip: AO (`src/ao.inl:567`), atmosphere (`src/atmos.inl:362`), GI (`src/gi.inl:105`, `src/gi2.inl:39`), emitter light (`src/emit.inl:139`, `227`), post and TAA jitter (`src/fx.inl:162`, `178`), light features (`src/light.inl:109`), and the distance LOD (`src/pbr.inl:185`). The shadow strength falls back to the stock literal (`src/tyrender.cpp:558`). The relief and PBR extras are switched off through `g_tsLive` (`src/pbr.inl:186`, `199-202`). The CHANGELOG lists the same set (`CHANGELOG.md`, "Fixed").
6. **Write.** `ShotCapture` (`abshot.inl:86-98`) reads the default framebuffer with `glReadPixels` in BGR order (`ShotReadFrame`, `abshot.inl:25-41`), encodes an 8-bit RGB PNG (`ShotWritePng`, `abshot.inl:49-84`), and writes it to `<game folder>\Screenshots\ty_<build>_<preset>_<label>_<yyyymmdd-hhmmss>.png` (`abshot.inl:92-93`). The preset comes from `[shots] Preset`, default `"custom"`, with characters other than letters, digits and `-` replaced by `-` (`abshot.inl:88-91`). The write is synchronous, on the frame thread.
7. **Feedback.** Success logs `[shot] <path> WxH frame=N` and a session event `screenshot <file name>` (`abshot.inl:94`). The window title shows `A/B <label> saved` for 2.5 seconds (`abshot.inl:97`, `111`).

The PNG writer uses stored (uncompressed) deflate blocks, so there is no compression library. The file is therefore large (`abshot.inl:48`, `57-65`).

### Harness-only alternative: `[tess] ABToggle`

`[tess] ABToggle=1` (`src/tess.inl:69`, default 0) turns on `TessABPoll` (`src/pbr.inl:206-211`). Every 8 frames it reads `<game folder>\tyrender\cam.txt`. Whenever the content changes, `g_tsLive` flips and `[tess] AB toggle: live=N` is logged. The poll does nothing while an F11 pair is in progress (`pbr.inl:207`). The code comment calls this a test aid (`tess.inl:20`). Users should use F11.

### Keyboard and window-message model

- The DLL never reads keys through a window message. It has no `WndProc` hook and no `SetWindowsHookEx`. The only `WndProc` in the launcher that is built is `src/launcher_ui.inc:397` (`TYLauncher.c` includes only that file; `src/ui_part_c.inc` has a second copy that nothing includes).
- Keys are sampled once per SwapBuffers call (`abshot.inl:114`, `overlay.inl:237-240`). A press shorter than one frame can be missed (inferred from the polling design).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Overlay key parser | `src/overlay.inl:37-40` | F1-F24 or decimal VK 1-255, else 0 |
| Overlay config read | `src/overlay.inl:41-45` | `[overlay] Key`, default `"0"` |
| Overlay item table | `src/overlay.inl:22-31` | eight items |
| Write one value and reload | `src/overlay.inl:59-73` | writes ini at once |
| Open / discard / keep | `src/overlay.inl:74-95` | discard restores only changed keys |
| Panel render (GDI into DIB) | `src/overlay.inl:98-137` | 470 x 200, Consolas 15 |
| Panel GL draw with state save | `src/overlay.inl:161-230` | raw entry points at 171-173 |
| Key polling and state machine | `src/overlay.inl:233-257` | toggle key not focus-gated (239) |
| F11 config | `src/abshot.inl:20-24` | default `"F11"` |
| F11 trigger and phases | `src/abshot.inl:110-132` | two consecutive frames |
| PNG read-back and write | `src/abshot.inl:25-98` | synchronous |
| Off-frame gating | `src/abshot.inl:123`, `src/ao.inl:567`, `src/atmos.inl:362`, `src/gi.inl:105`, `src/fx.inl:162` | `g_abOff` |
| Per-frame order | `src/tyrender.cpp:591-610` | overlay after F11 read |
| `[tess] ABToggle` poll | `src/pbr.inl:206-211` | harness aid |
| Debug camera applied in the VP matrix hook | `src/tyrender.cpp:350-351` | see [flag files](../reference/hotkeys-and-files.md) |
| Shipped defaults | `src/default_tyrender.ini:99` (`[shots] Key=F11`), `:114-115` (`[overlay] Key=F10`) | |
| Installer turns overlay key 0 into F10 | `src/installer/setup.c:60-62` | install time only |

## Settings

| key | default in code | shipped (`default_tyrender.ini`) | live or restart | effect |
|---|---|---|---|---|
| `[overlay] Key` | `"0"` (`overlay.inl:43`) | `F10` (`:115`) | live: read at start and on the reload trigger (`tyrender.cpp:598`) | Key that opens and closes the panel. 0 = off. |
| `[shots] Key` | `"F11"` (`abshot.inl:22`) | `F11` (`:99`) | live: `LoadShotConfig` is on the reload list (`tyrender.cpp:598`) | Key for the A/B pair. 0 = off. |
| `[shots] Preset` | `"custom"` (`abshot.inl:89`) | not in shipped ini; the launcher writes it (`src/launcher_ui.inc:242`) | read at each capture | Part of the screenshot file name only |
| `[tess] ABToggle` | 0 (`src/tess.inl:69`) | not set | read at start and on reload | harness aid, see above |

The eight panel keys are owned by their sections. Their defaults and effects are on the settings pages: [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md), [settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md), [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

## Cost

Not measured.

- Panel open: about 94,000 pixels (470 x 200) are processed on the CPU every frame (`overlay.inl:127-136`), and the text is redrawn each frame (`overlay.inl:98-125`). With the panel closed, the cost is one key-state read per key per frame.
- F11: two full-window `glReadPixels` calls and a CPU PNG encode on the frame thread, in the same frame as the press or the next one. Expect a hitch on those two frames (inferred from the synchronous design). Not measured.

## Limits

- The panel has eight items, no mouse, and no undo beyond Esc. The launcher is the full settings UI ([launcher.md](../reference/launcher.md)).
- Arrow keys also reach the game while the panel is open (`overlay.inl:7`). The code does not swallow keys.
- Key values are only the forms `F1` to `F24` and decimal virtual-key codes 1 to 255. `F25` and hex codes are read as off (`overlay.inl:37-40`).
- The panel and the launcher write the same `tyrender.ini`. The panel writes values one key at a time with `WritePrivateProfileStringA`, so the file is changed on every keypress (`overlay.inl:61`).
- The F11 pair is two consecutive frames, not one frame twice. Scene motion between the two frames is possible.
- The "off" frame is closer to vanilla, not identical: the stock shadow literal is used (`tyrender.cpp:558`), and the game's own pass order is unchanged (CHANGELOG, "Fixed": "closer to vanilla").
- If `g_tsLive` is already 0 when F11 is pressed (for example after an interrupted pair or `ABToggle`), the label of the first frame is `shaders-off` and the pair is not what the name suggests (inferred from `abshot.inl:120-122`).

## Known issues

- **F10 panel breaks geometry and cannot be closed (#12).** Described in `docs/PLAN_NEXT.md:19` for build cda9364. The fixes listed in the CHANGELOG are: explicit draw state, a focus-free toggle (#54, #59, #63), and the default key changed to F10 (#102). The CHANGELOG says "untested in level" (`CHANGELOG.md`, "Overlay fixes").
- **F10 does nothing in build 4a2ef95.** The existing `tyrender.ini` keeps `Key=0`, and the installer converts 0 to F10 only at install time (`src/installer/setup.c:60-62`). Set `[overlay] Key=F10` by hand (README, "Known issues", and `CHANGELOG.md`, "Playtest of 4a2ef95").
- **Live verification of the panel is still open (#79).** `docs/PLAN_NEXT.md` lists it as the first open item.
- **F11 "off" frame (#96).** Listed as untested in the CHANGELOG.

## How to verify

Paths below use `<game folder>` for the folder with `TY_tyrender.exe`.

1. **Panel opens.** Set `[overlay] Key=F10` in `<game folder>\tyrender.ini`. Start the game, press F10 once in a level. Expected: a dark panel at the lower left with eight rows. `<game folder>\tyrender.log` contains `[overlay] opened (build ...)` and, shortly after, `[overlay] drawn frame 1 (front=1)`.
2. **Value is written live.** On the first row press Right once. Expected: `Brightness` moves by 0.02 and `<game folder>\tyrender.ini` has the new `[post] Brightness=` value with two decimals. The session report has a line `setting changed (F10 overlay): post.Brightness = ...` (`src/sesslog.inl:138`, `SessIniCheck`).
3. **Esc restores.** Press Esc. Expected: the ini value returns to its old text, the log has `[overlay] discarded`. Enter instead keeps the value and the log has `[overlay] kept (written to tyrender.ini)`.
4. **Off means off.** Set `Key=0`, reload (`tyrender\reload` or restart). Press F10. Expected: nothing is drawn and no `[overlay]` line appears.
5. **F11 pair.** Press F11 in the foreground window. Expected: two new files in `<game folder>\Screenshots\` named `ty_<build>_<preset>_shaders-on_...png` and `..._shaders-off_...png`, and two `[shot]` lines in the log. The window title flashes `A/B shaders-on saved` for about 2.5 s.
6. **Focus rule.** Click another window and press F11. Expected: no new file, because F11 needs the game window in front (`abshot.inl:116`). Pressing F10 in that state still toggles the panel (see Discrepancies).

## NEEDS INVESTIGATION

- **NI-O1: does the F10 panel work in a level at commit 4db1d7d?** Method: run the automated overlay check in `docs/AUTOPLAYTEST.md` ("Overlay check") with `Key=F10`, or do step 1 above by hand. Issue #79. (Plan item N-24.)
- **NI-O2: what broke the panel in the earlier playtest, and which change fixed it?** The repo does not say. The CHANGELOG gives the issue numbers only. The issue tracker is not in the clone. Method: `gh issue view 12`, `gh issue view 54`, `gh issue view 59`, `gh issue view 63` and the PRs that closed them, or reproduce on an older commit. (Plan item N-19.)
- **NI-O3: does arrow input reach the game while the panel is open?** The code does not block it (`overlay.inl:7`). Method: open the panel in a menu and see whether the menu cursor moves.
- **NI-O4: is the toggle really focus-free on the owner's PC?** The code reads F10 without a focus check (`overlay.inl:239`). Method: press F10 with another window in front and check for `[overlay] opened` in the log.
- **NI-O5: how close is the F11 "off" frame to vanilla?** Untested (#96). Method: compare a pair against a stock-game capture of the same spot (`tools/abshots.ps1`, `tools/match_ab.ps1`). Visual judgement is needed.

## Discrepancies

1. **Focus rule for the toggle key.** `src/overlay.inl:6` says the panel reads keys only while the game window is in front. The code reads the toggle key without a focus check (`overlay.inl:239`). The arrow, Enter and Esc keys do check focus (`overlay.inl:236-237`). CHANGELOG ("Overlay fixes") says the toggle is focus-free. Code wins: F10 works without focus.
2. **Where the values are written.** First-pass `OVERLAY_AND_HOTKEYS.md` section 2 says Enter "keeps the value and writes it to tyrender.ini". Code: each Left or Right writes the ini at once (`overlay.inl:64-73`). Enter only closes the panel (`overlay.inl:95`).
3. **F11 pair frames.** First-pass `OVERLAY_AND_HOTKEYS.md` sections 3 and 8 say the pair is "for the same frame". Code: two consecutive frames (`abshot.inl:113-131`).
4. **Preset name when the key is absent.** First-pass section 3 says the name is `Custom`. Code uses lowercase `custom` when the key is missing (`abshot.inl:89`, `91`). The launcher writes `Custom` with a capital C (`src/launcher_ui.inc:242`), so normal file names use `Custom`.
5. **Panel label.** CHANGELOG (Added, F10 overlay) says the panel controls "fog intensity". The item is `[atmos] Intensity`, labelled "Atmos intensity" (`overlay.inl:27`).
6. **Cited "Overlay breaking" section.** The plan (Q111) cites `docs/PLAYTEST_2026-10-17_FINDINGS.md` section "Overlay breaking". That section (line 119) measures a pixel-change metric for the water and overlay layers. It does not discuss the F10 panel. Do not use it as evidence for issue #12.
7. **Key-format question.** `docs/AUTOPLAYTEST.md:59` asks to confirm the overlay key format. The parser accepts `F10` (`overlay.inl:38`). Resolved by reading the code.

## See also

- [hotkeys-and-files.md](../reference/hotkeys-and-files.md): every key, flag file and environment variable.
- [10-logging-timers-and-session-report.md](10-logging-timers-and-session-report.md): the log lines the panel and the F11 key write.
- [session-report.md](../guides/session-report.md): the report that records overlay and screenshot events.
- [settings-system-debug.md](../reference/settings-system-debug.md): `[shots]`, `[overlay]`, `[report]`.
- [debugging.md](../guides/debugging.md): using the debug views with the panel and the F11 pair.
