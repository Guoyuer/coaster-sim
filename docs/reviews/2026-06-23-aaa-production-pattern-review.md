# AAA production-pattern review

Date: 2026-06-23

Reviewed evidence:

- `Saved/Diagnostics/water-reference-calibrated-v1.png`
- `Saved/Diagnostics/slope-rockwall-align-v1.png`
- `docs/refs/local/02_fp_yarlung_coaster_canyon.png`
- `docs/refs/local/03_fp_mountain_coaster_valley.png`
- `Config/yarlung-assets.json`
- `Source/CoasterSim/YarlungSceneryActor.cpp`
- `Source/CoasterSim/YarlungRiverSurfaceBuilder.cpp`
- `Source/CoasterSim/YarlungTerrainSurface.cpp`
- `Source/CoasterSim/CoasterTrackVisuals.cpp`
- `scripts/create-coaster-materials.py`
- `docs/plans/photoreal-progress.md`
- `docs/plans/aaa-asset-pipeline.md`

## Verdict

The user's production-pattern critique is correct. The current visuals are not mostly blocked by one more scalar tweak. The main ceiling is that the hero image is still organized by generic corridor terrain and belt/scatter placement, while the references are organized as authored first-person set pieces: foreground vehicle anchor, track as a leading line, deep river valley, continuous rock/forest masses, cloud layers, and far snow mountains.

The project has made good engineering progress: single visible river surface, shared terrain/scenery height model, fail-close asset config, Megascans path validation, generated map pipeline, and cleaner track rendering. But those fixes mostly make the scene coherent and debuggable. They do not yet replace the production pattern that makes the frame read like "smooth slope with assets on top."

## 1. Corridor terrain is still carrying the hero mountain

Evidence:

- `YarlungTerrainSurface::SurfaceZCm()` still produces the rendered slope from the DEM source, relaxed corridor profile, relief displacement, river carve, and a small lift. This is a good base, but it remains a broad smooth mesh.
- The latest screenshots still show large green-gray smooth banks at t30/t90/t180. The rock and forest layers sit on top rather than owning the mountain silhouette.
- `Config/yarlung-assets.json` now has real Megascans/PN assets, but the terrain material is still a limited `surface` + `rock_surface` stack. This helps texture detail, not macro authored geology.

Why it is not AAA:

- In a first-person scenic coaster shot, the visible hillside cannot be a naked DEM-derived corridor surface. Terrain should be a collision/macro base. The camera-facing slope must be covered by authored cliff masses, forest canopy masses, scree/talus, decals/material layers, and wet rock strata.

Fix:

- Add an authored "hero slope surface" layer that is keyed by track time ranges, not by global lateral belts.
- Start with 2-3 set-piece ranges: t30 exposed left/right slope, t90/t150 canyon wall, t180 river bend.
- For each range, define composition volumes: `ForegroundTrack`, `NearSlopeBreakup`, `MidCliffMass`, `CanopyMass`, `RiverBankDetail`, `FarRidge`.
- Let the corridor terrain remain visible only as far/mid fill or through forest canopy, not as the main slope body.

Priority: P0. This is the biggest AAA gap.

## 2. Scatter and belt placement are replacing composition

Evidence:

- Live placement types are `scatter`, `canopy_belt`, `cliff_belt`, `ground_cover_belt`, and `slope_rock_wall_belt`.
- `YarlungSceneryActor` samples along the track or river and applies lateral bands, occupancy, jitter, clearance, slope, and height gates.
- This is technically clean and fail-close, but the visible result is still evenly procedural: repeated stripes of assets around the route, not a designed shot.

Why it is not AAA:

- The references are not "assets distributed along a path." They are composed frames. Track placement, valley reveal, rock mass, canopy density, cloud band, and river shape all cooperate around camera time.

Fix:

- Keep the current belt system as background fill.
- Add a higher-priority authored set-piece placement module above it.
- The authored module should consume a small JSON file with named first-person ranges and explicit composition goals:
  - time/sample range
  - preferred side
  - visible river window
  - cliff wall anchor points
  - forest canopy blocks
  - no-spawn windows for sightlines
  - optional track/camera bias
- The module should output HISM transforms or generated static meshes through the same commandlet, not hand-placed actors.

Priority: P0. Do this before adding more HISM density.

## 3. Rock walls are still patches, not continuous geology

Evidence:

