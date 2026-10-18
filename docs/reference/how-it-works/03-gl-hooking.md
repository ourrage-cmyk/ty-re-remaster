# GL hooking

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), C (reviewer). Full list: [hooks-reference](../reference/hooks-reference.md).

## Purpose

This page explains how the DLL sees the game's OpenGL calls and what it does with them. The DLL wraps the calls the engine makes: it forwards each call to the real driver function, and does extra work at a few chosen points. This is how the render passes know when the scene depth is ready, which draws are which material, and what the matrices are.

There are two hook layers. Both work by replacing a function pointer, never by writing machine code into the game.

## How it works

### 1. Layer one: import address table (IAT)

1. A game call to an imported function goes through a slot in the import address table (IAT). The DLL finds the slot in the game's own import table and writes its own function into the slot (`src/tyrender.cpp:108-129`, `PatchIAT`). Only the main module is searched: `GetModuleHandleA(nullptr)` (`src/tyrender.cpp:663`). A function the game gets later with `LoadLibrary` or `GetProcAddress` has no slot in that table and is not patched (inferred; see Limits).
2. `PatchIAT` matches the DLL name without regard to case (`src/tyrender.cpp:113`, `_stricmp`) and the function name exactly (`:119`, `strcmp`). Ordinal imports are skipped (`:117`). The original pointer is saved (`:121`) before the slot is replaced (`:122`).
3. The only memory write is to the slot, and the slot's page protection is changed only around that write (`:120`, `:123`). Nothing writes machine code. This is the basis for the "no inline hooks" statement in the file header (`src/tyrender.cpp:4`). The grep for `VirtualProtect` in `src/` finds only these two calls and no jump or trampoline bytes.
4. `Install` patches 17 functions from OPENGL32 (14 GL functions and `wglGetProcAddress`, `wglCreateContext`, `wglMakeCurrent`), and `SwapBuffers` from GDI32 (`src/tyrender.cpp:662-669`). `TexInstallHooks` adds four KERNEL32 file functions (`src/tex.inl:166-171`).
5. Each hook calls the saved original through an `o_` pointer. The `HOOK` macro defines the hook, its original pointer and the typedef in one line (`src/tyrender.cpp:250`). Example: `o_glViewport` is called at `src/tyrender.cpp:256`.

### 2. Layer two: wglGetProcAddress

OpenGL functions newer than version 1.1 are not exported by `opengl32.dll` (inferred from the design; the hook list depends on it). The engine gets them by calling `wglGetProcAddress`. The DLL hooks that call too.

1. `h_wglGetProcAddress` calls the real function (`src/tyrender.cpp:458`).
2. If the name is in the table `g_procHooks` (`:438-455`), it saves the real pointer in the `o_` variable (only the first time, `:462`), logs `[proc] <name> -> hooked` (`:463`), and returns the DLL's own function (`:464`).
3. Other names return the real pointer unchanged (`:466-467`).
4. The table has 18 functions. Each has an alias: `EXT` for the framebuffer and renderbuffer family, `ARB` for the texture and uniform family, and `ObjectARB` for the shader functions (`:439-454`, macros at `:436-437`).

The DLL's own calls to these functions use the real `wglGetProcAddress` (for example `src/ao.inl:37-41`, `GLProc`), so the DLL does not call its own hooks.

Timing (inferred): the engine asks for extension names after it creates its context, and that happens after DllMain has run. So the engine normally receives hooked pointers. A name asked for before `Install` would get the real pointer and no hook. `CheckOrig` only logs the saved originals; it does not repair anything (`src/tyrender.cpp:677-681`).

How a pass gets GL pointers of its own: `src/ao.inl` keeps a typed table (`ao.inl:7-25`) and a struct of pointers (`ao.inl:28-35`). `GLProc` asks the real `wglGetProcAddress` and, when that returns one of the failure codes 1, 2, 3 or -1, asks `GetProcAddress` on `opengl32.dll` (`ao.inl:37-41`). `LoadGX` fills the struct once, and returns false, with a log line, if a needed entry point is missing or if the engine never requested one of the hooked functions the passes need (`ao.inl:42-51`). Other pass files use their own copies of the same pattern, with `o_wglGetProcAddress` directly (`src/gpuprof.inl:11`, `src/tess.inl:90`, `src/pbr.inl:179`).

