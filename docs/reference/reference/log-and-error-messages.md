# Log and error messages

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B.

## Purpose

Every line the renderer writes to `<game folder>\tyrender.log`, and every event it writes to the session report, grouped by tag. For each one: the text pattern, where it is written, what it means, and what to do with it. Use this page when a log line is unclear. For how the log and report are produced, see [10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md). For what to send with a bug, see [session-report.md](../guides/session-report.md).

Notes on the tables:

- `%d`, `%s`, `%u` and similar are printf placeholders. `...` means more text follows in the source.
- "Every frame" and "every 600 frames" describe how often a line is written. "Once" means a static flag stops it after the first time.
- The report column says whether the same event also goes to `TYHD_report_*.txt` (session event). Events go to the report through `SessEvent` or `SessFailure`; all other lines are log-only.
- The code writes `logf` output with no timestamp. The report's events carry a `[mm:ss.mmm fN]` prefix.

## Start-up and hooks

| Pattern | Where | Meaning | What to do |
|---|---|---|---|
| `tyrender stage1 loaded. game dir ... capture=a,b dump=n override=n` | `src/tyrender.cpp:726` | The DLL loaded. The line gives the game folder path (a personal path on most PCs). | Remove the path before posting the log publicly. |
| `[iat] NOT FOUND <dll>!<function>` | `src/tyrender.cpp:127` | A hook target was not in the game's import table. | The function is not hooked. Check the game binary and the patch (see [02](../how-it-works/02-loading-and-injection.md)). |
| `[iat] <name> orig=<ptr> module=<path>` | `src/tyrender.cpp:680` | Sanity check of the saved original pointer for `SwapBuffers`, `wglGetProcAddress` and `glDrawElements`. The module should be a real DLL path. `module=?` means the import was not resolved. | If `?`, the hook chain is broken; the game may not render through the DLL. |
| `hooks installed` | `src/tyrender.cpp:732` | End of `DllMain` start-up. | Normal. |
| `[proc] <name> -> hooked` | `src/tyrender.cpp:463` | A `wglGetProcAddress` result was replaced with a hook. | Normal. |
| `[proc] <name> -> <ptr>` | `src/tyrender.cpp:466` | A `wglGetProcAddress` result was returned unchanged. | Normal for functions the DLL does not hook. |
| `wglCreateContext dc=<ptr> -> <ptr>` | `src/tyrender.cpp:486` | A GL context was created. | Normal. |
| `wglMakeCurrent dc=... ctx=... -> <bool>` | `src/tyrender.cpp:491` | Context switch. | Normal. |
| `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, `GLSL` | `src/tyrender.cpp:495-498` | The driver strings, logged once. | Put the renderer string in the bug report. |
| `[clash] no game settings.ini at <path>` | `src/tyrender.cpp:643` | The clash check could not find the game's `settings.ini`. The line contains the full Documents path. | Remove the path before posting. Usually harmless. |
| `[clash] settings.ini [Display] SSAO=true; ...` | `src/tyrender.cpp:657` | The game's own SSAO is on. The menu can stall. | Set `SSAO=false` in the game's `settings.ini`. |
| `[clash] settings.ini [Display] MSAA=<v> and tyrender Edge smoothing (post AA=n) both smooth edges. ...` | `src/tyrender.cpp:658` | Double smoothing. | Turn off one of them. |
| `[cfg] reloaded all` | `src/tyrender.cpp:603` | The reload trigger was handled. Session event: `setting ... (reload)` lines follow. | Normal after a launcher or overlay change. |
| `[frame N] avg X ms/frame (Y fps), draws last frame=n, tess progs=n, skipped non-triangle tess draws=n` | `src/tyrender.cpp:618` | Every 600 frames. Wall-clock average. | Use for a quick speed check. Not a GPU time. |
| `=== BEGIN CAPTURE frame N ===` | `src/tyrender.cpp:628` | A call-by-call capture starts. | Normal during `cap.req` or `CaptureFrame`. |
| `=== END CAPTURE frame N (draws=n) ===` | `src/tyrender.cpp:592` | The capture ends. The log is flushed. | Normal. |
| `[snap] <path> WxH frame=N` | `src/tyrender.cpp:537` | A `snap.req` snapshot was written. | Normal. The path is full. |
| `[dcam] camera offset X Y Z yaw A pitch B` | `src/debugcam.inl:11` | `cam.txt` was read and the offset changed. | Normal in test runs. |
| `[dcam] off` | `src/debugcam.inl:12` | `cam.txt` is gone. | Normal. |
| `[motion] request seen: <text>` | `src/debugcam.inl:49` | A `motion.req` file was read. | Normal in test runs. |
| `[motion] start <name> step ... deg/frame frames n` | `src/debugcam.inl:56` | Motion capture started. | Normal. |
| `[motion] done <name> -> <path>` | `src/debugcam.inl:74` | Motion capture finished and the `.raw` file was written. | Normal. |
| `[fix] skipped out-of-bounds glReadPixels x,y WxH fmt=... (window WxH) frame=N` | `src/tyrender.cpp:311` | A read-back outside the window was skipped. First 20 only. | A read outside the window would have been an error; the count shows how often the game asks. |
| `[shadows] shadow map enlarged to WxH` | `src/tyrender.cpp:274` | The shadow map was made larger by `MapScale`. | Normal when `[shadows] MapScale` is above 1. |
| `[stencil] STENCIL_BITS=n engine glEnable(STENCIL_TEST) calls so far=n` | `src/ao.inl:722` | Once, at frame 300. | Information only. |

## Hook trace lines (only during a capture, or early frames)

These lines appear for every matching call, which can be many per frame. They are written during a capture (`g_cap`), and some of them also when a condition is met as noted.

| Pattern | Where | When written | Meaning |
|---|---|---|---|
| `glTexImage2D tex=n t=... lvl=n ifmt=... WxH fmt=... type=... data=yes/null frame=N` | `src/tyrender.cpp:276` | during a capture, or whenever the upload has no pixel data (`data=null`) | Texture upload. A `null` upload is common and is not an error. |
| `glCopyTexImage2D t=... ifmt=... src=x,y WxH frame=N` | `src/tyrender.cpp:281` | during a capture, or the first 3 frames | Colour or depth copy, the frame anchors in [04](../how-it-works/04-frame-anatomy-and-pass-order.md). |
| `glFramebufferTexture2D t=... att=... textarget=... tex=n lvl=n frame=N` | `src/tyrender.cpp:321` | during a capture, or the first 3 frames | Attachment change. |
| `glFramebufferRenderbuffer t=... att=... rb=n frame=N` | `src/tyrender.cpp:323` | always | Attachment change with a renderbuffer. |
| `glRenderbufferStorage fmt=... WxH` | `src/tyrender.cpp:324` | always | Renderbuffer allocation. |
| `glRenderbufferStorageMultisample samples=n fmt=... WxH` | `src/tyrender.cpp:326` | always | MSAA buffer allocation. |
| `glAttachShader prog=n shader=n` | `src/tyrender.cpp:332` | always | Shader attach. |
| `glLinkProgram n` | `src/tyrender.cpp:333` | always | Link. Normal. |
| `uniform prog=n '<name>' -> loc` | `src/tyrender.cpp:337` | once per program and name | Uniform lookup. Useful when checking a shader uniform. |
| `glShaderSource shader=n hash=... [TESS VS]` | `src/tyrender.cpp:382` | always | A vertex shader was rewritten for tessellation. |
| `glShaderSource shader=n hash=... [PBR VS]` | `src/tyrender.cpp:383` | always | A vertex shader was rewritten for PBR. |
| `glShaderSource shader=n hash=... [FS:<PBR SHADOW LIGHT DECAL>]` | `src/tyrender.cpp:390` | always | A fragment shader was rewritten; the tags say which rewrites were applied. |
| `glShaderSource shader=n type=vs/fs/xs hash=... len=n [OVERRIDE]` | `src/tyrender.cpp:399` | always | Final source handed to GL. `[OVERRIDE]` means an `override\` file was used. |
| `[pbr] VS <hash> rewrite failed to compile` | `src/tyrender.cpp:380` | on failure | The PBR vertex rewrite did not compile; the original is used. |
| `[fs] <hash> combined rewrite failed to compile (pbr=n shadow=n light=n) -> trying shadow patch alone` | `src/tyrender.cpp:392` | on failure | The fragment rewrite fell back to the shadow patch. |
| `override for <hash> does not compile -> original used` | `src/tyrender.cpp:398` | on failure | Bad `override\` file. Delete or fix it. |
| `[compile test] FAILED:` then the driver's info log | `src/tyrender.cpp:479` | on failure | A test compile failed. Session event: `shader failure (compile test)`. |
| `glUniformMatrix4fv loc=n count=n` | `src/tyrender.cpp:346` | during a capture | Captured call. Only inside `=== BEGIN/END CAPTURE` blocks. Other hooks use the same `CAP(...)` macro (`src/tyrender.cpp:50`). |

## Crash lines

| Pattern | Where | Meaning | What to do |
|---|---|---|---|
| `!!! EXCEPTION <code> at <addr> (<module full path> +0x<off>) frame=N stage=<s> prog=n tessProg=n fbo=n` | `src/tyrender.cpp:692` | First fatal exception. `stage=` is always `init` (see below). | Note the code and the module. Send the log. |
| `    access read|write address <addr>` | `src/tyrender.cpp:693` | Access violation details. | Note whether it was a read or a write. |
| `    #n <addr> <module base name> +0x<off>` | `src/tyrender.cpp:695` | Up to 24 stack frames. | Look up the module offset. |

