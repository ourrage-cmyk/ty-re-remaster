# Changelog

Builds are identified by the git commit in the launcher title bar. "Untested" means not yet played by the owner.

## Unreleased (main after v0.3.5, up to 9388945)

Added
- **F10 settings overlay** (#12): in game, F10 opens a small panel with live brightness, saturation, sharpen, vignette, fog intensity, AO on/off, tess level and shadow taps. `[overlay] Key=F10`, `0` = off. Untested.
- **Water flicker fix** (#5): `[pbr] WaterStable=1` (default on) drops bump, relief, specular and sky Fresnel from water-tagged draws, aimed at the blue flicker on Ty. Untested.
- **Distance LOD** (#10): `[lod] Enable/GI/AO/Fog`, `Start=1500`, `End=6000`, `MinScale=0.35`. Off by default. Untested.
- **Fog consistency** (#4): `[fog] SeeThruDepth=0` and `LateEmitters=0` (both off by default). Untested.
- **Emitter light injection** (#9, phase 2): `[emit] Enable=0` (off by default); ranks emitter draws by screen area for the `MaxEmitters` cap. Untested.
- **Material classes** (#44, #48): one class lookup replaces the separate texture-flag tests. No intended visual change. Untested.
- **Settings clash guard** (#13): warns in the log when the game's own SSAO/MSAA is on (`[clash] Check=1`).

Changed
- Fog hue stays fixed as the camera turns (#3). Untested after the latest change.

## v0.3.5 (c255ae8)

Fixed
- Fog colour no longer changes with camera direction (#3); fog is 20% thinner by default and less tinted; new `[atmos] FogSaturation`, `HazeHue`, `FogAmount` defaults; the painted skybox is fogged.
- AO flicker and grain gone; AO fades out in thick fog (`[ao] FogFadePower`, default 2) (#1, #6).
- TAA ghosting reduced (#2).
- Half-res GI mask saves GPU time (#7).

Added
- Water fog option `[atmos] WaterFog` and `[pbr] SkyFresnelSkipWater` (#5).
- Surface relief slider now works; `[tess] ReliefDepth`, `ReliefCurve`, `ReliefKnee` (#41).
- Footprint glow clamp `[decal] Clamp` (off by default) (#8).
- F11 A/B screenshot hotkey `[shots] Key` (#14).
- Launcher embeds the DLL and default ini; title bar shows version and build id.
- Tessellation portability guards for older GPUs (#16); portal-light emitter logging `[spill] LogEmitters` (off).

Known
- Trees can look slightly dark in thick fog on the second level; character and grass shadows show a faint checkerboard pattern; Ultra costs a lot of GPU time.
- The title still says `v0.3.5-rc`.
