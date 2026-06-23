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
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix surface-cover-v3 -Times 60,120,180,240
```

Evidence:

- Contact sheet: `Saved/Diagnostics/surface-cover-v3.png`
- Manifest: `Saved/Diagnostics/surface-cover-v3-run.json`
- RiskGate: `WARN`
- Worst frame: `surface-cover-v3-t60.png`, risk `1.342`
- Map inspect: 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 15/15

Implemented:

- Replaced old `canopy_belt` / `ground_cover_belt` placement with reusable
  `surface_cover_profiles`.
- Added systemic profiles for `wet_rock_shore`, `talus_scree`, `forest_floor`,
  and `canopy_mass`, with track/river anchoring and explicit clearance gates.
- Added fail-close config validation and automation coverage for surface-cover
  profile references.
- Fixed scenery tint application to cover every material slot, so multi-slot
  shrubs/trees do not leak mismatched branch/trunk materials after tinting.
- Hardened map inspect so real shrub asset validation checks static mesh source,
  not material names that can legitimately change after tinting.

Visual read:

- Shore rocks and scree are more systematic and less one-off.
- PN spruce assets are usable only as a limited far canopy hint in this setup;
  near/mid use reads as individual branchy silhouettes, not AAA forest mass.
- Still visibly non-AAA: large smooth gray-green corridor terrain remains the
  dominant hero surface in t60/t120/t180. HISM layers alone are not enough.
- Water still lacks photo-level flow/shore detail; foreground coaster hardware
  remains too proxy-like.

## Next Task

Highest-return next task:

**Build a continuous terrain material coverage layer** so forest-floor / scree /
wet-rock shore read as surfaces, not isolated HISM instances.

Scope:

- Generate authored masks or vertex-color bands for forest floor, talus/scree,
  and wet-rock shore from the same river/track field used by surface cover.
- Feed those masks into the mesh terrain material so the corridor base stops
  showing as smooth green-gray primer.
- Keep canopy as a restrained far/mid layer until better tree assets/materials
  are available.
- Validate in `Terrain` or `Full` mode, then first-person contact sheet.

Do not simply increase HISM instance counts.

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
