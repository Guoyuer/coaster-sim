# Technical Architecture

## Stack

- TypeScript.
- Vite.
- Three.js.
- Vitest for deterministic physics and geometry tests.
- Playwright for browser smoke tests and screenshot regression later.

## Main Modules

```text
src/
  app/
    bootstrap.ts
    input.ts
    hud.ts
  sim/
    fixedStep.ts
    track.ts
    train.ts
    sections.ts
    telemetry.ts
    validation.ts
  geometry/
    frames.ts
    arcLength.ts
    trackMesh.ts
    supportMesh.ts
    trainMesh.ts
  render/
    scene.ts
    cameras.ts
    materials.ts
    postprocess.ts
  audio/
    rideAudio.ts
  data/
    showcaseTrack.ts
```

## Data Model

Track definitions should be plain structured data:

```ts
type TrackControlPoint = {
  id: string;
  position: [number, number, number];
  bankDeg: number;
  tension?: number;
};

type TrackSection = {
  id: string;
  type: "station" | "lift" | "launch" | "coast" | "trim" | "brake" | "block";
  startS: number;
  endS: number;
  targetSpeedMps?: number;
  accelerationMps2?: number;
  decelerationMps2?: number;
};
```

Arc length, frames, curvature, and mesh buffers are derived data and should be
rebuildable from the authored definition.

## Simulation Loop

- Render loop uses `requestAnimationFrame`.
- Physics advances with a fixed timestep accumulator.
- Simulation state is separate from Three.js object state.
- Rendering interpolates from simulation snapshots where useful.

## Determinism

The same track definition and seed should produce the same telemetry samples.
Avoid tying physics to frame delta. Store golden telemetry snapshots for
validation tracks.

## Performance Budget

M1 target:

- 60 FPS desktop in a 90-180 second showcase scene.
- Track mesh generated once at load unless editing.
- Instanced geometry for ties, bolts, supports, and trees.
- Keep draw calls low enough for future WebXR exploration.

## Editor Readiness

Even before the editor exists:

- Track data should be serializable.
- Derived geometry should be rebuildable.
- Validation should be callable without rendering.
- UI state should not own physics state.

## Testing Strategy

- Unit tests for math, frames, arc-length mapping, section transitions, and
  energy behavior.
- Snapshot tests for telemetry on validation tracks.
- Browser smoke test for loading the scene and advancing simulation.
- Visual regression after the renderer stabilizes.