`stage=` is always `init`. The variable `g_lastStage` is set once (`src/tyrender.cpp:684`) and never changed. Session report: `CRASH exception <code> in <module base name> +0x<off>` and `session end: crash` (`src/sesslog.inl:365`, `359`).

## Textures (`src/tex.inl`)

| Pattern | Where | When written | Meaning |
|---|---|---|---|
| `[tex] tags: n embedded, n overrides from file (enable=n)` | `src/tex.inl:101` | at start-up | How many texture tags are built in, and how many came from `texture_tags.csv`. |
| `[tex] MISS id=n px=... nearest=...+n <name> ringW=n` | `src/tex.inl:185` | `[tex] Debug=1`, when a texture name is not found | Name lookup failed. Not an error on its own. Count these for unknown textures. |
| `[tex] upload id=n WxH ifmt=... bytes=n px=... name=... flags=n` | `src/tex.inl:195` | `[tex] Debug=1`, first 500 uploads | Upload with its resolved name. |
| `[spill] additive draws=n tex=n name=... emissive=0/1 (last 600 frames)` | `src/tex.inl:266` | every 600 frames, for additive draws | Additive (blended) textures, the usual source of emitter light and glow. |
| `[tex] stats frame=n reads=n uploads(with data)=n named=n tagged=n \| relief-program draws ...` | `src/tex.inl:269` | every 600 frames | Texture-tag coverage. |
| `[tex] untagged draws=n id=n name=... <info>` | `src/tex.inl:275` | every 600 frames, top 25 | Textures drawn without a tag. Candidates for `texture_tags.csv`. |

