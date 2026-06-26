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
.\scripts\test-yarlung.ps1 -Build
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -NamePrefix rockwall-mass-upright-v1
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -NamePrefix rockwall-mass-upright-v2
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -NamePrefix rockwall-canopy-mass-v3
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -NamePrefix rockwall-canopy-balanced-v4
.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Focus -Build -NamePrefix canopy-scale-v5
.\scripts\test-yarlung.ps1 -Build
```

Evidence:

- Contact sheets: `Saved/Diagnostics/rockwall-mass-upright-v2.png`,
  `Saved/Diagnostics/rockwall-canopy-mass-v3.png`,
  `Saved/Diagnostics/rockwall-canopy-balanced-v4.png`,
  `Saved/Diagnostics/canopy-scale-v5.png`
- Latest focus manifest: `Saved/Diagnostics/canopy-scale-v5-run.json`
- RiskGate: `WARN`
- Latest focus t30: `canopy-scale-v5-t30.png`, risk `1.270`, flat `0.666`,
  green `0.048`
- Map inspect: `canopy-scale-v5` had 0 errors, 0 warnings
- Automation: `.\scripts\test-yarlung.ps1 -Build` passed 15/15

Implemented:

- Rejected a generated near-slope drape mesh experiment. It entered the map, but
  visually repeated the old "view-dependent skin" failure: smooth corridor
  terrain remained the hero surface.
- Reworked rock-wall profiles from small surface-aligned modules into larger,
  upright, deeply embedded cliff masses. Tests now enforce that authored cliff
  faces do not lie on the slope and that the right near-slope segment samples
  every track segment.
- Tuned the rock-wall density back from the over-busy `rockwall-mass-upright-v2`
  and the too-barren `rockwall-canopy-mass-v3` to the current balanced pass:
  `1942` total rock-wall instances.
- Increased forest-floor and canopy-mass scale while preserving the previously
  validated 30m near-track canopy clearance, avoiding the old near-track black
  tree-wall failure mode.

Visual read:

- `canopy-scale-v5` is the best point from this slice: less naked slope than the
  prior baseline, more readable right-side cliff/canopy mass, and no near-track
  tree-wall occlusion.
- It is still not AAA. The left foreground slope remains too smooth and empty,
  and some right-side cliff modules still read as placed assets rather than
  continuous geology.
- Important lesson kept live: when a high-ceiling direction looks bad, check for
  scale, placement, grounding, material, or pipeline bugs before rejecting the
  whole direction. The failed drape mesh was a low-ceiling representation; the
  upright cliff/canopy mass route remains viable but needs authored set pieces.

## Next Task

Highest-return next task:

**Author a camera-segment left-slope forest/cliff set piece for the first-person
t30 corridor.**

Scope:

- Treat `canopy-scale-v5` as the baseline.
- Do not re-add generated drape-surface skinning, old `cliff_belt`, or
  heightfield strata relief.
- Add an authored segment system keyed by track sample/time window, not another
  global scatter pass: left foreground cliff/canopy masses first, then
  forest-floor/scree/wet-rock detail.
- Preserve the 30m near-track canopy clearance unless a screenshot proves the
  old black-wall bug is not returning.
- Continue foreground coaster work only after the macro terrain/cliff read is
  no longer a smooth corridor.

Do not add screenshot-specific exclusion lists or more water boulders.

## Active Risks

- Smooth corridor terrain is still the main visual surface in several hero frames.
- Current global rock-wall/canopy profiles improve the right-side read but do
  not author the left foreground composition. More global density is likely
  lower return than a segment-aware set piece.
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
