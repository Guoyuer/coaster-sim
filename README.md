# Coaster Sim

An original high-fidelity 3D roller coaster simulator targeting realistic ride
feel, inspectable physics, and modern browser rendering with Three.js.

This project is not a clone of any existing commercial simulator. It aims to
build an original coaster simulation stack with explicit physical assumptions,
measurable visual targets, and iterative validation.

## Initial Scope

- Browser-based 3D simulation using Three.js.
- First-person onboard ride camera plus free and trackside cameras.
- Spline-based track geometry with banking, lift, launch, brake, and block
  sections.
- Physics model that exposes speed, acceleration, curvature, slope, and
  estimated rider G-forces.
- Procedural rails, ties, supports, terrain, sky, atmosphere, lighting, and
  ride audio.
- Later editor support for creating, validating, saving, and loading tracks.

## Specs

- [Product Spec](docs/specs/product.md)
- [Physics Spec](docs/specs/physics.md)
- [Visual Spec](docs/specs/visual.md)
- [Technical Architecture](docs/specs/architecture.md)
- [Validation Plan](docs/specs/validation.md)

## Quality Bar

The simulator should feel physically plausible before it looks flashy, and it
should expose enough internal telemetry to debug why a ride feels wrong.
Visuals should support the motion experience with stable frame pacing, readable
 speed perception, coherent lighting, and realistic scale.
