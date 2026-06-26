# Yarlung AAA Progress

This is the current dashboard. Keep it short. Move stale detail to `docs/archive/`
or `docs/reviews/archive/`.

## Verdict

- Overall visual status: **FAIL**. The project is improving, but not yet close
  enough to the first-person reference images.
- Current target: photo/film-real first-person Yarlung/Linzhi scenic coaster.
- Current strategy: systemic AAA asset and composition layers, not one-off visual
  patches or global scatter density.

## Latest Validated State

Latest run:

```powershell
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -Times 90 -NamePrefix river-footprint-clearance-v1
.\scripts\iterate-yarlung.ps1 -Mode Terrain -Preset Focus -Build -Times 90 -NamePrefix rapid-water-width-v1
.\scripts\iterate-yarlung.ps1 -Mode Terrain -Preset Focus -Build -Times 90 -NamePrefix rapid-shore-edge-v1
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -Times 90 -NamePrefix foreground-track-v1
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -Times 90 -NamePrefix foreground-track-v2
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -Times 90 -NamePrefix foreground-track-v3
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -Times 90 -NamePrefix slope-gated-rockwall-v1
.\scripts\test-yarlung.ps1 -Build
.\scripts\iterate-yarlung.ps1 -Mode Terrain -Preset Focus -Build -NamePrefix terrain-material-canopy-safe-v1
```

Evidence:

- Contact sheets: `Saved/Diagnostics/foreground-track-v3.png`,
  `Saved/Diagnostics/footprint-grounding-v1.png`,
  `Saved/Diagnostics/no-cliff-belt-v1.png`,
  `Saved/Diagnostics/slope-gated-rockwall-v1.png`,
  `Saved/Diagnostics/terrain-material-canopy-safe-v1.png`
- Latest focus manifest:
  `Saved/Diagnostics/terrain-material-canopy-safe-v1-run.json`
- RiskGate: `FAIL`
- Latest focus t30: `terrain-material-canopy-safe-v1-t30.png`,
  risk `1.467`, flat `0.725`
- Previous water/shore focus: `rapid-shore-edge-v1-t90.png`, risk `1.534`,
  flat `0.675`
- Map inspect: `terrain-material-canopy-safe-v1` had 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 15/15

Implemented:

- Added footprint-aware river clearance for all scenery placement paths. River
  clearance now uses `distance_to_visible_water_edge - scaled_mesh_radius`, so
  large boulder, cliff, scree, and rock-wall meshes cannot pass validation with
  only their pivot outside the protected water/shore band.
- Widened the generated river/channel contract and rebuilt the river surface
  with more longitudinal rapid/foam breakup. River surface `foam_vertices`
  increased from `1246` to `2865`.
- Reworked the riverbed/shore height curve so the visible ribbon edge meets the
  waterline, then rises through a longer wet shelf instead of forming an
  immediate hard wall.
- Changed terrain wet-rock coverage to use distance from the visible water edge
  rather than distance from river centerline. This keeps the wet-bank material
  layer stable when the generated water width changes.
- Recast `wet_rock_shore` as smaller embedded bank stones near the visible
  water edge. The latest generated map places `1547` wet-shore instances,
  compared with `949` in the previous wide-water pass.
- Upgraded the first-person track proxy into a more useful authored-system
  placeholder: wider gauge, darker oxide rails, box-beam cross ties, thinner
  steel braces, more frequent structural rhythm, and a right-seat camera offset.
- Tested a more extreme right/open camera (`foreground-track-v2`) and rejected
  it as `DIRECTION-LIMIT`: it opened the valley but weakened the foreground
  coaster anchor without solving the left-wall dominance. The kept v3 uses the
  stronger v1 camera with lighter/thinner internal steel.
- Removed the legacy `cliff_belt` pipeline and its `CliffRockFacesA-F`
  components. This path scattered isolated cliff chunks over smooth terrain and
  was a major source of fake/floating rock reads.
