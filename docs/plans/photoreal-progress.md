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
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix steep-grounded-rocks-v1 -Times 60,120,180,240
```

Evidence:

- Contact sheet: `Saved/Diagnostics/steep-grounded-rocks-v1.png`
- Manifest: `Saved/Diagnostics/steep-grounded-rocks-v1-run.json`
- RiskGate: `FAIL`
- Worst frame: `steep-grounded-rocks-v1-t60.png`, risk `1.483`
- Map inspect: 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 15/15

Implemented:

- Added explicit terrain coverage masks in corridor vertex color channels:
  wet-rock shore, forest-floor, and scree/rock coverage.
- Rebuilt `M_YarlungMeshTerrain` so those masks drive continuous terrain
  material blending instead of treating vertex color as final albedo.
- Fixed rock/cliff placement bugs exposed by the screenshots:
  rock-wall/cliff/river-wall instances now use upright yaw plus thick geologic
  scaling instead of thin slab scaling, and every scenery instance grounds its
  pivot from `StaticMesh` bounds bottom rather than assuming pivot-at-ground.
- Tightened large cliff/rock-wall placement: big rock massing now requires
  steeper surfaces, larger track clearance, and negative embed offsets. Low
  smooth slopes should be covered by material/scree/forest-floor systems, not
  free-standing cliff slabs.

Visual read:

- The previous "thin paper / horizontal plank" rock bug is removed from the
  validated contact sheet.
- The large floating-rock read was a real placement bug: large cliff assets
  were allowed on low smooth slopes and only sampled one pivot point. The
  stricter placement pass removes the worst false visual richness.
- Still visibly non-AAA: the result is more honest but emptier. Large smooth
  green-gray corridor terrain remains the dominant hero surface, especially
  t60/t120/t180.
- Water still lacks photo-level flow/shore detail; foreground coaster hardware
  remains too proxy-like.

## Next Task

Highest-return next task:

**Replace low-slope naked corridor terrain with continuous authored surface
coverage**, without reintroducing free-standing cliff slabs.

Scope:

- Tune the new terrain coverage masks/material so low slopes read as wet
  forest-floor, scree, and wet-rock bands instead of green primer.
- Add only ground-hugging scree/small-rock/decal-like cover on low slopes; keep
  large cliff modules on steep slopes and river walls.
- Rebuild canopy as distant/mid massing, not slope patches or branchy near
  foreground fillers.
- Validate in `Terrain` or `Full` mode, then first-person contact sheet. Treat
  new floating/hovering/sheet-like assets as pipeline bugs, not aesthetic
  tradeoffs.

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
