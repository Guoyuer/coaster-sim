#pragma once

#include "CoreMinimal.h"
#include "YarlungTerrainProfile.h"

namespace YarlungTerrainRelief
{
struct FReliefConfig
{
    float EncodedMinZ = YarlungTerrain::EncodedMinZCm;
    float EncodedMaxZ = YarlungTerrain::EncodedMaxZCm;

    float SlopeGateStart = 0.0f;
    float SlopeGateRange = 0.10f;
    float RiverProtectInnerCm = 22000.0f;
    float RiverProtectFadeCm = 24000.0f;
    float HighAltitudeGateStart = 0.82f;
    float HighAltitudeGateRange = 0.14f;
    float HighAltitudeReduction = 0.30f;

    float WarpScale = 1.0f / 60000.0f;
    float WarpAmplitudeCm = 13000.0f;
    float NoiseScale = 1.0f / 24000.0f;
    int32 Octaves = 5;
    float Lacunarity = 2.0f;
    float Gain = 0.5f;
    float RidgedBias = 0.42f;
    float RidgedWeight = 1.9f;
    float FbmWeight = 0.40f;
    float DetailMin = -1.25f;
    float DetailMax = 1.25f;

    float MinAmplitudeCm = 2500.0f;
    float MaxAmplitudeCm = 7000.0f;
    float NearTrackProtectStartCm = 4000.0f;
    float NearTrackProtectFadeCm = 12000.0f;
    float NearTrackPositiveScale = 0.15f;
};

float ComputeReliefCm(
    const FVector2D& Position,
    float HeightCm,
    const FVector& BaseNormal,
    float TrackDistanceCm,
    const FReliefConfig& Config = FReliefConfig());
FVector ApplyNormalDisplacement(const FVector& BasePosition, const FVector& BaseNormal, float DisplacementCm);
}
