# 2026-06-18 Visual Pipeline Bug Log

This records the visual/asset bugs found during the Yarlung Tsangpo canyon iteration on June 18, 2026.

> Historical note: several entries below refer to the retired Landscape macro material chain (`M_YarlungLandscapeGround`, `YarlungMacro`, `LeafyGrass`, and helper inspection scripts). As of the 2026-06-20/21 corridor-mesh cleanup, runtime terrain uses `SM_YarlungCorridorTerrain` + `M_YarlungMeshTerrain`; the old macro/LeafyGrass source assets have been removed. Keep this file as bug history, not as current implementation guidance.

## Open

### Visual target still below photo-real / 3A bar

- Symptom: `Saved/VisualCheck-aaa-cool-steel-t10s.png` and `Saved/VisualCheck-aaa-cool-steel-t15s.png` are now green/daylight, but still read as stylized procedural terrain rather than photo-real Yarlung/Nyingchi scenery.
- Current diagnosis: the pipeline now imports the intended landscape material, but the project still needs higher-frequency terrain displacement/detail, real vegetation scatter, cooler wet rock layering, and less primitive coaster/support geometry.
- Next action: continue with a higher-ceiling asset route: Nanite rock clusters, instanced vegetation, better river shader/foam, and camera validation shots outside the first-person coaster path.

### Landscape macro albedo renders warm/orange in game

- Symptom: `Saved/VisualCheck-aaa-1009-cool-t10s.png` still shows brown/orange slopes, even though the generated source macro albedo is cool gray-green.
- Evidence: pixel sampling on the screenshot averaged warm brown values around `(139, 124, 101)`, while the source macro albedo preview is visually cool gray-green.
- Evidence: `scripts/inspect-yarlung-material.py` confirmed `M_YarlungLandscapeGround` BaseColor is connected to `/Game/Generated/Materials/YarlungMacro/T_YarlungMacroAlbedo`.
- Current diagnosis: not just a monitor/HDR issue. Windows HDR or video enhancement may affect perceived display, but the saved PNG is already warm. The root cause found afterward was pipeline-level: UE Python reported a material graph error while still returning process exit code 0, and `YarlungLandscapeImportCommandlet.cpp` then silently fell back to `M_CoasterTint`.
- Fix: `scripts/create-coaster-materials.py` now writes a success marker only after the landscape material graph is rebuilt; `scripts/import-yarlung-landscape.ps1` checks that marker and the generated material/model assets; `YarlungLandscapeImportCommandlet.cpp` now fails if `M_YarlungLandscapeGround` is missing instead of using fallback materials.
- Follow-up: generated macro texture sources are now uncompressed TGA, the source albedo average is green-dominant, and the latest validation shots show wet green slopes instead of the earlier red canyon look. Residual warm sun bands remain a visual tuning issue, not the original hidden fallback bug.

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

### Hidden landscape material fallback

- Symptom: the landscape import appeared to succeed while screenshots still showed the wrong warm/brown material.
- Root cause: `scripts/create-coaster-materials.py` could fail inside UE Python, but `UnrealEditor-Cmd.exe -ExecutePythonScript=...` still returned 0. The import commandlet then loaded `M_CoasterTint` or engine default material as a fallback.
- Fix: added material-generation success marker checks in `scripts/import-yarlung-landscape.ps1` and removed fallback material loading from `YarlungLandscapeImportCommandlet.cpp`.
- Lesson: headless UE scripts need explicit artifact/sentinel validation; process exit code alone is not enough.

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

- `scripts/offscreen-shot.ps1` is the default validation path for this project: build if needed, jump directly to the target on-rails time, capture a high-resolution first-person frame, and avoid stealing focus from fullscreen apps.
- UE Python stdout is not always visible in terminal; inspect `Saved/Logs/CoasterSim.log` for script output such as `[YARLUNG-MATERIAL]`.
- `scripts/inspect-yarlung-material.py` was added as a small read-only material introspection helper while debugging the orange landscape issue.
- `scripts/export-yarlung-macro-texture.py` exports the imported UE macro albedo texture so source TGA, UE asset, and final PNG pixels can be compared separately.
