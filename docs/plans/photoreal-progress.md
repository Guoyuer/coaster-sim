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
.\scripts\iterate-yarlung.ps1 -Mode Terrain -Preset Standard -Build -NamePrefix low-slope-coverage-v3
.\scripts\iterate-yarlung.ps1 -Mode Material -Preset Standard -Build -NamePrefix cleanup-material-v1
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix authored-wall-massing-v1
.\scripts\iterate-yarlung.ps1 -Mode ScreenshotOnly -Preset Focus -Build -NamePrefix authored-wall-focus-v2
```

Evidence:

- Contact sheets: `Saved/Diagnostics/low-slope-coverage-v3.png`,
  `Saved/Diagnostics/cleanup-material-v1.png`,
  `Saved/Diagnostics/authored-wall-massing-v1.png`,
  `Saved/Diagnostics/authored-wall-focus-v2.png`
- Manifest: `Saved/Diagnostics/authored-wall-focus-v2-run.json`
- RiskGate: `FAIL`
- Worst frame: `authored-wall-focus-v2-t30.png`, risk `1.531`
- Map inspect: `authored-wall-massing-v1` had 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 15/15

Implemented:

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
- `authored-wall-focus-v2` is faster and shows more near-wall presence, but t30
  still has a large smooth left foreground slope. That is the next visual bug
  to attack.
- Water still lacks photo-level flow, foam, reflection, and wet shore breakup;
  foreground coaster hardware remains too proxy-like.

## Next Task

Highest-return next task:

**Fix the t30 foreground slope read, then confirm with Standard.**

Scope:

- Use `.\scripts\iterate-yarlung.ps1 -Mode ScreenshotOnly -Preset Focus -Build`
  for config/Actor visual tuning while targeting t30.
- Do not keep adding small rock chunks. The t30 left foreground needs a system
  that reads as continuous ground-hugging rock/scree/forest-floor cover or a
  better composed foreground wall.
- Once the Focus frame looks materially better, validate with `Actor` or
  `ScreenshotOnly` `Standard` and open the contact sheet.

Do not simply increase HISM counts or relax cliff slope/clearance gates.

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
