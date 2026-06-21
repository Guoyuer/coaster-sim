#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UCoasterTrackComponent;
class UInstancedStaticMeshComponent;

namespace CoasterTrackVisuals
{
using FSampleFrameFn = TFunctionRef<void(float, FVector&, FRotator&, FVector&, FVector&, FVector&)>;

void ConfigureMeshes(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports);

void ApplyMaterials(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports);

void SetVisible(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports,
    bool bVisible);

void Rebuild(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports,
    const UCoasterTrackComponent* TrackSpline,
    float TrackLengthCm,
    float RailGaugeCm,
    FSampleFrameFn SampleFrame);
}
