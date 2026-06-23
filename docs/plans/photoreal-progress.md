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
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix rockwall-segments-v2 -Times 60,120,180,240
```

Evidence:

- Contact sheet: `Saved/Diagnostics/rockwall-segments-v2.png`
- Manifest: `Saved/Diagnostics/rockwall-segments-v2-run.json`
- RiskGate: `WARN`
- Worst frame: `rockwall-segments-v2-t60.png`, risk `1.216`
- Map inspect: 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 14/14

Implemented:

- Deleted one-off rock-wall patch interfaces:
  `hero_rock_wall_groups`, `foreground_rock_apron_groups`,
  `FYarlungRockWallGroupConfig`, `AddRockWallGroups()`.
- Replaced them with `rock_wall_profiles` + `rock_wall_segments`.
- Renamed `hero_rock_wall_only` to `rock_wall_source` so mesh-source semantics are
  clear.
- Added fail-close validation for profile references, component names, side
  coverage, and full ride-corridor coverage.

Visual read:

- Better continuous canyon rock massing, especially in high valley views.
- Still visibly non-AAA: exposed smooth gray-green corridor terrain remains in
  t60/t120/t180; forest and ground layers are not continuous; water still lacks
  photo-level flow/shore detail; foreground coaster hardware is too proxy-like.

## Next Task

Highest-return next task:

**Build a systemic ground/canopy coverage layer** that makes the corridor terrain
stop carrying the hero mountain surface.

Scope:

- Forest-floor / scree / talus near visible slopes.
- Wet-rock shore transition along river edges.
- Canopy massing: far canopy blocks, mid whole trees, near understory.
- Use config-driven, fail-close placement; avoid one-off named patch lists.
- Validate with `Actor` or `Terrain` mode depending on whether geometry/material
  changes are required.

Do not simply increase rock-wall instance counts.

## Active Risks

- Smooth corridor terrain is still the main visual surface in several hero frames.
- Rock-wall segments are cleaner than patch groups, but they are still instance
  massing rather than continuous authored wall geometry.
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
