# 2026-06-22 Scenery / Terrain Height-Model Divergence

- Date: 2026-06-22
- Branch: `codex/yarlung-aaa-visual-route`
- Found by: code review (visual + pipeline pass), no runtime repro yet
- Status: RESOLVED — shared terrain-surface sampler landed and validated
- Reviewed artifacts: `YarlungSceneryActor.cpp`, `YarlungCorridorImportCommandlet.cpp`,
  `YarlungCorridorProfile.cpp`, `YarlungTerrainRelief.cpp`, `YarlungRiverField.cpp`,
  `YarlungViewCorridor.cpp`, `Config/yarlung-assets.json`, `Config/yarlung-terrain.json`,
  `scripts/analyze-offscreen-shots.py`

> These are review findings, not confirmed-on-screen. The magnitudes below are derived
> from the code + config, so the P0 should reproduce on the canyon walls; verify with
> multi-angle off-rails shots, not just the first-person path.

---

## Resolution

Fixed in `codex/yarlung-aaa-visual-route` by extracting the rendered corridor surface into
`YarlungTerrainSurface` and making both the terrain mesh builder and scenery placement use
that same analytic chain:

- cubic source height sample
- 900 cm source normal sample
- nearest track profile frame
- relaxed/blended corridor profile
- terrain relief displacement
- river channel carve
- final `+25 cm` render lift

`AYarlungSceneryActor::TryResolvePlacement()` now samples `YarlungTerrainSurface::SurfaceZCm()`
and `SurfaceNormal()` instead of its old bilinear source DEM plus full profile height. This
fixes P0 and P1 at the source rather than by per-asset offsets.

P2 is fixed by centralizing carved-channel and visible-ribbon half-width constants on
`FYarlungRiverField`, with an automation test asserting the visible water ribbon remains
inside the carved channel. P3's boulder scale jitter now includes `Seed`, matching the rest of
the scatter hash calls. P4 remains documented as a process rule: `analyze-offscreen-shots.py`
is a visual triage heuristic, not a geometry/placement correctness gate.

Validation:

- `.\scripts\test-yarlung.ps1 -Build` passed: 12/12 automation tests.
- `.\scripts\iterate-yarlung.ps1 -Mode Actor -Preset Standard -Build -NamePrefix terrain-surface-parity-v1`
  passed import, map inspect, and visual survey.
- Contact sheet inspected manually: `Saved\Diagnostics\terrain-surface-parity-v1.png`.

## P0 🔴 Scenery is placed on a different height model than the rendered terrain mesh

Scenery instances sit at a height that **does not match the terrain surface they are
supposed to rest on**. The two heights are computed by two unrelated code paths, and
nothing keeps them in sync.

### Rendered terrain mesh height (the surface the player sees)

`YarlungCorridorImportCommandlet.cpp:404-422` (`ComputeBaseTerrainPositions`):

```cpp
CorridorProfileHeight = YarlungCorridorProfile::CorridorTerrainHeightCm(...);
RelaxedProfileHeight  = BaseHeight + (CorridorProfileHeight - BaseHeight) * 0.48f;   // only 48% of the profile delta
ProfileBlend          = clamp(ViewCorridorMask,0,1) * Smooth01((TrackDistance - 30000)/52000); // 0 outside view corridor
HeightBeforeRiverCarve = Lerp(BaseHeight, RelaxedProfileHeight, ProfileBlend) + DisplacementCm; // + relief
Height                 = CarveRiverChannelCm(HeightBeforeRiverCarve, ...);                       // river downcut
Positions[i]           = FVector(X, Y, Height + 25.0f);                                          // +25 cm
```

- base sample is **cubic B-spline** (`YarlungHeightAtSourceGrid`, `:67-93`)
- `+ DisplacementCm` = relief (slope-gated, up to `MaxAmplitudeCm 1900` + `CliffFoldMaxAmplitudeCm 2400` cm)
- `CarveRiverChannelCm` lowers bank vertices toward water (bank zone out to `WaterHalfWidthCm + 12000` cm)
- `+ 25` cm constant

### Scenery placement height (where rocks/cliffs/trees are dropped)

`YarlungSceneryActor.cpp:546-552` (`TryResolvePlacement`, used by every placement path):

```cpp
const float BaseHeight      = SampleHeightCm(EncodedHeights, Loc.X, Loc.Y);      // BILINEAR, :415-436
const float TrackBaseHeight = SampleHeightCm(EncodedHeights, Center.X, Center.Y);
OutHeightCm = YarlungCorridorProfile::CorridorTerrainHeightCm(                   // FULL profile
    {Center.X, Center.Y}, SignedOffsetCm, TrackBaseHeight, BaseHeight);
```

Then the instance Z is `OutHeightCm + KindConfig.HeightOffsetCm` (offsets are only 18–50 cm
in `Config/yarlung-assets.json`).

So scenery uses the **full** `CorridorTerrainHeightCm` with:
- **no** `0.48` relaxation,
- **no** `ProfileBlend` / `ViewCorridorMask`,
- **no** relief `DisplacementCm`,
- **no** `CarveRiverChannelCm`,
- **no** `+25`,
- and a **bilinear** base sample vs the mesh's **cubic** sample.

### Why this is large, not cosmetic

