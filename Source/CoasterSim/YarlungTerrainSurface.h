#pragma once

#include "CoreMinimal.h"
#include "YarlungViewCorridor.h"

class FYarlungRiverField;

namespace YarlungTerrainSurface
{
struct FSurfaceStats
{
    int32 DisplacedVertexCount = 0;
    float MaxAbsDisplacementCm = 0.0f;
    int32 RiverCarvedVertexCount = 0;
    float MaxRiverCarveCm = 0.0f;
};

COASTERSIM_API float SourceHeightCm(const TArray<uint16>& EncodedHeights, float X, float Y);
COASTERSIM_API FVector SourceNormal(const TArray<uint16>& EncodedHeights, float X, float Y);
COASTERSIM_API bool FindNearestTrackProfileFrame(
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FVector2D& Position,
    FVector2D& OutCenter,
    float& OutSignedOffsetCm);
COASTERSIM_API float SurfaceZCm(
    const TArray<uint16>& EncodedHeights,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    const FVector2D& Position,
    FSurfaceStats* OutStats = nullptr);
COASTERSIM_API FVector SurfaceNormal(
    const TArray<uint16>& EncodedHeights,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    const FVector2D& Position);
}
