#pragma once

#include "CoreMinimal.h"

namespace YarlungViewCorridor
{
struct FTrackPoint
{
    FVector2D Position = FVector2D::ZeroVector;
};

struct FViewCorridorConfig
{
    float BackwardFadeCm = 12000.0f;
    float FarStartCm = 160000.0f;
    float FarFadeCm = 120000.0f;
    float SideBaseCm = 14000.0f;
    float SideFadeCm = 26000.0f;
    float MaxSideCm = 180000.0f;
    float TanHalfFov = 0.84f;
};

COASTERSIM_API float DistanceToTrackCm(const TArray<FTrackPoint>& TrackPoints, const FVector2D& Position);
COASTERSIM_API float ComputeMask(
    const TArray<FTrackPoint>& TrackPoints,
    const FVector2D& Position,
    const FViewCorridorConfig& Config = FViewCorridorConfig());
}
