# Coaster Sim

An original high-fidelity 3D roller coaster simulator targeting realistic ride
feel, inspectable physics, and high-end native rendering with Unreal Engine 5.

This project is not a clone of any existing commercial simulator. It aims to
build an original coaster simulation stack with explicit physical assumptions,
measurable visual targets, and iterative validation.

## Initial Scope

- Native desktop 3D simulation using Unreal Engine 5 and C++.
- First-person onboard ride camera plus free and trackside cameras.
- Spline-based track geometry with banking, lift, launch, brake, and block
  sections.
- Physics model that exposes speed, acceleration, curvature, slope, and
  estimated rider G-forces.
- Procedural rails, ties, supports, terrain, sky, atmosphere, lighting, and
  ride audio.
- Later editor support for creating, validating, saving, and loading tracks.

## Engine

The project is now a native Unreal Engine 5 C++ project. The `.uproject` is
associated with UE 5.8 by default, but the specs should remain valid for a
nearby UE5 version if the project file is reassociated locally.

## Specs

- [Product Spec](docs/specs/product.md)
- [Physics Spec](docs/specs/physics.md)
- [Visual Spec](docs/specs/visual.md)
- [Technical Architecture](docs/specs/architecture.md)
- [Engine Decision](docs/specs/engine-decision.md)
- [Validation Plan](docs/specs/validation.md)

## Quality Bar

The simulator should feel physically plausible before it looks flashy, and it
should expose enough internal telemetry to debug why a ride feels wrong.
Visuals should support the motion experience with stable frame pacing, readable
 speed perception, coherent lighting, and realistic scale.
