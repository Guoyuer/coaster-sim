# Product Spec

## Goal

Build an original high-quality native desktop 3D roller coaster simulator that
prioritizes realistic first-person ride feel, transparent physical telemetry,
and visually convincing coaster environments.

The first milestone is a polished playable ride, not a full park-management
game. Track editing arrives after the ride model and rendering pipeline are
credible.

## Non-Goals

- Do not copy any commercial simulator UI, branding, content, track libraries,
  coaster models, or proprietary workflows.
- Do not target engineering certification or real-world safety approval.
- Do not build park-management economics in the first phase.
- Do not build a browser/Web version in the first phase.
- Do not prioritize mobile or low-end hardware until desktop quality is
  established.

## Target User

- Coaster enthusiasts who want believable first-person rides.
- Creators who want to design and inspect virtual coaster layouts.
- Developers who want an inspectable physics/rendering sandbox.

## Experience Pillars

1. Realistic motion: speed changes, banking, airtime, lateral force, lift,
   launch, and braking should be believable and measurable.
2. Stable immersion: first-person camera orientation should remain smooth,
   physically coherent, and comfortable at high speed.
3. Visual scale: track, train, terrain, supports, shadows, and atmosphere should
   make height and velocity legible.
4. Inspectability: HUD and debug tools should reveal the simulation state
   instead of hiding it.
5. Iterative creation: the system should support a future editor without
   rewriting the simulation core.

## Milestones

### M1: Playable High-Fidelity Ride

- One hand-authored showcase track with lift hill, drop, turns, airtime hills,
  inversion-capable orientation, trims, and final brakes.
- First-person onboard camera.
- Free camera and trackside fly-by camera.
- Procedural track mesh, ties, and basic supports.
- Terrain, sky, fog, lighting, post-processing, and ride audio placeholders.
- HUD with speed, height, slope, curvature, estimated vertical/lateral G, and
  section state.
- Deterministic replay from a fixed track definition.

### M2: Track System and Section Semantics

- Data-driven track format.
- Explicit section types: station, lift, launch, coast, trim, brake, block.
- Multiple trains with dispatch timing and block section occupancy.
- Export/import of track files.
- Debug visualization for tangent, normal, binormal, banking, curvature, and
  acceleration vectors.

### M3: Editor MVP

- Add, move, and delete control points.
- Edit banking, section type, target speed, launch acceleration, brake
  deceleration, and lift speed.
- Live validation for curvature spikes, excessive G-force, clearance, and
  unsupported spans.
- Save/load local projects.

### M4: Visual and Audio Polish

- More detailed train models.
- Procedural support families.
- Environment presets.
- Spatial audio for wheels, wind, lift chain, launch, brakes, tunnels, and
  station ambience.
- VR investigation after desktop frame pacing and ride comfort are stable.

## Success Criteria

- A rider can complete a 90-180 second coaster run at stable desktop frame
  pacing.
- Speed changes match energy expectations within documented tolerances.
- G-force telemetry is plausible and useful for debugging.
- First-person camera remains smooth through hills, turns, and banking.
- Visual scale makes drops, height, and velocity clear without relying on UI.
