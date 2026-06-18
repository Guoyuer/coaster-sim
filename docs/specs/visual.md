# Visual Spec

## Objective

Create visuals that communicate realistic scale, speed, height, and material
weight with a high-end native desktop renderer. Visual quality should serve the
ride sensation first.

## Rendering Stack

- Unreal Engine 5.
- Lumen for dynamic global illumination where performance allows.
- Nanite for dense static environment geometry where it fits the asset type.
- Virtual Shadow Maps or equivalent high-quality shadowing for track, train,
  supports, and terrain.
- PBR materials for train, rails, supports, terrain objects, and station assets.
- HDR environment lighting where practical.
- Directional sun, ambient sky light, contact shadows, and optional baked or
  cached lighting for static scenery.
- Post-processing: tone mapping, subtle bloom for lights, volumetric fog, color
  grading, depth of field only for non-ride cameras, and motion cues used
  conservatively.

## Visual Quality Targets

- Stable 60 FPS on a modern desktop GPU for M1 showcase scene.
- Headroom for 90 FPS VR exploration after desktop mode is stable.
- Track and train scale should read correctly from first-person and external
  cameras.
- No visible rail discontinuities, tie popping, or support flicker during the
  ride.
- Camera should not clip through train geometry during normal operation.
- High-speed motion should stay readable without excessive blur.

## Track Rendering

Track mesh is generated procedurally from sampled path frames:

- Dual rails with circular or rectangular profile.
- Cross ties/sleepers placed by arc-length interval.
- Spine or catwalk options by coaster type later.
- Rail joints, bolt plates, and service details added as secondary mesh layers
  after the base mesh is stable.

M1 track should support:

- Continuous banking.
- Smooth normals.
- Color/material overrides per section.
- Debug overlays for samples, tangents, frames, and curvature.

## Support Rendering

M1 supports are procedural and conservative:

- Vertical columns for low complexity sections.
- A-frame or triangular braces for elevated turns and hills.
- Footers placed on terrain intersections.
- Support density driven by height, curvature, and section type.

Later versions should add support families, collision-aware placement, and
manual support editing.

## Train Rendering

M1 train requirements:

- Several connected cars.
- Seat rows, restraints, wheels, and bogies represented at readable silhouette
  level.
- First-person front-row and arbitrary-seat cameras.
- Wheel rotation and subtle chassis vibration.
- Separate material zones for painted body, metal, rubber, and glass/plastic.

M1 does not require brand-specific train replicas.

## Environment Rendering

M1 includes:

- Sculpted terrain mesh with grass/rock/asphalt materials.
- Sky dome or procedural sky.
- Light fog for depth.
- Station platform blockout.
- Trees or vertical scenery markers to make speed and height legible.

M2+ adds:

- Terrain editing.
- Water, tunnels, themed structures, queue/station details.
- Weather and time-of-day presets.
- Higher-detail authored environment set pieces once the procedural track and
  ride model are stable.

## Camera Requirements

### Onboard Camera

- Mounted relative to selected seat.
- Follows train-local orientation with smoothing.
- Allows slight look-around offset later.
- Avoids raw spline torsion artifacts.

### Free Camera

- Orbit and fly controls.
- Pausable simulation.
- Telemetry inspector.

### Trackside Camera

- Fixed camera positions with auto-targeting.
- Useful for validating scale and train movement.

## Audio Requirements

Audio is part of visual realism because it sells speed and mechanical force:

- Wind volume and pitch scale with speed.
- Rail/wheel noise scales with speed and curvature.
- Lift chain loop and anti-rollback clicks.
- Launch hum.
- Brake squeal/air release.
- Tunnel reverb and station ambience later.

## Asset Policy

- Use original procedural geometry by default.
- Any third-party assets must be license-compatible and documented.
- Do not import commercial simulator assets, textures, train models, track
  files, or UI designs.
