# Request: F11 frame A is a true stock frame

Checks that the "shaders-off" frame written by the F11 A/B hotkey (src\abshot.inl) matches a stock frame at the same fixed camera.
Not executed by this change. It is a spec for the harness owner to run in the TEST phase, under the game lock (see common.ps1).

## Inputs
- Game folder: `C:\CLAUDE\TY the Tasmanian Tiger` (or a `-Copy` of it).
- Spot: `rainbow` (slot 1, the Red Mountain start view). Fixed camera: no `tyrender\cam.txt`, so the spot start view is used.
- Build: the build under test (`src\out\tyrender.dll`, built from this branch).

## Variants
1. `stock`: `tyrender.dll` and the other mods off (no `tyrender.dll` in the game folder, or the vanilla TY.exe), same spot and resolution.
2. `f11`: the build under test, F11 pressed once; capture the `shaders-off` file (frame A, the vanilla frame).

## Check
- Both images at the same resolution. Ignore Ty's region (`$TyX0..$TyY1` in common.ps1).
- Mean absolute difference of luma over the rest of the frame, using `PlaytestImg.Luma` on 4x4 blocks.
- Pass if MAD <= 2.0 (out of 255). Report MAD, the max block difference, and the two file names.
- Fail if MAD > 2.0. In that case, list the passes still on (AO, GI, atmos, fx, TAA, shadows) from the log.

## Known residuals (expected to remain, report them as such)
- Shadows are not gated by the hotkey. This fix does not change the shadow strength path.
- Tessellation geometry stays subdivided during the off frame. Only its relief displacement and PBR extras go to zero (g_tsLive = 0).
- The light uniforms in light.inl (shadow tint, hemisphere light) stay on during the off frame. They have only the ini `[light] Enable` switch and no hotkey gate. The colour grade runs inside RunFX, so it is skipped with the rest of the post chain.