## Settings and shader setup

| Pattern | Where | When written | Meaning |
|---|---|---|---|
| `[light] config (<ini path>): view=n plane=n clamp=x slices=n mapscale=n` | `src/light.inl:27` | every config load | Light and shadow settings in effect. Note that the `slices` value is read from `[ao]`. |
| `[light] fog=r,g,b -> shade tint r,g,b` | `src/light.inl:118` | once | Fog colour and the shade tint derived from it. |
| `[fallback] FORCED invalid light shader rewrite -> must fall back` | `src/light.inl:36` | `[debug] ForceFallback` bit 16 | Test of the fallback path. |
| `[dbgwater] term OFF: <name> (DebugWater bit n)` | `src/pbr.inl:25` | `[pbr] DebugWater` set | A water term is switched off for a test. |
| `[pbr] prog n locs cam<n> sun<n> pbr<n> pbr2<n> pbr3<n>` | `src/pbr.inl:173` | first 6 programs | Uniform locations for PBR. |
| `[cam] pos=x,y,z forward=x,y,z` | `src/pbr.inl:229` | every 300 frames | Camera position used by PBR. Test views. |
| `[tess] AB toggle: live=0/1` | `src/pbr.inl:210` | `[tess] ABToggle=1`, when `cam.txt` changes | Relief and PBR extras switched. |