### 3. The flow of one call

```
 engine code
    |
    |  glDrawElements(...)   (called through the IAT slot)
    v
 h_glDrawElements  (src/tyrender.cpp:267)
    |  bookkeeping, material uniforms, tess check, pass-specific draws
    v
 o_glDrawElements  (driver)

 engine code
    |
    |  wglGetProcAddress("glUniform4f")
    v
 h_wglGetProcAddress  (src/tyrender.cpp:457)
    |  real = o_wglGetProcAddress(name)
    |  name in g_procHooks? yes -> store real in o_glUniform4f, return h_glUniform4f
    v
 engine code now calls h_glUniform4f, which forwards to o_glUniform4f
```

### 4. Logging

- `logf` writes a line and flushes the file, unless a capture is running (`src/tyrender.cpp:44-49`).
- `CAP` writes only while `g_cap` is set (`:50`). `g_cap` is set for the frame chosen by `[debug] CaptureFrame`, or by the `cap.req` file (`:622-627`).
- Outside a capture, the per-call log lines cost one flag check each. Lines written only at start-up or once per name are not in the hot path: for example `glGetUniformLocation` logs each new (program, name) pair once (`:336-337`).

### 5. How draws are found

The per-draw hooks are `glDrawElements` and `glDrawArrays` (`src/tyrender.cpp:267-271`). Each one:

1. counts the draw (`g_draws`, `:267`, `:270`);
2. sets material uniforms for the bound texture (`TexDrawPrep`, `:267`, `:270`; details on [texture names and tags](07-texture-names-and-tags.md));
3. replaces the draw mode when the program is tessellated: only triangles are accepted, and the mode is changed to `GL_PATCHES` (`:268`, `:271`); any other draw in such a program is dropped and counted (`g_tessBadDrawsExt`);
4. binds pending shadow textures (`:269`);
5. calls the see-through depth and late-fog hooks around the draw (`:269`);
6. when `GIMaskBegin` or `EmitBegin` returns true for this draw, issues the same draw again with the GI source mask or the emitter setup in place (`:269`, `:271`). The cost of those extra draws is not measured.

Pass-level detail is on [04](04-frame-anatomy-and-pass-order.md) and in the shader pages.

### 6. The scene depth copy: the main trigger

The engine copies its colour and depth buffers from the default framebuffer with `glCopyTexImage2D` right before bloom (comment at `src/tyrender.cpp:283`). The DLL uses the depth copy as the point where screen-space passes run (`src/tyrender.cpp:280-303`):

1. The hook only acts for level-0 copies from the default framebuffer (`g_curFBO == 0`, `:284`).
2. A colour copy (format RGB or RGBA: `0x1907`, `0x1908`, `:285`) is remembered for later.
3. A depth copy (format `GL_DEPTH_COMPONENT` `0x1902`, or the sized forms `0x81A5`, `0x81A6`, `0x81A7`, `:286`) records the screen size and the scene depth texture (`:287`).
4. Then, in order: scene material update (`PbrUpdate`, `:289`), AO (`RunAO`, `:290`), GI (`RunGI`, `:291`, not when AO debug is on), emitter light (`RunEmit`, `:292`, same rule), atmosphere (`RunAtmos`, `:293`). Timer marks 1 to 6, 20 and 21 wrap these (`GProf`, `:290-293`).
5. If a pass changed the scene colour, the DLL copies the colour again into the engine's copy, so bloom sees the changes (`:294-300`). Timer marks 18 and 19 wrap that copy.

### 7. The 64 by 64 viewport: the post trigger

The engine draws the first 64 by 64 viewport after the depth copy when it starts its brightness chain. `glViewport` (`src/tyrender.cpp:252-257`) runs the final post pass (`RunFX`, `:254`) at that point. A flag makes it run once per frame (`g_fxDoneFrame`, `:254`). The same hook also enlarges the viewport of 2048 by 2048 shadow passes when the shadow map scale is above 1 (`:255`). This second part is for [shadow-filter](../shaders/shadow-filter.md).

