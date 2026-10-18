# Hooks reference

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), C (reviewer). Mechanism: [03-gl-hooking](../how-it-works/03-gl-hooking.md).

## Purpose

This is the complete list of functions the DLL replaces or wraps. Use it to find out which function a bug touches, or to check that a new hook is registered in the right place. It lists 40 hooked functions: 32 GL functions, 3 WGL functions (`wglGetProcAddress`, `wglCreateContext`, `wglMakeCurrent`), `SwapBuffers`, and 4 file functions. It also lists the non-hook entry points (DllMain, the crash handler and the export).

Two mechanisms are used:

- **IAT**: the function's slot in the game's import table is replaced. Registered by `Install` (`src/tyrender.cpp:662-671`) and `TexInstallHooks` (`src/tex.inl:166-171`).
- **proc**: the function is returned by `wglGetProcAddress` when the engine asks for it by name (`src/tyrender.cpp:438-455`, `h_wglGetProcAddress` at `:457-468`). Each proc hook has an alias (EXT, ARB or ObjectARB) that resolves to the same hook.

No hook uses a trampoline or patches code bytes (see [03](../how-it-works/03-gl-hooking.md#1-layer-one-import-address-table-iat)).

## How it works

Each hook forwards to the original through an `o_` pointer. The column "when it runs" says when the engine calls the function, as far as the code shows. Measured call counts are not in the repo. Use a capture (`tyrender\cap.req`, see [03](../how-it-works/03-gl-hooking.md)) to count them.

## Code pointers

### IAT: OPENGL32 (17 functions)

| Function | Mechanism | Purpose in the DLL | file:line | When it runs |
|---|---|---|---|---|
| `glViewport` | IAT | Runs the final post pass on the first 64x64 viewport after the depth copy, once per frame. Enlarges the viewport of 2048x2048 shadow passes when the shadow scale is above 1. | `src/tyrender.cpp:252-257`; IAT `:665` | Every viewport change |
| `glBindTexture` | IAT | Records the texture bound to each unit (`GL_TEXTURE_2D` only). | `:260`; IAT `:665` | Every texture bind |
| `glClear` | IAT | Logs the call during a capture. Nothing else. | `:261`; IAT `:665` | Every clear |
| `glEnable` | IAT | Counts `GL_STENCIL_TEST` enables. The count is used in one diagnostic log line only (`src/ao.inl:722`). | `:262`; IAT `:665` | Every enable |
| `glDisable` | IAT | Logs the call during a capture. | `:263`; IAT `:665` | Every disable |
| `glBlendFunc` | IAT | Keeps the last source and destination factors for the spill log (`src/tex.inl:243`). | `:264`; IAT `:666` | Every blend change |
| `glDepthMask` | IAT | Logs the call during a capture. | `:265`; IAT `:666` | Every depth-mask change |
| `glColorMask` | IAT | Logs the call during a capture. | `:266`; IAT `:666` | Every colour-mask change |
| `glDrawElements` | IAT | Per-draw work: draw count, material uniforms (`TexDrawPrep`), spill and fog diagnostics, late fog, tessellation mode (only triangles are drawn; others are dropped), pending shadow texture binding, see-through depth, emitter query, GI source mask redraw, emitter redraw. | `:267-269`; IAT `:666` | Every indexed draw |
| `glDrawArrays` | IAT | Same as `glDrawElements` except late fog and see-through depth. | `:270-271`; IAT `:666` | Every array draw |
| `glTexImage2D` | IAT | Enlarges the 2048x2048 cascade shadow maps when the shadow scale is above 1. Logs render-target allocations. Sends uploads to `TexOnUpload`. | `:272-278`; IAT `:667` | Texture allocation and upload |
| `glCopyTexImage2D` | IAT | Pass trigger. On a depth copy from the default framebuffer, runs AO, GI, emitter light and atmosphere. Copies the colour again if a pass changed the scene. | `:280-303`; IAT `:667` | Engine copy before bloom |
| `glReadPixels` | IAT | Skips reads on the default framebuffer that fall outside the window (Intel driver crash workaround). | `:308-315`; IAT `:667` | Engine readbacks (lens flare) |
| `glGetString` | IAT | Pass-through. | `:316`; IAT `:667` | At start-up |
| `wglGetProcAddress` | IAT | Returns the DLL's hooks for the names in the proc table. Stores the real pointer for the DLL's own use. Logs each name. | `:457-468`; IAT `:668` | Every name lookup |
| `wglCreateContext` | IAT | Logs the context handle. | `:486`; IAT `:668` | Context creation |
| `wglMakeCurrent` | IAT | Logs the context; logs vendor, renderer, version and shading language version once. | `:489-500`; IAT `:668` | Context switch |

### IAT: GDI32 (1 function)

| Function | Mechanism | Purpose in the DLL | file:line | When it runs |
|---|---|---|---|---|
| `SwapBuffers` | IAT | Frame boundary. Motion test, every-8-frame flag polls and reload, texture report, A/B screenshot, overlay, real present, session frame row, timer collection, frame counter, TAA jitter update, capture flags. | `src/tyrender.cpp:591-630`; IAT `:669` | Once per frame |

### IAT: KERNEL32 (4 functions, in `src/tex.inl`)

| Function | Mechanism | Purpose in the DLL | file:line | When it runs |
|---|---|---|---|---|
| `CreateFileW` | IAT | Records handles for `data_pc.rkv` and `.dds` files opened for reading. Only while `[tex] Enable` is on. | `src/tex.inl:116-127`; IAT `:167` | Every file open |
| `CloseHandle` | IAT | Removes a tracked handle. | `src/tex.inl:128-131`; IAT `:168` | Every handle close (fast path when no handle is tracked) |
| `SetFilePointerEx` | IAT | Tracks the file position of tracked handles. | `src/tex.inl:132-139`; IAT `:169` | Every seek |
| `ReadFile` | IAT | Maps each read buffer to a texture name (ring of 8192 entries; RKV reads use a binary search over the DDS offset table). | `src/tex.inl:142-165`; IAT `:170` | Every read on a tracked handle |

### proc: resolved by `wglGetProcAddress` (18 functions, 18 aliases)

| Function | Alias | Mechanism | Purpose in the DLL | file:line |
|---|---|---|---|---|
| `glBindFramebuffer` | `glBindFramebufferEXT` | proc | Records the current FBO when the target is `0x8D40` (GL_FRAMEBUFFER) or `0x8CA9` (GL_DRAW_FRAMEBUFFER). Binds to `0x8CA8` (GL_READ_FRAMEBUFFER) are not recorded. The DLL itself binds the read target this way at `src/tyrender.cpp:518`, through the original pointer. | `src/tyrender.cpp:319`; table `:439` |
| `glFramebufferTexture2D` | `glFramebufferTexture2DEXT` | proc | Logs during a capture and the first two frames. | `:320-321`; table `:440` |
| `glFramebufferRenderbuffer` | `glFramebufferRenderbufferEXT` | proc | Logs every call. | `:322-323`; table `:441` |
| `glRenderbufferStorage` | `glRenderbufferStorageEXT` | proc | Logs every call. | `:324`; table `:442` |
| `glRenderbufferStorageMultisample` | `glRenderbufferStorageMultisampleEXT` | proc | Logs every call. | `:325-326`; table `:443` |
| `glBlitFramebuffer` | `glBlitFramebufferEXT` | proc | Logs during a capture. | `:327-328`; table `:444` |
| `glCompressedTexImage2D` | `glCompressedTexImage2DARB` | proc | Sends compressed uploads to `TexOnUpload`. | `src/tex.inl:197-200`; table `:445` |
| `glUseProgram` | `glUseProgramObjectARB` | proc | Records the current program and the tess flag. Calls `PbrApply` and `LightApply`. | `:329`; table `:446` |
| `glActiveTexture` | `glActiveTextureARB` | proc | Sets the active texture unit. | `:330`; table `:447` |
| `glCreateShader` | `glCreateShaderObjectARB` | proc | Records the type of each shader object. | `:331`; table `:448` |
| `glAttachShader` | `glAttachObjectARB` | proc | Logs; calls `TessOnAttach`. | `:332`; table `:449` |
| `glLinkProgram` | `glLinkProgramARB` | proc | Logs; calls `TessLink` and `AfterLink`. | `:333`; table `:450` |
| `glGetUniformLocation` | `glGetUniformLocationARB` | proc | Marks skinned programs (uniform `boneMats`), records the locations of `modelMatrix`, `VPMatrix`, and the named shadow and fog uniforms. Logs each new (program, name) pair once. | `:334-344`; table `:451` |
| `glUniformMatrix4fv` | `glUniformMatrix4fvARB` | proc | Captures the scene `VPMatrix`, adds TAA jitter, applies the debug camera, records character positions. | `:345-359`; table `:452` |
| `glShaderSource` | `glShaderSourceARB` | proc | Dumps the source, reads an override, rewrites PBR, shadow, light, decal and emitter code, test-compiles, falls back on error. | `:362-402`; table `:453` |
| `glUniform4fv` | `glUniform4fvARB` | proc | Fog coordinates (through `FogOverride`) and named uniform capture. | `:409-418`; table `:454` |
| `glUniform4f` | `glUniform4fARB` | proc | Same as `glUniform4fv`, for a single vector. | `:419-425`; table `:454` |
| `glUniform1i` | `glUniform1iARB` | proc | Records the texture units of the shadow maps and the diffuse texture. | `:426-430`; table `:454` |

### Other entry points

| Name | Kind | Purpose | file:line |
|---|---|---|---|
| `PatchIAT` | function | Finds an import by DLL and function name and replaces its slot | `src/tyrender.cpp:108-129` |
| `Install` | function | Calls `PatchIAT` for the list above | `src/tyrender.cpp:662-671` |
| `CheckOrig` | function | Logs the module of three saved originals | `src/tyrender.cpp:677-681` |
| `CrashVEH` | vectored exception handler | Logs and records the first fatal exception; returns continue-search | `src/tyrender.cpp:685-699`; registered `:713` |
| `DllMain` | entry point | Start-up order (see [01](../how-it-works/01-architecture-overview.md)) | `src/tyrender.cpp:701-734` |
| `tyrender_init` | exported function, empty | Gives the copy a named import, so the loader loads the DLL | `src/tyrender.cpp:673-674` |

## Settings

No key turns a hook on or off as a whole. Gates that matter:

- `[tex] Enable` (default 1, `src/tex.inl:67`): the KERNEL32 file hooks record nothing when it is 0 (`src/tex.inl:118`).
- `[debug] CaptureFrame` and `cap.req`: log every call in a frame (`src/tyrender.cpp:58`, `:624-627`).
- `[debug] DumpShaders` and `OverrideShaders` (both default 1, `src/tyrender.cpp:60-61`): `glShaderSource` dump and override.
- Each pass has its own enable key. See the settings reference pages.

## Cost

Not measured. Per-call work is listed under "Purpose". The per-call logs are behind the capture flag (`src/tyrender.cpp:50`). Logs outside a capture are only written at start-up or once per name.

## Limits

- Only the 17 OPENGL32 functions, `SwapBuffers`, and the 4 KERNEL32 functions are patched in the import table. Other imports are not hooked.
- Only the 18 names in the proc table are returned as hooks. Other GL functions reach the driver unchanged.
- Functions the passes use that are not in the proc table are fetched with the real `wglGetProcAddress` and are not hooked. Example: `glPatchParameteri` (`src/tyrender.cpp:576`).
- The `tyrender_init` export does nothing. Its only job is to give the loader a named import.

## Known issues

- `glReadPixels` out-of-window reads crashed an Intel driver (comment at `src/tyrender.cpp:304-306`, verified 2026-10-11). The DLL skips those reads (`:308-315`). Intel is untested in README (`README.md:11`).
- The tessellation draw path drops non-triangle draws (`src/tyrender.cpp:268`, `:271`). The count is in the `[frame N]` line (`:618`).

## How to verify

1. Count entries: `grep -n 'HOOK(' src/*.inl src/tyrender.cpp` should give 33 lines: 32 `HOOK` definitions (31 in `src/tyrender.cpp`, `glCompressedTexImage2D` at `src/tex.inl:197`) plus the macro definition at `src/tyrender.cpp:250`. The three non-macro GL-side hooks are `h_wglGetProcAddress`, `h_wglCreateContext` and `h_wglMakeCurrent` (`src/tyrender.cpp:457`, `:486`, `:489`). `h_SwapBuffers` is at `:591`. The file hooks are in `src/tex.inl:116-165`.
2. Check the IAT list: `grep -n 'IAT_GL(\|PatchIAT(' src/*.cpp src/*.inl` should show the 17 `IAT_GL` names (`src/tyrender.cpp:665-668`), `SwapBuffers` (`:669`) and the four KERNEL32 calls (`src/tex.inl:167-170`).
3. Check the proc table: `grep -n 'PH(\|PHA(' src/tyrender.cpp` should show 18 `PH` and 18 `PHA` entries (`:438-455`).
4. Runtime: `tyrender.log` should have no `[iat] NOT FOUND` line, and one `[proc] <name> -> hooked` line for each proc name the engine requests (`src/tyrender.cpp:463`).

## NEEDS INVESTIGATION

- **NI-GL-1 (new item, proposed):** Does the engine resolve any hooked function with `GetProcAddress` on `opengl32.dll` or with `LoadLibrary`? Such calls bypass the IAT. Method: count `[iat] NOT FOUND`, compare `[proc]` names with the list above, and set a breakpoint on `GetProcAddress` in a test run if needed.
- **NI-GL-2 (new item, proposed):** Per-frame call counts for each hooked function. Method: `cap.req` capture, then count the lines per frame in `tyrender.log`. Not measured in this pass.
- **NI-8 (threading):** Whether any hooked function is called from a thread other than the render thread. See [01](../how-it-works/01-architecture-overview.md#needs-investigation).

## Discrepancies

- None between this table and the code. The first-pass `ARCHITECTURE.md`, section 2, lists the SwapBuffers, copy, draw and shader hooks. Those match the lines above.
- The plan's C-HOOKS list (section 3.3) matches the code: 17 OPENGL32 IAT names, `SwapBuffers`, 4 KERNEL32 names, and 18 proc names with their aliases. No hook is missing from the list and none is extra.

## See also

- [03-gl-hooking.md](../how-it-works/03-gl-hooking.md): how the hooks are placed and what they do.
- [01-architecture-overview.md](../how-it-works/01-architecture-overview.md): start-up order and the module map.
- [04-frame-anatomy-and-pass-order.md](../how-it-works/04-frame-anatomy-and-pass-order.md): where the passes run.
- [log-and-error-messages](log-and-error-messages.md): the `[iat]`, `[proc]` and `[frame]` lines.
- [source-map](source-map.md): where each hook body lives.
