# Validation Plan

## Purpose

Realistic claims must be backed by checks. This plan defines how physics,
visuals, and ride feel will be evaluated before features are considered done.

## Physics Validation

### Unit Checks

- Arc-length lookup maps distance to position monotonically.
- Parallel-transport frames remain continuous through low-curvature track.
- Gravity accelerates downhill and decelerates uphill.
- Drag reduces speed on flat track.
- Brake sections approach target speed without reversing the train.
- Launch sections hit target speed within tolerance.

### Golden Tracks

Each validation track stores expected telemetry bands:

- `flat-coast`: speed should decay smoothly.
- `lift-drop`: energy behavior should match height loss minus losses.
- `banked-turn`: lateral G should reduce as banking approaches ideal angle.
- `launch-brake`: acceleration/deceleration should match section settings.
- `airtime-hill`: vertical G should dip near crest without camera instability.

### Telemetry Output

Every run should be exportable as CSV or JSON with:

- Time.
- Arc length.
- Position.
- Speed.
- Height.
- Section id/type.
- Tangent, normal, binormal.
- Slope.
- Curvature.
- Vertical/lateral/longitudinal G.
- Jerk estimate.

## Visual Validation

### Screenshot Checks

Capture fixed camera screenshots for:

- Full track overview.
- First drop.
- Banked turn.
- Station.
- First-person frame.

Screenshots should verify:

- Scene is nonblank.
- Track is continuous.
- Train is visible and correctly scaled.
- No obvious UI overlap.
- Shadows/fog/lighting preserve readability.

### Motion Checks

Record short clips or sample frames at high-speed sections:

- No camera roll snapping.
- No train jitter.
- No rail/tie popping.
- No terrain/support clipping visible from normal cameras.

## Ride Feel Review

For each showcase track iteration, capture notes:

- Does speed feel consistent with drop height?
- Are hills and turns readable from the first-person camera?
- Does banking reduce uncomfortable lateral motion?
- Are brakes and launches too abrupt?
- Does the camera make the rider feel attached to the train?

## Done Criteria for M1

- All physics unit checks pass.
- Golden track telemetry stays within documented tolerance.
- Showcase track completes without NaN or section-state errors.
- Desktop browser render stays stable enough for a full run.
- First-person ride has no camera flips or major clipping.
- HUD telemetry updates continuously and matches exported samples.