### 8. Frame boundary: SwapBuffers

`h_SwapBuffers` (`src/tyrender.cpp:591-630`) runs once per frame before the real present. Why here (inferred): the frame is finished at this call, so the per-frame housekeeping and the A/B screenshot must run after the last draw and before the image is shown. The steps inside the hook:

1. A capture-end line if a capture is running (`:592`).
2. Motion test (`MotionFrame`, `:593`).
3. Every 8 frames: a snapshot, the debug camera and motion polls, the AO, atmosphere and GI flag-file polls, the `post_off` file check, and the reload check (`:594-604`).
4. Texture report (`TexReport`, `:605`), GPU timer mark 9 (`:606`), the A/B screenshot (`ABShotFrame`, `:607`), and the overlay (`OverlayFrame`, `:608`).
5. The real `SwapBuffers` (`:609`), then the session frame row (`SessFrame`, `:610`), the timer collect (`GProfNextFrame`, `:611`), the frame counter (`:612`) and the TAA jitter update (`:613`).
6. Every 600 frames, a timing line `[frame N] avg ... ms/frame` (`:616-620`).
7. The capture flags for the next frame (`:622-628`).

### 9. Other hooks that matter for the passes

- `glShaderSource` (`src/tyrender.cpp:362-402`) reads each shader source, writes a dump file, reads an override, and rewrites the source of selected programs. It test-compiles each rewrite and falls back to the original on error (`:398`). Details are on [shader source rewriting](05-shader-source-rewriting.md).
- `glUniformMatrix4fv` (`:345-359`) finds the scene `VPMatrix` through `TrackVP`. It adds the TAA jitter to the matrix and applies the debug camera (`:350-356`). This is how jitter reaches the scene (see [TAA](../shaders/taa-fxaa.md)).
- `glUniform4fv` and `glUniform4f` (`:409-425`) catch the engine's `fogCoords` and pass the fog through `FogOverride` (`:404-408`).
- `glUniform1i` (`:426-430`) records which texture unit holds each cascade shadow map.
- `glBindTexture` and `glActiveTexture` (`:260`, `:330`) keep the texture bound to each unit. The copy hook and the upload hook use it to name a texture (`:287`; `src/tex.inl:278`).
- `glUseProgram` (`:329`) records the current program, calls `PbrApply` and `LightApply`, and sets the tess flag (`g_curProgTess`).

### 10. Re-entry

The DLL has no dedicated re-entry flag. Three things keep the hooks from calling themselves (inferred from the code, not stated in comments):

1. The DLL's own GL work uses the original pointers only. A search for direct `gl...(` calls in `src/` finds none outside `h_` and `o_` names (`src/*.inl`, `src/tyrender.cpp`). Pass code calls `o_glBindTexture`, `o_glUseProgram` and so on.
2. Hook actions check frame and state flags: `g_fxDoneFrame` (`:254`), `g_curFBO == 0` before the named-uniform and fog hooks (`:349`, `:414`, `:423`), and `g_abOff`, which the code comment defines as the A/B "off" frame that AO, GI, atmosphere, post and TAA skip (`:78`).
3. Passes that draw skip the others when a debug view is on (`:291-293`).

### 11. Crash handler

`AddVectoredExceptionHandler(1, CrashVEH)` is registered in DllMain (`src/tyrender.cpp:713`). The handler:

- filters for access violation, breakpoint, illegal instruction, stack overflow, integer divide by zero and `0xC0000409` (`:687-688`);
- logs the first fatal exception once (`:689`), with the code, address, module and offset, frame, stage, program and FBO (`:692-693`), and a stack trace (`:694-695`);
- writes a crash line to the session report (`:696`, `src/sesslog.inl:362-366`);
- returns `EXCEPTION_CONTINUE_SEARCH` (`:698`), so it never handles the exception itself. The later handlers still run (inferred from the return value).

### 12. Reads that could fail