## Shader failures and fallbacks

| Pattern | Where | Session event | Meaning | What to do |
|---|---|---|---|---|
| `[ao] missing gl<name>` | `src/ao.inl:45` | no | A GL entry point needed by AO is missing; AO is off. | Check GL version (the AO page lists the needs). |
| `[ao] engine did not request a needed GL entry point` | `src/ao.inl:50` | no | AO setup failed. | Same as above. |
| `[ao] <program> vs/fs compile FAILED:` then the info log | `src/ao.inl:450` | `shader failure (ao compile)` | An AO shader did not compile. | Send the log and the shader hash. |
| `[ao] <program> link FAILED:` then the info log | `src/ao.inl:455` | `shader failure (ao link)` | An AO program did not link. | Same. |
| `[fallback] FORCED compile failure of AO v2 program <name>` | `src/ao.inl:440` | no | `[debug] ForceFallback` bit 4. | Test only. |
| `[fallback] FORCED compile failure of GI program <name>` | `src/ao.inl:441` | no | `[debug] ForceFallback` bit 2. | Test only. |
| `[fallback] AO v2 unavailable -> AO OFF (no legacy fallback)` | `src/ao.inl:546` | `AO turned off: v2 shader unavailable` | AO is off for the session. | Check the GL version and the shader log. |
| `[ao] v2 (depth-mip GTAO) unavailable, using legacy GTAO (Legacy=1)` | `src/ao.inl:547` | no | Legacy AO path in use. | Information. |
| `[ao] initialised WxH (half WxH)` | `src/ao.inl:560` | no | AO buffers allocated. | Information. |
| `[ao] config: debugview=n tmode=n blend=x fogfade=x wide=x` | `src/ao.inl:516` | every config load | AO settings in effect. | Information. |
| `[ao] debug view a -> b` | `src/ao.inl:521` | when `ao_debug` changes | AO debug view switched. | Information. |
| `[ao] config reloaded: radius=... strength=... power=... slices=n steps=n zmode=n` | `src/ao.inl:523` | on `ao_reload` | AO config re-read. | Information. |
| `[gi] prof [<variant>]: pyramid ... trace ... temporal ... atrous ... composite ... \| total ... ms` | `src/gi2.inl:87` | every 300 samples (100 in sweep mode) | GI pass timing for the Hi-Z path. | Use for GI cost. Not a fault. |
| `[gi] source mask: n marked draws this frame, n total, copy n` | `src/gi2.inl:428` | every 600 frames | Source mask counts. | Information. |
| `[gi] Hi-Z disabled: glDrawBuffers missing` | `src/gi2.inl:353` | once | Hi-Z path cannot run. | Check GL version. |
| `[gi] Hi-Z shaders failed, using legacy path` | `src/gi2.inl:362` | once | Hi-Z shader set did not build. | See Discrepancies: in the default mode the result is GI off. |
| `[gi] Hi-Z framebuffers incomplete, using legacy path` | `src/gi2.inl:387` | once | Hi-Z framebuffer check failed. | Same as above. |
| `[gi] Hi-Z initialised WxH, n pyramid levels` | `src/gi2.inl:388` | once per size | Hi-Z buffers allocated. | Information. |
| `[gi] Hi-Z path first run ok (rays=n iters=n atrous=n)` | `src/gi2.inl:513` | once | Hi-Z GI ran. | Information. |
| `[gi] initialised WxH` | `src/gi.inl:146` | per size | Legacy GI buffers allocated. | Information. |
| `[gi] first run ok` | `src/gi.inl:199` | once | Legacy GI ran. | Information. |
| `[fallback] Hi-Z GI unavailable -> GI OFF (no legacy fallback)` | `src/gi.inl:130` | once | GI is off for the session when Mode is not 0. | Check the shader log for the Hi-Z failure. |
| `[fallback] FORCED tessellation stage compile failure -> ...` | `src/tess.inl:382` | once | `[debug] ForceFallback` bit 8. | Test only. |
| `[tess] GL entry points missing (shader compile/query/detach) -> tessellation off` | `src/tess.inl:92` | once | Tess cannot run. | Check GL version. |
| `[tess] stage <type> compile FAILED:` then the info log, `--- source ---`, and the full shader source | `src/tess.inl:387` | not a session event | A tessellation stage did not compile. The whole rewritten source follows, which is long. | Send the log. Do not post the source publicly. |
| `[tess] GL below 4.0: tessellation stages skipped (TessVersionGate=1)` | `src/tess.inl:397` | once | GL is below 4.0; tess is skipped. | Expected on old GPUs. |
| `[tess] program n linked WITH tessellation` | `src/tess.inl:420` | per program | Tess link worked. | Information. |
| `[tess] program n tessellated link FAILED -> restoring original:` then the info log | `src/tess.inl:421` | per program | Link failed; the program was relinked without tess. | Send the log. |
| `[atmos] froxel: GL <v> lacks compute, using ray-march path` | `src/atmos.inl:260` | once | GL is below 4.3; compute froxels are not used. | Expected on old GPUs. |
| `[atmos] froxel: missing entry points` | `src/atmos.inl:264` | once | Compute entry points missing. | Check GL. |
| `[atmos] froxel compute compile FAILED:` then the info log | `src/atmos.inl:267` | `shader failure (atmos compile)` | The froxel shader did not compile. | Send the log. |
| `[atmos] froxel compute link FAILED:` then the info log | `src/atmos.inl:270` | `shader failure (atmos link)` | The froxel program did not link. | Send the log. |
| `[atmos] froxel compute path ready` | `src/atmos.inl:271` | once | Compute froxels are running. | Information. |
| `[atmos] froxel 3D texture allocation failed (WxHx64)` | `src/atmos.inl:280` | once | Not enough memory or unsupported size. | Lower the resolution or check VRAM. |
| `[atmos] froxel volume WxHx64` | `src/atmos.inl:283` | per size | Froxel volume size. | Information. |
| `[fallback] froxel unavailable -> atmosphere OFF (no heavy ray-march fallback)` | `src/atmos.inl:371` | `atmosphere turned off: froxel unavailable` | Atmosphere is off, when contact shadows are also off. | See the atmosphere page. |
| `[fallback] FORCED froxel failure -> ray-march path` | `src/atmos.inl:257` | once | `[debug] ForceFallback` bit 1. | Test only. |
| `[atmos] half-res target WxH` | `src/atmos.inl:485` | per size | Half-resolution target allocated. | Information. |
| `[fogdiag] transparent-tagged draws (sampled): ...` | `src/atmos.inl:359` | every 600 frames | Where transparent draws sit relative to the fog pass. | Diagnostic for tree fog (#4). |
| `[emit] enabled: strength ... radius ... spread ... compensate n max n` | `src/emit.inl:42` | when enabled | Emitter settings. | Information. |
| `[emit] shaders or GL entry points missing, emitter light off` | `src/emit.inl:109` | once | Emitter light is off. | Check GL. |
| `[emit] emission framebuffer incomplete, emitter light off` | `src/emit.inl:127` | once | Emitter light is off. | Check GL. |
| `[emit] emission buffer WxH (depth tested)` | `src/emit.inl:128` | per size | Emitter buffer allocated. | Information. |
| `[emit] occlusion query entry points missing: ranking off, draw order cap` | `src/emit.inl:146` | once | Emitter ranking is off. | Check GL. |
| `[fx] initialised WxH` | `src/fx.inl:195` | per size | Post buffers allocated. | Information. |
| `[fx] first run ok at frame N` | `src/fx.inl:261` | once | Post ran. | Information. |
| `[state] StateGuard ON: logging GL state leaked by custom passes` | `src/stateguard.inl:32` | when `[debug] StateGuard` is set | State guard is on. | Information. |
| `[state] LEAK after <pass>: <key> a -> b (total leaks this pass so far: n)` | `src/stateguard.inl:39` | first 24 per pass | A custom pass left GL state changed. | Fix the pass; see the debugging guide. |
| `[shot] no frame to read` | `src/abshot.inl:87` | when F11 is pressed | The read-back failed. | Check that the window is visible. |
| `[shot] <path> WxH frame=N` | `src/abshot.inl:94` | on success | Screenshot written. | Information. |
| `[shot] could not write <path>` | `src/abshot.inl:95` | on failure | File could not be written. | Check the folder is writable. |
| `[overlay] opened (build <id>)` | `src/overlay.inl:83` | when the panel opens | Panel opened. | Information. |
| `[overlay] discarded` | `src/overlay.inl:93` | Esc or toggle | Changes reverted. | Information. |
| `[overlay] kept (written to tyrender.ini)` | `src/overlay.inl:95` | Enter | Panel closed with changes kept. | Information. |
| `[overlay] draw skipped: missing GL entry point (wp=... pd=...)` | `src/overlay.inl:194` | once | Panel cannot draw. | Check GL. |
| `[overlay] drawn frame N (front=0/1)` | `src/overlay.inl:255` | frames 1 and 120 of the panel | Panel is drawing. | Information. |
| `[overlay-state] <tag> program=... drawFB=... ... glError=0x...` | `src/overlay.inl:158` | first 3 panel draws, before and after | GL state dump for the panel. | Compare before and after if the panel breaks geometry. |

## Frame, GPU and report lines

| Pattern | Where | When written | Meaning |
|---|---|---|---|
| `[gpu] frame X ms \| ao X gi X atmos X post X \| engine+rest X ms (n draws)` | `src/gpuprof.inl:37` | every 300 GPU samples | GPU pass averages in milliseconds. |
| `[gpusub] sub0 X sub1 X sub2 X sub3 X sub4 X \| emit X` | `src/gpuprof.inl:38` | same | Sub-stage averages. The names sub0 to sub4 are not defined in the code (see [10](../how-it-works/10-logging-timers-and-session-report.md)). |
| `[frame N] ...` | `src/tyrender.cpp:618` | every 600 frames | See above. |

Session report lines are in the next section.

## Session report events

These go to `<game folder>\tyrender\logs\TYHD_report_*.txt`, with a `[mm:ss.mmm fN]` prefix (`src/sesslog.inl:56-63`). Events come from `SessEvent`, `SessEventForce` (always written, even when the file is nearly full) and `SessFailure`.

| Event text | Where | Meaning |
|---|---|---|
| `F10 overlay opened` | `src/overlay.inl:83` | Panel opened. |
| `setting added (<why>): <section.key> = <value>` / `changed` / `removed` | `src/sesslog.inl:138`, `141` | An ini key changed since the last check. `<why>` is `reload` (`src/tyrender.cpp:603`, which the launcher triggers through the reload file) or `F10 overlay` (`src/overlay.inl:62`). At most 20 per write. |
| `... n more setting changes not listed` | `src/sesslog.inl:142` | More than 20 changes. |
| `GL error 0x<code> <name> (later repeats are counted, see session end)` | `src/sesslog.inl:102` | First time each GL error code is seen. |
| `GL error total 0x<code> <name>: n` | `src/sesslog.inl:357` | Totals at session end. |
| `shader failure (<kind>), first time: <text>` | `src/sesslog.inl:74` | First time each distinct shader failure. Text is cut to 160 characters. |
| `shader failure total <kind>: n` | `src/sesslog.inl:358` | Totals at session end. |
| `window WxH windowed/fullscreen -> WxH windowed/fullscreen` | `src/sesslog.inl:339` | Window size or mode changed. |
| `screenshot <file name>` | `src/abshot.inl:94` | F11 file written. |
| `AO turned off: v2 shader unavailable` | `src/ao.inl:546` | See above. |
| `atmosphere turned off: froxel unavailable` | `src/atmos.inl:371` | See above. |
| `capture window closed after N s, only events from here` | `src/sesslog.inl:209` | Frame-time window ended. |
| `session end: <clean exit\|crash>. frames n, GL errors n, shader failures n, file n bytes` | `src/sesslog.inl:359` | Last line of a normal or crashed session. |
| `CRASH exception <code> in <module base name> +0x<off>` | `src/sesslog.inl:365` | Crash, see above. |

Report sections (headings in the file, in order): `== system ==` (`sesslog.inl:286`), `== graphics ==` (`287`), `== window ==` (`291`), `== game settings.ini (...) ==` (`294`), `== tyrender.ini, effective values as written ... ==` (`312`), `== frame times, one row per second, then a summary ==` (`322`), `== performance summary: capture window, N s, N frames ==` (`173`), `== events after the capture window ==` (`187`). When the window is off: `capture window is off ([report] CaptureSeconds=0)` (`324`). When the size cap is reached: `[cap] size limit reached, further events are dropped` (`44`).

## Discrepancies

1. **GI fallback message.** `src/gi2.inl:362` and `387` say "using legacy path". In the default mode (`[gi] Mode=1`), `src/gi.inl:130` logs `GI OFF (no legacy fallback)` when the Hi-Z path fails. The message at 362 and 387 is accurate only when Mode is 0 (the legacy path is then used). Code wins: check the mode before reading these lines.
2. **Atmosphere fallback message.** `src/atmos.inl:257` and `260` say "ray-march path". Line 371 turns atmosphere off instead, but only when contact shadows are off (`g_atContact <= 0`). With contact shadows on, `InitVolume` is not required. The messages are right for their own case; the outcome depends on `[atmos] ContactShadows`. Inferred from the condition at line 371.
3. **`stage=` in the crash line.** Written as if it showed the last stage; it always prints `init` (`src/tyrender.cpp:684`).
4. **Paths in `tyrender.log`.** The start-up line (`src/tyrender.cpp:726`), the clash lines (`643`), the `[iat]` lines (`680`), the crash module path (`692`), and the `[shot]` and `[snap]` lines (`abshot.inl:94`, `tyrender.cpp:537`) all contain full local paths. The report file does not. The launcher's "Report a problem" text says the report has no personal paths (`src/launcher_ui.inc:479`), which is true of the report but not of the log.
5. **`[gpu]` cadence.** `src/gpuprof.inl:3` says every 300 frames. Code: every 300 samples (`gpuprof.inl:35`).
6. **Shader failure events.** Only AO, atmosphere and the compile test write `shader failure` events. Tessellation and Hi-Z GI set-up failures are log-only (`src/tess.inl:387`, `421`; `src/gi2.inl:362`, `387`) (inferred from the call sites).
7. **Log line text in the plan.** Plan item Q122 mentions "PERF lines". No `PERF` line exists in `src/`.

## NEEDS INVESTIGATION

- **NI-LG1: the meaning of the `[gpusub]` names.** The code only has numbers. Method: confirm the mark-to-pass mapping in [10](../how-it-works/10-logging-timers-and-session-report.md) by a capture (`cap.req`) and a `[gpusub]` comparison.
- **NI-LG2: which `[tex]` names are missing from `texture_tags.csv`.** Method: run with `[tex] LogUnknown=1` and `Debug=1` and count `MISS` lines (plan item N-27).
- **NI-LG3: whether any shader failure is missing from the report.** Method: grep each compile path for `SessFailure` against `logf` (done here for the main ones); a full audit needs each module reviewed.

## See also

- [10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md): how the log and report are produced.
- [session-report.md](../guides/session-report.md): how testers read and send the report.
- [hotkeys-and-files.md](hotkeys-and-files.md): the trigger files that produce many of these lines.
- [debugging.md](../guides/debugging.md): using these lines in a bug hunt.
- [known-issues.md](../decisions/known-issues.md): open bugs that show up in the log.