- Added rotation-aware and footprint-aware grounding for large rock-wall
  modules, then gated rock-wall profiles to cliff-like slopes. Rock-wall
  instances dropped from `2690` in `no-cliff-belt-v1` to `237` in
  `slope-gated-rockwall-v1`, removing most fake random blocks from ordinary
  slopes.
- Added fail-close tests so removed belt fields are rejected and rock-wall
  profiles must remain surface-aligned and slope-gated.
- Fixed the mesh terrain material color pipeline. The previous graph multiplied
  macro coverage color by Megascans base color, which double-darkened the
  terrain in linear space. The material now keeps coverage color as the base and
  uses texture color only as a restrained detail tint.
- Corrected terrain material tiling from kilometer-scale reads to meter-scale
  reads. Forest-floor tiling is now `850` and rock tiling is `620`, replacing
  the old `44`/`20` values that repeated textures at roughly hundred-meter
  scale.
- Strengthened terrain vertex coverage masks for forest floor, wet rock, and
  scree so the generated corridor terrain is no longer mostly unclassified base
  rock.
- Increased safe-distance canopy mass without moving trees into the first-person
  track view. An attempted near-track 16m clearance pass caused a gray occlusion
  read and was rejected; the kept pass starts canopy at 30m clearance.

Visual read:

- Water/shore is clearly better than `river-footprint-clearance-v1`: open-water
  fake boulders are gone, the river is wider, and the surface has readable
  longitudinal rapid streaks instead of a flat narrow road.
- Foreground track now reads less like toy red pipes. It is still not a final
  coaster/train asset, but it gives the frame a more believable first-person
  structural anchor.
- The latest frame is still not AAA. The left near wall dominates as a dark,
  smooth foreground mass, and the far canyon still reads as repeated rock chunks
  over terrain instead of continuous authored geology.
- This was an effective foreground pass, not a failed one. Remaining issues are
  now near-wall composition, continuous cliff forms, and a real cockpit/car nose.
- The latest rock cleanup is a bug/pipeline fix, not a final visual win. It
  removes the floating/fake rock-block read, but exposes how much of the image
  is still smooth corridor terrain. The frame is cleaner but more barren.
- The latest terrain/material pass is also a pipeline fix, not a final visual
  win. It restores correct color composition and readable material scale, but
  t30 still reads as a smooth corridor valley with sparse tree points. This
  confirms the next bottleneck is authored continuous slope/cliff geometry, not
  another coverage or scatter-density tweak.

## Next Task

Highest-return next task:

**Build a real continuous authored slope/cliff surface to replace the now-exposed
smooth corridor terrain.**

Scope:

- Treat `slope-gated-rockwall-v1` as the clean baseline after removing fake
  rock clutter.
- Do not re-enable `cliff_belt` or loosen rock-wall slope gates to hide naked
  terrain.
- Add continuous authored geology/forest-floor coverage: large coherent cliff
  forms and slope geometry first, then scree/wet-rock/canopy detail on top.
- Continue foreground coaster work only after the macro terrain/cliff read is
  no longer a smooth corridor.

Do not add screenshot-specific exclusion lists or more water boulders.

## Active Risks

- Smooth corridor terrain is still the main visual surface in several hero frames.
- Rock-wall segments are cleaner and less bug-prone after grounding/slope gates,
  but they are now sparse; continuous authored wall geometry is still missing.
- Water is visible, wider, and better contacted to the riverbed. It still needs
  better reflection/depth variation and localized foam, but it is no longer the
  only blocker.
- Lack of real cockpit/train foreground still hurts first-person scale and photo
  read, although the rail proxy is now less toy-like.
- Long docs were archived on 2026-06-23; if a future task needs old rationale,
  search `docs/archive/` deliberately instead of reading it by default.

## Update Rule

For each iteration, add only:

- command
- contact sheet
- build/test result
- visual verdict after opening the image
- concrete next task

When this file exceeds roughly 150 lines, archive old entries and keep only the
current dashboard.