`glReadPixels` (`src/tyrender.cpp:308-315`) skips reads on the default framebuffer that fall outside the window. The comment says the engine reads a 16 by 16 depth patch around the sun for the lens flare, and an Intel driver crashed on those out-of-window reads (`:304-306`, "verified 2026-10-11"). The skipped reads are counted, and the first 20 are logged as `[fix] skipped out-of-bounds glReadPixels` (`:311`).

## Code pointers

| What | file:line | Note |
|---|---|---|
| IAT patch primitive | `src/tyrender.cpp:108-129` | `PatchIAT` |
| Hook macro | `src/tyrender.cpp:250` | `HOOK(ret, name, params)` |
| Proc hook table | `src/tyrender.cpp:438-455` | 18 names, 18 aliases |
| `wglGetProcAddress` hook | `src/tyrender.cpp:457-468` | Returns hooks for table names |
| Draw hooks | `src/tyrender.cpp:267-271` | Per draw |
| Copy hook (pass trigger) | `src/tyrender.cpp:280-303` | Depth copy |
| Viewport hook (post trigger) | `src/tyrender.cpp:252-257` | 64 by 64 |
| Frame hook | `src/tyrender.cpp:591-630` | `h_SwapBuffers` |
| Install | `src/tyrender.cpp:662-671` | IAT list |
| Crash handler | `src/tyrender.cpp:685-699` | |
| KERNEL32 file hooks | `src/tex.inl:116-165` | Texture file tracking |
| Real-pointer lookup in modules | `src/ao.inl:37-41` | `GLProc` |

## Settings

| Key | Default in code | Shipped in `default_tyrender.ini` | Live or restart | Effect |
|---|---|---|---|---|
| `[debug] CaptureFrame` | 0 (`src/tyrender.cpp:58`) | absent | restart | Logs one frame call by call |
| env `TYRENDER_CAPTURE` | unset (`src/tyrender.cpp:63`) | not applicable | restart | Same as `CaptureFrame` |
| `cap.req` file | not applicable | not applicable | live (polled every 8 frames) | Logs the next frame (`src/tyrender.cpp:624-627`) |
| `[debug] DumpShaders` | 1 (`src/tyrender.cpp:60`) | absent | restart | Shader dump files (`:369-372`) |
| `[debug] OverrideShaders` | 1 (`src/tyrender.cpp:61`) | absent | restart | Override files (`:374-375`) |
| `[tex] Enable` | 1 (`src/tex.inl:67`) | absent | restart | Turns off the KERNEL32 file hooks and texture tags |
| `[tex] LogUnknown` | 0 (`src/tex.inl:68`) | absent | restart | Logs untagged draws by texture name |
| `[spill] LogEmitters` | 0 (`src/tex.inl:69`) | absent | restart | Logs additive draws (uses `glBlendFunc` state, `:264`) |

The full settings are on the settings reference pages. [settings-system-debug](../reference/settings-system-debug.md) covers `[debug]`.

## Cost

Not measured. Each hook adds one call and some checks. Costs that scale with the frame:

- Every hooked draw runs about nine helper calls (`src/tyrender.cpp:267-269`). Each checks state first. Costs are not measured.
- Every texture file read through the KERNEL32 hooks takes a mutex (`src/tex.inl:145`, `:149`). Each file open with read access converts the name to UTF-8 (`src/tex.inl:119`). Both run only while `[tex] Enable` is on. Costs are not measured.
- Every 8 frames the DLL checks several flag files: `post_off`, `reload`, `cap.req`, and the poll functions called at `src/tyrender.cpp:595`.

Measure with a GPU and CPU capture (see [debugging](../guides/debugging.md)). No figure is recorded in this page.

## Limits

- Only functions the game imports from `opengl32.dll` at load time go through layer one. A function the game loads with `GetProcAddress` or `LoadLibrary` bypasses the IAT (inferred). See NI-GL-1.
- Only the functions in `g_procHooks` are redirected in layer two. Other extension functions return the real pointer and run unseen (`src/tyrender.cpp:466-467`).
- Tessellated programs drop every draw that is not a triangle list (`src/tyrender.cpp:268`, `:271`). Those draws do not appear on screen. The count is logged in the `[frame N]` line (`:618`).
- The GI mask and emitter passes draw the same geometry again. They cost time that is not measured.
- `h_wglMakeCurrent` logs the first context only (`src/tyrender.cpp:489-500`). A second context is not handled (inferred).
- `glGetString` is a plain pass-through (`src/tyrender.cpp:316`). The DLL does not change the vendor or version strings.
- Uniform calls that are not in the named-location table go through unchanged (`src/tyrender.cpp:417`, `:424`, `:429`).

