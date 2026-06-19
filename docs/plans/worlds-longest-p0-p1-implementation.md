# P0/P1 Implementation Plan

> Scope: first implementation slice for `docs/plans/worlds-longest-coaster.md`.
> Goal: establish clean runtime boundaries and an offline generated long-track
> artifact without taking on full P2 runtime CSV integration yet.

## P0 Runtime Refactor: Behavior-Preserving Track Core

Intent: prepare the C++ runtime for a generated 5km track while preserving the
current 8-point short-loop behavior.

Steps:

1. Add a track component boundary:
   - `UCoasterTrackComponent` derives from `USplineComponent`.
   - It owns rebuild/sample helpers for the centerline.
   - It exposes `GetTrackLengthCm`, `SampleBaseFrame`, and legacy section lookup.
2. Add typed section vocabulary:
   - `ECoasterSection` enum replaces raw string decision logic internally.
   - Telemetry can continue exposing `FName` for HUD compatibility.
3. Extract banking to a pure helper:
   - P0 keeps the legacy sinusoidal formula exactly.
   - P3 will replace the implementation with curvature/speed-driven banking.
4. Keep actor behavior stable:
   - Existing `ControlPoints`, command-line start ratio/seconds, speed physics,
     visual rebuild loops, and camera transform remain equivalent.
   - No CSV loading in P0.
5. Verification:
   - Compile C++.
   - Run one offscreen smoke frame at the existing hero time.
   - Do not score photoreal progress from P0 unless visuals are actually
     inspected.

Exit criteria:

- Build passes.
- Short-loop runtime still starts and advances.
- Track/section/banking logic has a named boundary for P2.

## P1 Offline Track Generator And Validator

Intent: generate and verify the new world-longest scenic coaster track as a
data artifact, independent of Unreal runtime integration.

Steps:

1. Add `scripts/generate-yarlung-track.py`:
   - Read `Content/Generated/YarlungLandscape/manifest.json` and `.r16`.
   - Extract an approximate thalweg from the real heightfield, not the old
     synthetic `river_center_y` fallback.
   - Build a 5km out-and-back route:
     `Station -> Lift -> Outbound -> Turnaround -> Return -> Launch -> Brake`.
   - Write `Content/Generated/YarlungLandscape/YarlungTrack.csv`.
   - Update manifest `track` metadata.
   - Emit a hillshade overlay preview for human inspection.
2. Add `scripts/verify-track-clearance.py`:
   - Read `YarlungTrack.csv` and the same `.r16`.
   - Densely sample the generated path.
   - Report clearance, grade, approximate curvature radius, and estimated G.
   - Write CSV/PNG diagnostics under `Saved/Diagnostics`.
   - Exit non-zero if clearance or comfort gates fail.
3. Verification:
   - Run generator.
   - Run validator.
   - Check `TrackLengthCm >= 250000` and target near `500000`.
   - Confirm the preview exists for human review.

Exit criteria:

- `YarlungTrack.csv` exists and is diffable.
- `manifest.json` has a `track` block.
- Validator passes or clearly reports actionable violating segments.

## P2 Boundary

P2 starts only after P0 and P1 are both usable:

- Runtime loads `YarlungTrack.csv`.
- Section lookup switches from legacy ratios to distance ranges.
- The generated CSV is required for the Yarlung runtime path; missing or invalid
  CSV is a hard error, not an automatic fallback to the legacy short loop.
