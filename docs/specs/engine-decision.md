# Engine Decision

## Decision

Use Unreal Engine 5 as the primary engine. Do not build the first version as a
web or Three.js application.

## Rationale

The project goal is a high-ceiling coaster simulator with realistic first-person
ride feel and strong visual presentation. Unreal Engine 5 gives the project a
better long-term ceiling for:

- Native desktop rendering quality.
- High-detail terrain, lighting, shadows, fog, materials, and post-processing.
- Editor tooling for authored environments, cameras, debug UI, and later track
  authoring.
- VR investigation after the desktop ride is stable.
- C++ control over deterministic simulation code.

Three.js is useful for shareable prototypes, but it makes the highest-quality
version harder: browser frame pacing, rendering feature limits, asset pipeline
constraints, and VR stability become project risks. The project no longer needs
web distribution, so those tradeoffs are not worth taking.

## Physics Boundary

Unreal is the renderer and runtime shell. It does not replace the coaster
physics model.

The core coaster solver remains custom C++:

- Track centerline and arc-length sampling.
- Parallel-transport frames.
- Banking, curvature, slope, and section metadata.
- Fixed-step train motion along the track.
- Gravity, drag, rolling resistance, lift, launch, trim, brake, and block
  behavior.
- Telemetry for speed, acceleration, G-force, jerk, and validation.

Chaos Physics may be used for collision, scenery interactions, editor gizmos,
and secondary effects. It is not the authoritative ride motion solver.

## Consequences

- The repository is a UE5 project instead of a Node/Vite project.
- CI and tests should use Unreal Automation Tests rather than browser tests.
- Visual validation should use scripted UE camera captures.
- Development requires Unreal Engine installed locally.
- A web previewer is out of scope unless explicitly reintroduced later.

## Rejected Alternatives

### Three.js

Rejected because the project no longer needs web delivery and the visual,
editor, and VR ceiling is lower for this use case.

### Unity

Viable, but not preferred. Unity has good C# iteration speed and a mature
physics stack, but Unreal has the stronger out-of-box high-end rendering and
world-building toolchain for this target.

### Godot

Viable for open-source native development, but not preferred for this target
because the project prioritizes maximum visual ceiling and mature high-end 3D
production tooling.

### Custom Engine

Rejected because it would spend too much time on renderer, editor, asset
pipeline, platform, and tooling foundations before proving the coaster model.