## Known issues

- Intel driver crash on out-of-window `glReadPixels`. Worked around in the DLL (`src/tyrender.cpp:304-315`). The code comment says it was seen on an Intel driver module (`igxelpicd32`) on 2026-10-11. README says Intel is untested (`README.md:11`). No issue number in the code.
- Tessellation portability (issue #16, `CHANGELOG.md:60`). The gate is described on the tessellation page.

## How to verify

1. Install check: after the first start, `tyrender.log` has no `[iat] NOT FOUND` line (`src/tyrender.cpp:127`). The three `[iat] ... orig=` lines show the real modules (`:728-730`).
2. Layer two check: `[proc] glUseProgram -> hooked` (or `glUseProgramObjectARB` for an alias) appears when the engine asks for the name (`src/tyrender.cpp:463`).
3. Driver strings: after the first `MakeCurrent`, `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION` and `GLSL` lines are logged once (`src/tyrender.cpp:492-499`).
4. Per-call trace: create `<game folder>\tyrender\cap.req` (empty file is fine). On the next frame boundary, `tyrender.log` gets `=== BEGIN CAPTURE frame N ===`, then lines such as `glDrawElements 0x4 n=...`, `glUseProgram ...` and `glCopyTexImage2D ...`, and then `=== END CAPTURE frame N ===` (`src/tyrender.cpp:592`, `:624-628`).
5. Depth copy trigger: in that captured frame, the `glCopyTexImage2D` line with `ifmt` in `0x1902`, `0x81a5` to `0x81a7` is the one that runs the passes (`src/tyrender.cpp:286`). Timer marks 1 to 6 in the `[gpu]` lines (every 300 frames, if the timers are on) show the pass time in that order. See [log messages](../reference/log-and-error-messages.md).
6. Post trigger: in the same capture, a `glViewport 0 0 64 64` line appears after the depth copy (`src/tyrender.cpp:254`).

## NEEDS INVESTIGATION

- **NI-8 (threading):** Are any hooked calls made from a thread other than the render thread? Method in [01](01-architecture-overview.md#needs-investigation).
- **NI-GL-1 (new item, proposed for section 6):** Does the engine call `GetProcAddress` on `opengl32.dll`, or `LoadLibrary`, for any hooked function? Such calls bypass the IAT. Method: count `[iat] NOT FOUND` lines, compare the `[proc]` names with the hook table, and if needed set a breakpoint on `GetProcAddress` in a test run.
- **NI-GL-2 (new item, proposed):** Number of calls per frame for each hooked function. Method: a capture with `cap.req`, then count the lines per frame in `tyrender.log`. Not measured.

## Discrepancies

- None found between this page and `ARCHITECTURE.md` section 2 (the hook list and the SwapBuffers description agree with the code). The first-pass wording "pointers handed out by wglGetProcAddress are also replaced" matches `src/tyrender.cpp:457-468`.
- The plan's checklist C-HOOKS (section 3.3) lists all hooked functions, and the code has no other hooks. Checked with `grep -n 'HOOK(' src/*.inl src/*.cpp` and the `IAT_GL` / `PatchIAT` calls.

## See also

- [hooks-reference](../reference/hooks-reference.md): every hook in one table.
- [01-architecture-overview.md](01-architecture-overview.md): where the hooks sit in the start-up order.
- [02-loading-and-injection.md](02-loading-and-injection.md): how the DLL is loaded into the game.
- [04-frame-anatomy-and-pass-order.md](04-frame-anatomy-and-pass-order.md): what runs at each hook point.
- [05-shader-source-rewriting.md](05-shader-source-rewriting.md): the shader hooks.
- [debugging](../guides/debugging.md): capture and trace.
