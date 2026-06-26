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
.\scripts\iterate-yarlung.ps1 -Mode Terrain -Preset Focus -Build -NamePrefix neartrack-rock-breakup-v2
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -NamePrefix t30-near-apron-v1
.\scripts\iterate-yarlung.ps1 -Mode ScreenshotOnly -Preset Standard -Build -NamePrefix t30-near-apron-v1-standard
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -Times 90 -NamePrefix river-footprint-clearance-v1
.\scripts\test-yarlung.ps1 -Build
```

Evidence:

- Contact sheets: `Saved/Diagnostics/neartrack-rock-breakup-v2.png`,
  `Saved/Diagnostics/t30-near-apron-v1.png`,
  `Saved/Diagnostics/t30-near-apron-v1-standard.png`,
  `Saved/Diagnostics/river-footprint-clearance-v1.png`
- Manifest: `Saved/Diagnostics/t30-near-apron-v1-standard-run.json`
- Latest focus manifest:
  `Saved/Diagnostics/river-footprint-clearance-v1-run.json`
- RiskGate: `FAIL`
- Standard worst frame: `t30-near-apron-v1-standard-t90.png`, risk `1.520`
- Latest focus t90: `river-footprint-clearance-v1-t90.png`, risk `1.614`
- Focus t30 improved from risk `1.549` / flat `0.775` to risk `1.165` /
  flat `0.545`
- Map inspect: `river-footprint-clearance-v1` had 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 15/15

Implemented:

- Added a t30 authored slope set-piece using reusable rock-wall profiles:
  `foreground_slope_patch` for larger midground rock mass and
  `foreground_near_apron` for close ground-hugging breakup. The first attempt
  missed because it was incorrectly placed at opening samples; the live segment
  is now aligned to the actual t30 travel distance from `CoasterStartRatio=0.34`.
- Added near-track terrain coverage breakup so first-person-adjacent slopes
  reduce forest-floor dominance and gain wet-rock/scree masks. It is only a
  base-layer improvement; geometry coverage is still doing most of the visible
  t30 work.
- Fixed a material pipeline bug exposed by screenshot fail-close:
  `M_YarlungWaterSurface` still contained an invalid UE 5.8
  `SingleLayerWaterMaterialOutput` graph, so `-game` screenshots compiled it
  to Default Material. The Python generator now rebuilds the generated water
  material graph in-place and emits a simpler transparent Default Lit water
  material that compiles reliably.
- Removed the temporary delete/recreate material path. Generated materials now
  reuse existing `Material` assets and fail loudly if the asset type is wrong,
  avoiding UE reference-gather warnings during routine material iterations.
- Tuned low-slope terrain coverage so forest-floor no longer dominates the
  whole corridor. Wet-rock and scree masks now carry more of the canyon floor,
  and material detail contributes more to final terrain albedo.
- Preserved the previous large-rock hardening: no visible return of the
  paper-thin slab or large floating-rock bug in the validated contact sheet.
- Restored the rock-wall massing path from sparse placement to continuous
  coverage. Rock-wall instances went from 450 in the previous validated map to
  4696 in `authored-wall-massing-v1`; near-slope wall profiles can now align to
  terrain while mid/far wall profiles stay upright and river-facing.
- Added a fast visual loop: `ScreenshotOnly -Build` now compiles without map
  import, and the new `Focus` preset captures the current worst frame at
  960x540. The first check ran in 19s versus roughly 107s for the Actor
  Standard loop.
- Added footprint-aware river clearance for all scenery placement paths. River
  clearance now uses `distance_to_river - scaled_mesh_radius`, so large boulder,
  cliff, scree, and rock-wall meshes cannot pass validation with only their pivot
  outside the protected water/shore band.
- Recast `wet_rock_shore` from large dry boulders to smaller, lower, farther
  bank stones. This removes the worst "rock sitting on water" read, but does
  not solve the larger water/shore realism problem by itself.

Visual read:

- The water material bug is fixed: screenshots no longer trip the default
  material gate, material import reports 0 warnings, and water remains visible.
- Low slopes are less green-primer and more wet-rock/scree-gray than
  `steep-grounded-rocks-v1`, but the macro read is still too smooth and
  terrain-driven. This is a partial improvement, not AAA.
- Rock placement remains cleaner, but the composition is still sparse; the
  hero view needs authored wall/slope set pieces, not more global scatter.
- `authored-wall-massing-v1` improves t90/t150 by breaking up smooth slopes, but
  it still reads as many rock chunks rather than continuous geology.
- `t30-near-apron-v1` materially improves the t30 foreground slope read: the
  pure smooth plane is now broken by rock massing and close apron geometry.
  It is still not AAA because it reads as placed chunks over terrain, not a
  continuous authored forest-floor/scree/wet-rock surface.
- Standard validation moves the worst frame to t90. t90/t150 still show large
  dark or green corridor slopes and need the same set-piece/continuous-surface
  treatment across their real travel-distance ranges.
- `river-footprint-clearance-v1` removes the most fake water boulders, but the
  frame reads worse overall because the real bottleneck is now exposed: water is
  still a smooth synthetic strip, the left near wall overpowers the composition,
  and the shore lacks authored wet-rock/foam breakup.
- Water still lacks photo-level flow, foam, reflection, and wet shore breakup;
  foreground coaster hardware remains too proxy-like.

## Next Task

Highest-return next task:

**Change the water/shore approach before adding more rocks. Build a systemic
rapid-water + wet-shore composition layer instead of placing visible boulders in
the river.**

Scope:

- Add flow/foam/breakup to the existing generated river surface so deep canyon
  water reads as rapid glacial water, not a flat road-like ribbon.
- Move visible stones out of open water. Allow only small, dark, wet, embedded
  shore stones or partially submerged rapid hints with foam interaction.
- Reduce left near-wall dominance with a route/composition-level solution, not
  screenshot-specific exclusions.
- Continue authored slope/wall work after the water/shore layer stops looking
  synthetic.

Do not add screenshot-specific exclusion lists or more water boulders.

## Active Risks

- Smooth corridor terrain is still the main visual surface in several hero frames.
- Rock-wall segments are cleaner and less bug-prone after grounding/slope gates,
  but they are now sparse; continuous authored wall geometry is still missing.
- Water is visible and better contacted to the riverbed, but still reads too flat
  and synthetic.
- Lack of real cockpit/train foreground hurts first-person scale and photo read.
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