- `cliff_belt` and `river_wall_*` generate many vertical-facing cliff instances, but each instance is still an isolated Megascans mesh.
- `slope_rock_wall_belt` improved the wrong "fallen timber" read by forcing `slope_rock_wall` assets and chunkier scale, but it still places many independent chunks.
- Screenshots show some good rock masses in the canyon, but exposed terrain still determines the main silhouette.

Why it is not AAA:

- AAA canyon walls usually start from continuous massing: large cliff planes, buttresses, ledges, overhang silhouettes, and strata direction. Smaller meshes, boulders, scree, and vegetation break the transitions after the mass is established.

Fix:

- Build a "rock-wall kitbash" pass that creates continuous wall groups instead of individual instances.
- Each wall group should choose 3-8 cliff meshes, place them along a local wall spline/plane, align them to a shared geological direction, and overlap them enough to hide the terrain surface.
- Add group-level parameters: wall height, wall length, strata yaw, slope embed depth, silhouette roughness, and vegetation pockets.
- Keep instance-level jitter only inside the group. Do not let every rock pick its own visual logic.

Priority: P0. This is the direct fix for the "green-gray slope plus rocks" read.

## 4. Water is visible, but still not photo water

Evidence:

- The default map now has one visible explicit mesh river: `SM_YarlungRiverSurface`.
- `YarlungRiverSurfaceBuilder` now resamples the river rows and smooths the banks, so the "hard zigzag boundary" bug is improved.
- Current worktree WIP also changes `YarlungTerrainSurface` to carve the channel relative to visible water width, reducing the slab-above-ground look. This is not yet validated in a screenshot in this review.
- Current worktree WIP changes the generated water material from `MSM_DEFAULT_LIT` toward `MSM_SINGLE_LAYER_WATER` and adds absorption/scattering parameters. That is the correct direction, but the reviewed screenshots (`water-reference-calibrated-v1`) still represent the prior non-photo water read. The WIP still needs build/material regeneration and screenshot validation before it can be called fixed.

Why it is not AAA:

- The river reads as a turquoise road/flat plastic because there is no real water shading stack: flow normals, reflected sky/cliffs, depth/shore fade, turbidity, whitewater mask, wet shoreline, and foam breakup.
- The current WIP addresses only part of that stack. Single Layer Water and visible-width channel carving are necessary, but photo water still needs flow normals, reflected sky/cliffs, shore wetness, foam breakup, and a validation mode that distinguishes material failure from geometry/contact failure.

Fix:

- Short term: improve geometry/contact and remove the remaining plastic cues:
  - validate the current visible-width river carve and Single Layer Water WIP with fresh `iterate-yarlung` screenshots
  - edge feather/drop at the mesh shore if the shoreline still reads like a raised slab
  - no emissive lift unless it is proven invisible under the new water shader
  - stronger non-repeating flow normal illusion through material-generated panners or imported normal maps
  - whitewater from slope/curvature masks, not just vertex color noise
- Medium term: switch to a proper UE water shading path or a custom translucent/single-layer-water material. It should use flow-direction UVs, two normal scales, roughness variation, depth/shore fade, SSR/Lumen reflection contribution, and foam masks.
- Add a water diagnostic screenshot mode that hides scenery and terrain separately to isolate material vs geometry/contact issues.

Priority: P0/P1. It matters a lot in reference alignment, but mountain massing should not wait for final water.

## 5. Foreground first-person anchor is missing

Evidence:

- `CoasterTrackVisuals` still loads `/Engine/BasicShapes/Cylinder.Cylinder` for rails, ties, braces, and supports.
- The hidden cube train placeholder has been removed, but there is no authored cockpit/train/handrail/seat/safety bar.
- The reference images get a large realism boost from hands, lap bar, nose cone, seats, bolts, stickers, and real track assemblies.

Why it is not AAA:

- Even a good canyon will read as a tech demo if the foreground is only red proxy tubes. First-person rides need physical scale anchors near the lens.

Fix:

- Add a simple but authored first-person cockpit/vehicle front as a generated or imported mesh:
  - nose shell, lap bar, seat edge, side rails
  - black rubber grip/handrail
  - bolts/plates/decals as geometry or material details
  - optional hands only if the asset quality is good enough
- Replace cylinder-only rails with a swept track mesh or modular rail/tie/support kit after the cockpit is present.
- Keep current cylinder track as a fallback only during development; final visual gate should fail if BasicShapes are visible in hero mode.

Priority: P1. It is extremely visible, but mountain/water massing has higher frame-wide impact.

## 6. Forest layering is still not organized like a real valley

Evidence:

- `canopy_belt` uses full PN spruce static meshes and many bands, which is better than previous branch-clump approximations.
- `ground_cover_belt` and shrub scatter still act as route-relative belts.
- The latest screenshots still show single-tree silhouettes and gaps on smooth slopes, not continuous canopy carpets.

Why it is not AAA:

- Forest should have three visual distances:
  - far canopy mass: continuous dark-green texture/volume over slopes
  - mid trees: individual crowns at the cliff/track scale
  - near understory: shrubs/ferns/ground cover only where camera can inspect it

Fix:

- Add a forest massing layer separate from individual tree scatter:
  - far canopy impostor/static clusters or dense HISM groups with low silhouette noise
  - mid-distance full trees in clumps, not uniform bands
  - near shrubs/understory only in controlled pockets
- Use slope/aspect/river humidity masks, but let set-piece ranges override density for composition.
- Stop using shrubs or half-tree-like assets as slope cover. They should be understory only.

Priority: P1. Needed after rock-wall groups so the forest can wrap around real cliff/slope forms.

## 7. Track/camera composition is not yet serving the valley reveal

Evidence:

- Current route can cross/overlook the river, but many screenshots still center the red track and push the valley to the sides/background.
- The refs use the track as a leading line through a larger composition: high overlooks, sweeping bends, far valley, and visible scale drops.

Why it is not AAA:

- A scenic coaster frame should be composed around the reveal. The track should guide the eye, not monopolize the frame.

Fix:

- Choose 3 hero beats and author the route/camera around them:
  - high overlook: track curves along/above the slope, river far below
  - cross-river/bridge beat: water and opposite cliff framed by track
  - cliff-hugging bend: near rock wall on one side, deep valley on the other
- For each beat, adjust camera pitch/yaw/banking and track height for view, then re-run gates.
- Do not optimize for "always follows river." Optimize for first-person scenic composition.

Priority: P1. This becomes much more valuable once slope/water are not obviously fake.

## 8. More HISM count has diminishing returns

Evidence:

- The config already has thousands of instances across boulders, cliffs, scree, shrubs, and canopy trees.
- Previous progress notes show density increases improved some frames but left t30/t90/t180 as smooth terrain or repeated chunks.

Why it is not AAA:

- The bottleneck is not instance count. It is the lack of composition ownership, continuous massing, material water, and first-person foreground anchors.

Fix:

- Freeze broad density increases unless a specific set piece needs them.
- Spend effort on higher-level modules:
  - authored set-piece scenery
  - grouped cliff wall kitbash
  - forest canopy massing
  - real water material
  - cockpit/track assets
- Add gates that report "visible naked terrain area" and "BasicShapes visible in hero mode" as diagnostic warnings, but do not treat metrics as final quality.

Priority: P0 policy decision. Avoid wasting the next iterations.

## Recommended execution order

1. Build authored set-piece data and placement pass for t30/t90/t150/t180, with explicit sightlines and no-spawn windows.
2. Implement grouped rock-wall kitbash for those set-piece ranges; make it cover the visible corridor terrain rather than just decorate it.
3. Add forest massing on top of the new wall/slope forms: far canopy mass first, then mid trees, then understory pockets.
4. Improve water shading/contact enough that it stops reading as road/plastic: edge feather plus flow normals/foam/depth/shore fade.
5. Add a first-person cockpit/vehicle anchor and start replacing BasicShapes track with authored coaster structure.
6. Recompose route/camera hero beats once the environment can support them visually.

## Architectural recommendation

The current `YarlungSceneryActor` is doing too much as a single placement module. It is technically cleaner than before, but it has become a shallow interface: every new visual idea becomes another placement enum plus many scalar fields.

The next deeper module should be a generated "Yarlung hero set piece" layer:

- Small interface: named time/sample ranges plus composition intent.
- Large implementation: uses terrain, river, track, asset config, wall grouping, forest massing, and visibility rules internally.
- Output: HISM transforms and optional generated static meshes, still produced by the commandlet.

This gives better locality: visual composition decisions live in one authored layer, while generic belts remain background fill.

## What not to do next

- Do not add more global `lateral_bands_cm` or occupancy unless tied to a named set piece.
- Do not restore old procedural canyon-wall, old procedural river actor, full-map fallback, or dual-water path.
- Do not judge water/route/cliff direction as failed until material/contact/setup bugs are isolated.
- Do not treat risk metrics as acceptance; they are triage only.
- Do not spend time on corridor terrain color tweaks as the main mountain solution.