`CorridorTerrainHeightCm` (`YarlungCorridorProfile.cpp:16-37`) adds `MacroBreakup` that turns
on with lateral offset: `WallMask` ramps from `AbsOffset` 58000→163000 cm, with `WallLift`
up to `24000 + 36000` and `ButtressLift` up to `16000`, clamped to `+86000` cm.

The scenery bands land squarely in that wall region (`Config/yarlung-assets.json`):
- `cliff_belt.lateral_bands_cm`: `26000 … 220000`
- `slope_patch_belt.lateral_bands_cm`: `12000 … 152000`
- `canopy_belt.lateral_bands_cm`: up to `222000`

Resulting vertical mismatch on the walls:
- Inside the view corridor (`ViewCorridorMask≈1`): mesh gets `0.48 × MacroBreakup`, scenery
  gets the full amount → gap ≈ `0.52 × MacroBreakup` → **~hundreds of meters** in the worst case.
- Outside the view corridor (`ViewCorridorMask=0` → `ProfileBlend=0`): mesh = raw DEM, scenery
  = DEM + full `MacroBreakup` → cliffs/far trees float **up to ~860 m**.
- Plus on slopes: scenery ignores relief (`±~4300` cm) — and cliffs are slope-placed
  (`min_slope 0.02`) exactly where relief is strongest (`SlopeGateStart 0.0`).
- Plus near the river: scenery ignores `CarveRiverChannelCm`, so `RiverbankBoulders` float
  above the downcut bank (carve up to ~12000 cm).

`height_offset_cm` of 18–50 cm cannot absorb any of this. The affected assets are exactly the
hero canyon-wall set: `CliffRockFaces*`, `SlopeRockWall*`, far `CanopyTrees*`, `RiverbankBoulders`.

### Fix direction (for Codex)

The terrain height pipeline is implemented **once**, inline in the commandlet. Extract the full
chain into a single shared function and have **both** the mesh builder and scenery call it, e.g.:

```
float YarlungCorridorProfile::SampledTerrainSurfaceZCm(
    EncodedHeights, TrackPoints, RiverField, WorldXY)
  -> reproduces: cubic base sample
     + Lerp(base, base + 0.48*(profile-base), ViewCorridorMask*Smooth01((trackDist-30000)/52000))
     + ComputeReliefForRiverDistanceCm(...)
     + CarveRiverChannelCm(...)
     + 25
```

Scenery’s `TryResolvePlacement` should set `OutHeightCm` from that shared function so instances
sit on the surface that is actually rendered. (Alternative: line-trace/sample the built terrain
mesh at spawn, but a shared analytic function is cheaper and deterministic with the build.)

Add a guard test: sample N points across the wall bands and assert
`|scenery_height - mesh_height| < small tolerance`. Can hang off the existing
`YarlungTerrainReliefTests` / `YarlungTerrainConfigParityTests`.

---

## P1 🟠 Base sampler + normal step differ between scenery and mesh

Even on the flat valley floor where `MacroBreakup≈0`, the two paths won't coincide:
- mesh base height = cubic B-spline (`Commandlet:67-93`); scenery = bilinear (`SceneryActor:415-436`)
- normal step: scenery `StepCm = 1800` (`:440`) vs mesh `SampleSpacingCm = 900` (`:108`), so the
  slope used for slope-gating placement differs from the rendered slope.

Folding this into the P0 shared function fixes both. At minimum, match the interpolation and step.

---

## P2 🟡 Un-synced water-width magic numbers

The visible water ribbon uses `HalfWidthCm * 0.34, clamp(3600, 8200)`
(`Commandlet:779`), while the river field's `WaterHalfWidthCm` (used for `CarveRiverChannelCm`
and scenery river clearance) uses `* 0.38, clamp(4000, 9000)` (`YarlungRiverField.cpp:149`).
Looks intentional (keep the ribbon inside the carved channel), but the two constant sets have no
shared source — change one and the water silently overflows the carve or exposes the bed. Bind
them to one config/constant with a comment.

---

## P3 🟡 `Hash01` inconsistencies (determinism only, not a crash)

`YarlungSceneryActor.cpp:636`: boulder scale uses `Hash01(Index*9.0f, 1.0f)` /
`Hash01(Index*7.0f, 2.0f)` **without `+ Seed`**, unlike every other call in the function — multiple
boulder components share the same scale-jitter pattern. Also `Hash01` is `Frac(sin(big)*k)`; with
belt `SampleIndex` in the thousands the `sin` argument loses float precision and scatter quality
degrades along the track. Low priority; fix the missing `Seed` while in the file.

---

## P4 🟡 Visual-risk analyzer cannot catch P0 (and does not gate)

`scripts/analyze-offscreen-shots.py` is triage only: `main()` (`:242-248`) just sorts and prints
`worst` — **no threshold ever fails the run**. Its heuristics (`washed` needs `luma>148`, `flat`
needs `stddev<6`, `dark` needs `luma<42`) measure exposure/greenness/flatness, not spatial
correctness. A correctly-lit frame with **floating rocks** (the P0 bug) scores low risk and passes.
Do not use this as a regression gate for geometry/placement bugs; it answers a different question.

---

## Suggested fix order

1. P0 — extract shared terrain-surface-Z function, repoint scenery, add parity test.
2. P1 — fold base-sample + normal-step into the same function.
3. P2 / P3 — small cleanups while touching these files.
4. P4 — note in review docs that the analyzer is not a placement-correctness gate.
