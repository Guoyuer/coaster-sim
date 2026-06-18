# Technical Architecture

## Stack

- Unreal Engine 5.
- C++ for simulation, geometry generation, telemetry, and validation.
- Blueprints only for thin presentation, debug tooling, and designer-facing
  configuration where useful.
- Unreal Automation Tests for deterministic physics and geometry tests.
- Functional tests and scripted camera captures for scene smoke tests and
  screenshot regression.

## Main Modules

```text
Source/CoasterSim/
  CoasterSim.Build.cs
  Public/
    Sim/
      FixedStepSim.h
      TrackDefinition.h
      TrackSampler.h
      TrainState.h
      SectionModel.h
      Telemetry.h
      Validation.h
    Geometry/
      TrackFrame.h
      ArcLengthTable.h
      TrackMeshBuilder.h
      SupportMeshBuilder.h
      TrainPlacement.h
    Runtime/
      CoasterRideActor.h
      CoasterTrainActor.h
      RideCameraComponent.h
      CoasterHudWidget.h
  Private/
    Sim/
    Geometry/
    Runtime/
Content/
  Materials/
  Maps/
  Audio/
  UI/
Tests/
  Automation/
  GoldenTelemetry/
```

## Data Model

Track definitions should be plain structured data:

```cpp
struct FTrackControlPoint
{
  FGuid Id;
  FVector PositionMeters;
  double BankDegrees = 0.0;
  double Tension = 0.5;
};

enum class ETrackSectionType
{
  Station,
  Lift,
  Launch,
  Coast,
  Trim,
  Brake,
  Block
};

struct FTrackSection
{
  FGuid Id;
  ETrackSectionType Type = ETrackSectionType::Coast;
  double StartMeters = 0.0;
  double EndMeters = 0.0;
  TOptional<double> TargetSpeedMps;
  TOptional<double> AccelerationMps2;
  TOptional<double> DecelerationMps2;
};
```

Arc length, frames, curvature, procedural meshes, and authored debug overlays
are derived data and should be rebuildable from the authored definition.

## Simulation Loop

- Unreal's game tick drives an accumulator, but ride physics advances with an
  explicit fixed timestep.
- Simulation state is separate from Actor and Component transform state.
- Runtime Actors consume immutable simulation snapshots for rendering,
  animation, camera placement, HUD, and audio.
- Chaos Physics is not the primary coaster solver. It is used for supporting
  collision, scene interactions, and future secondary effects.

## Determinism

The same track definition and seed should produce the same telemetry samples.
Avoid tying physics to frame delta. Store golden telemetry snapshots for
validation tracks.

## Performance Budget

M1 target:

- 60 FPS desktop in a 90-180 second showcase scene at high visual settings.
- Track mesh generated once at load unless editing.
- Instanced geometry for ties, bolts, supports, and trees.
- Keep frame pacing stable enough that future VR investigation is plausible.

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
- Unreal functional smoke test for loading the showcase map and advancing
  simulation.
- Visual regression after the renderer stabilizes.
