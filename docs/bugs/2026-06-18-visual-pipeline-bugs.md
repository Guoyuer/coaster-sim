# 2026-06-18 Visual Pipeline Bug Log

This records the visual/asset bugs found during the Yarlung Tsangpo canyon iteration on June 18, 2026.

## Open

### Landscape macro albedo renders warm/orange in game

- Symptom: `Saved/VisualCheck-aaa-1009-cool-t10s.png` still shows brown/orange slopes, even though the generated source macro albedo is cool gray-green.
- Evidence: pixel sampling on the screenshot averaged warm brown values around `(139, 124, 101)`, while the source macro albedo preview is visually cool gray-green.
- Evidence: `scripts/inspect-yarlung-material.py` confirmed `M_YarlungLandscapeGround` BaseColor is connected to `/Game/Generated/Materials/YarlungMacro/T_YarlungMacroAlbedo`.
- Current diagnosis: not just a monitor/HDR issue. Windows HDR or video enhancement may affect perceived display, but the saved PNG is already warm. The likely root is Unreal texture import/color handling, texture source format handling, or material sampling.
- Current mitigation: changed generated macro texture sources from BMP to uncompressed TGA in `scripts/generate-yarlung-landscape-assets.py` and `scripts/create-coaster-materials.py`.
- Next action: re-import from TGA, inspect the imported `T_YarlungMacroAlbedo` asset pixel/color settings, then re-run multi-position screenshots before committing the visual change.

### Terrain surface still exposes grid/contour artifacts

- Symptom: close and mid-distance slopes show dense contour-like lines and grid texture artifacts, especially in `VisualCheck-aaa-1009-cool-t10s.png`.
- Root cause: the previous 505 x 505 landscape heightmap was too coarse for the camera distance and steep slope framing. Some artifacts may also be from material texture filtering or landscape component sampling.
- Current mitigation: upgraded the generated heightmap path from `YarlungTsangpo_505.r16` to `YarlungTsangpo_1009.r16` and updated `YarlungLandscapeImportCommandlet.cpp` to import size 1009.
- Status: partially improved in pipeline, but visually not accepted yet. Needs validation after the TGA texture import fix.

### Display HDR may be a confounder, not the primary bug

- Symptom: user asked whether display HDR could explain the color mismatch.
- Finding: possible for perceived display, but unlikely to be the primary cause because the captured PNG itself has warm pixel values.
- Evidence: registry query only showed `EnableAutoEnhanceDuringPlayback`; no direct HDR confirmation was established in this run.
- Next action: if needed, capture a reference screenshot with Windows HDR off and compare raw image pixel averages. Treat HDR as a validation variable, not the main fix.

## Fixed Or Removed

### Bad low-ceiling procedural forest/rock overlay

- Symptom: temporary procedural `ForestCanopyMesh` and `RockStrataMesh` looked like paper patches/flat overlays instead of real vegetation or cliff geology.
- Root cause: hand-authored quads were used to hide missing landscape/asset quality, violating the project rule to prefer high-ceiling methods.
- Fix: removed the temporary components/functions and kept the direction on landscape materials plus imported assets.
- Lesson: do not cover terrain/material defects with ad hoc overlay geometry.

### Global close-detail normal map caused moire-like mountain artifacts

- Symptom: after adding `AerialGrassRock` PBR detail, mountain slopes showed dense high-frequency striping/ringing.
- Root cause: a close-range PBR normal map was sampled directly across the whole landscape without a proper landscape coordinate/macro-detail blend.
- Fix: disconnected the detail normal from `M_YarlungLandscapeGround` until proper landscape UV/layer blending exists.
- Lesson: scanned PBR detail should be layered by scale and masks, not blindly applied as the mountain material.

### Macro mask generator produced checker/diamond repetition

- Symptom: the first macro mask/albedo generation showed obvious repeating checker or diamond patches.
- Root cause: overlapping sine waves created regular interference patterns.
- Fix: replaced the sine pattern with low-frequency value noise in `scripts/generate-yarlung-landscape-assets.py`.
- Lesson: procedural masks need noise designed for non-repeating terrain breakup.

### Daylight regressed into gray/foggy lighting

- Symptom: earlier screenshots looked overcast/gray rather than the requested clear daytime blue sky.
- Root cause: fog density, aerial perspective, post-process saturation/contrast, and lower sun/sky intensity pushed the scene away from the initial bright daylight look.
- Fix: restored brighter sky/sun settings, lowered fog, reduced motion blur, and validated with `VisualCheck-blue-sky-sun-final`.
- Lesson: Yarlung/Nyingchi default weather should remain clear blue sky and white clouds unless explicitly changed.

## Harness Notes

- `scripts/visual-check.ps1` is still the fastest useful validation path for this project: build if needed, run game, capture multiple time offsets.
- UE Python stdout is not always visible in terminal; inspect `Saved/Logs/CoasterSim.log` for script output such as `[YARLUNG-MATERIAL]`.
- `scripts/inspect-yarlung-material.py` was added as a small read-only material introspection helper while debugging the orange landscape issue.
