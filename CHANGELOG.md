# Changelog

Builds are identified by the git commit in the launcher title bar. "Untested" means not yet played by the owner.

## Unreleased (main after v0.3.5, up to 4a2ef95)

Added
- **F10 settings overlay** (#12): in game, F10 opens a small panel with live brightness, saturation, sharpen, vignette, fog intensity, AO on/off, tess level and shadow taps. `[overlay] Key=F10`, `0` = off. Untested.
- **Water flicker fix** (#5): `[pbr] WaterStable=1` (default on) drops bump, relief, specular and sky Fresnel from water-tagged draws, aimed at the blue flicker on Ty. Untested.
- **Distance LOD** (#10): `[lod] Enable/GI/AO/Fog`, `Start=1500`, `End=6000`, `MinScale=0.35`. Off by default. Untested.
- **Tessellation distance LOD** (#10, #60): `[lod] TessEnable`, default off. Untested.
- **Shadow LOD** (#10, #78): `[lod] Enable=1` + `Shadow=1` drops the 3x3/4x4 filter grid on the far shadow cascades (2 and 3), 8 taps instead of 17. Off by default. Untested.
- **Fog consistency** (#4): `[fog] SeeThruDepth=0` and `LateEmitters=0` (both off by default). Untested.
- **Emitter light injection** (#9, phase 2): `[emit] Enable=0` (off by default); ranks emitter draws by screen area for the `MaxEmitters` cap. Untested.
- **GI checker** (#7, #75): `[gi] Checker` halves the GI trace per frame. Untested.
- **Material classes** (#44, #48): one class lookup replaces the separate texture-flag tests. No intended visual change. Untested.
- **Settings clash guard** (#13): warns in the log when the game's own SSAO/MSAA is on (`[clash] Check=1`).
- **Water debug** (#5): `[pbr] DebugWater` bitmask to bisect the blue flicker on Ty (#62). **Emitter blur** `[emit]` wider portal blur, default off (#61).
- **Overlay fixes** (#12): the overlay toggle is focus-free and uses explicit draw state; the key default changed to F10 in #102 (existing inis keep `Key=0` and need `Key=F10`); explicit draw state and focus-free toggle (#54, #59, #63). Untested in level.
- **Automated playtest** (`tools/playtest`, #64, #65): boots variants, takes screenshots, checks flicker and overlay. Runs only in phase TEST and takes `C:\CLAUDE\LOCK\game.lock` (#66).
- **Water fog mask** (#5, #92): `[fog] WaterMaskSoft` softens the water-fog mask edge. Off by default. Untested.
- **GI far ring** (#7, #93): `[gi] AtrousFar` uses a 3x3 ring beyond `[lod] Start`. Off by default. Untested.
- **Water names** (#4, #106): `[fog] WaterNames` lists extra texture names (lily pads) that WaterBoost also fogs. Default empty. Untested.
- **README** (#107, #108, #109): old A/B pairs removed, three build `4a2ef95` pairs with technical captions.
- **Playtest harness** (#91): progress log, status.json, plan files, request queue, focus fix, visual review step.

Fixed
- F11 A/B: the "off" frame skips AO, GI, atmosphere, FX, shadow strength, tess LOD, light amounts and emit, so it is closer to vanilla (#96). Untested.

Playtest of 4a2ef95 (owner, Ultra, 2026-10-18)
- Good: sky and red rock fade smoothly with no hard steps, fog looks good, trees look fine, no crashes, F11 screenshots work, tessellation floor lift is on.
- Open: F10 overlay does nothing; water brightness ramps slowly on the character (GI off the blue texture, not a frame flicker, #5); lilies and rocks look brighter than the water under them and the fog is heavier from the top down (#4); sharp shadow line on the bridge near the fire; fire is not fogged; tree frog still broken (not noticeable); water outline from above looks off; textures not emissive (known).

Known
- `[emit] Enable=1` renders a flat white upper band and stripes in the autoplaytest view (portal variants, 4a2ef95 era); keep it off.

Changed
- Fog hue stays fixed as the camera turns (#3). Untested after the latest change.
- Sky and painted rock are fogged by `[atmos] SkyFogDist` (#58). Untested.
- Water draws are fogged by WaterBoost (#4, #74). Untested.

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
