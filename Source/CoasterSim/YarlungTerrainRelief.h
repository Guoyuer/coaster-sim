#pragma once

#include "CoreMinimal.h"
#include "YarlungTerrainProfile.h"

namespace YarlungTerrainRelief
{
struct FReliefConfig
{
    float EncodedMinZ = YarlungTerrain::Config().EncodedMinZCm;
    float EncodedMaxZ = YarlungTerrain::Config().EncodedMaxZCm;

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

    float CliffFoldRiverDistanceStartCm = 36000.0f;
    float CliffFoldRiverDistanceFadeCm = 120000.0f;
    float CliffFoldWarpScale = 1.0f / 140000.0f;
    float CliffFoldWarpAmplitudeCm = 18000.0f;
    float CliffFoldAlongRiverScale = 1.0f / 180000.0f;
    float CliffFoldAcrossWallScale = 1.0f / 30000.0f;
    int32 CliffFoldOctaves = 4;
    float CliffFoldBias = 0.36f;
    float CliffFoldWeight = 1.0f;
    float CliffFoldFbmWeight = 0.20f;
    float CliffFoldDetailMin = -1.25f;
    float CliffFoldDetailMax = 0.35f;
    float CliffFoldMinAmplitudeCm = 450.0f;
    float CliffFoldMaxAmplitudeCm = 1200.0f;

    float MinAmplitudeCm = 450.0f;
    float MaxAmplitudeCm = 1400.0f;
    float NearTrackProtectStartCm = 4000.0f;
    float NearTrackProtectFadeCm = 12000.0f;
    float NearTrackPositiveScale = 0.15f;
};

float ComputeReliefCm(
    const FVector2D& Position,
    float HeightCm,
    const FVector& BaseNormal,
    float TrackDistanceCm,
    float ViewCorridorMask = 1.0f,
    const FReliefConfig& Config = FReliefConfig());
FVector ApplyVerticalDisplacement(const FVector& BasePosition, float DisplacementCm);
}
