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

// Target terrain height for the carved river bed/bank at a given perpendicular
// distance from the river centerline. The bed is deepest under the channel
// centre and rises to sit just under the waterline exactly at the visible water
// ribbon edge, then climbs out of the water as the bank. Pure helper so the
// shoreline contract can be unit tested without a populated river field.
COASTERSIM_API float RiverBedTargetHeightCm(
    float DistanceCm,
    float WaterSurfaceZCm,
    float VisibleHalfWidthCm);
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
