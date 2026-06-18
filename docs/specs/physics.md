# Physics Spec

## Objective

Create a coaster physics model that is believable, deterministic, inspectable,
and accurate enough to drive first-person ride feel. The model should start with
a constrained path solver and evolve toward richer train dynamics only where
the extra complexity improves the experience.

## Coordinate Model

Track geometry is represented as a continuous centerline curve parameterized by
arc length `s`.

For each sampled position:

- `p(s)`: world position.
- `t(s)`: tangent direction.
- `n(s)`: transported normal.
- `b(s)`: binormal.
- `bank(s)`: roll angle around tangent.
- `kappa(s)`: curvature magnitude.
- `grade(s)`: slope relative to gravity.

Use a parallel-transport frame rather than a raw Frenet frame to avoid sudden
camera twists near low-curvature regions. Banking is applied as a separate
authoring channel.

## Initial Motion Model

M1 uses a 1D constrained motion model along the track:

```text
dv/dt = a_drive + a_gravity - a_drag - a_rolling - a_brake
ds/dt = v
```

Where:

- `a_gravity = dot(gravity, t)`.
- `a_drag = Cd * v^2`, calibrated per train profile.
- `a_rolling = Crr * sign(v)`, with optional speed-dependent terms.
- `a_drive` comes from lift or launch sections.
- `a_brake` comes from trims, final brakes, and block control.

The train is constrained to the track. It does not derail in M1. Later
milestones can add derailment only as an editor validation warning or sandbox
mode.

## Section Types

### Station

- Holds train until dispatch.
- Uses low-speed drive tires when moving.
- Supports loading-state simulation later.

### Lift

- Applies target lift speed through a motor constraint.
- Includes chain anti-rollback audio and tooth cadence.
- Can optionally enforce minimum speed.

### Launch

- Applies acceleration up to target speed or end of section.
- Supports linear and shaped acceleration curves.

### Coast

- No powered acceleration.
- Gravity, drag, and rolling resistance dominate.

### Trim

- Applies capped deceleration to approach target speed.
- Should never reverse the train.

### Brake

- Applies stronger deceleration with comfort limits.
- Can stop and hold the train at block boundaries.

### Block

- Reserves a track interval for one train.
- Prevents dispatch or releases brakes based on downstream occupancy.

## G-Force Estimation

Telemetry should estimate rider forces in train-local axes:

- Vertical G: force along seat-up direction.
- Lateral G: side force across the seat.
- Longitudinal G: launch/brake force along the train.

For M1:

```text
a_world = d(v * t) / dt
seat_force = a_world - gravity
g_local = transform_to_train_local(seat_force) / 9.80665
```

Curvature contribution:

```text
a_curvature = v^2 * kappa * curve_normal
```

Banking rotates the perceived lateral/vertical split. This makes banking
authoring meaningful even before full wheel dynamics exist.

## Comfort and Realism Targets

These are not hard safety limits, but validation warnings:

- Sustained vertical G: prefer -0.5g to +4.5g.
- Short vertical G peaks: warn above +5g.
- Sustained lateral G: prefer below 1.5g after banking.
- Launch acceleration: warn above 1.5g sustained.
- Brake deceleration: warn above 1.3g sustained.
- Jerk spikes should be highlighted because they break ride feel.

## Train Model Roadmap

### M1: Point-Mass Train

- One simulated reference point rides the track.
- Cars are placed by offsetting along arc length behind the reference point.
- Visual cars follow the track frame independently.

### M2: Multi-Car Approximation

- Train mass is distributed across cars.
- Gravity and drag contributions sample each car.
- Long trains cresting hills behave more plausibly.

### M3+: Constraint-Based Train

- Wheel assemblies and car joints become explicit simulation constraints.
- This requires more numerical stability work and should be added only after
  the simpler model is fully validated.

## Numerical Requirements

- Fixed timestep simulation, independent of render framerate.
- Deterministic replay for the same track and initial conditions.
- Arc-length lookup table for stable speed and camera placement.
- Unit tests for energy conservation on simple tracks.
- Golden telemetry snapshots for showcase tracks.

## Validation Tracks

- Straight flat coast: verifies drag and rolling resistance.
- Lift-drop-hill: verifies gravity and energy behavior.
- Circular turn with banking: verifies lateral/vertical G split.
- Launch-brake segment: verifies powered acceleration and braking.
- Low-curvature crest: verifies camera frame stability.
