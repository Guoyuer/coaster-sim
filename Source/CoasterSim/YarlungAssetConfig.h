#pragma once

#include "CoreMinimal.h"

enum class EYarlungSceneryPlacement : uint8
{
    Scatter,
    CanopyBelt,
    CliffBelt,
    GroundCoverBelt
};

struct FYarlungScatterKindConfig
{
    float SideBias = 0.0f;
    float DistancePower = 1.0f;
    float MinLateralCm = 0.0f;
    float MaxLateralCm = 0.0f;
    float RiverClearanceCm = 0.0f;
    float MinHeightCm = 0.0f;
    float MaxHeightCm = 0.0f;
    float MinSlope = 0.0f;
    float MaxSlope = 1.0f;
    float ScaleMin = 1.0f;
    float ScaleMax = 1.0f;
    float HeightOffsetCm = 0.0f;
    bool bAlignToSurface = false;
};

struct FYarlungSceneryComponentConfig
{
    FString Name;
    FString MeshPath;
    FString Kind;
    EYarlungSceneryPlacement Placement = EYarlungSceneryPlacement::Scatter;
    int32 Count = 0;
    float Seed = 0.0f;
    bool bUseTint = false;
    FLinearColor Tint = FLinearColor::White;
};

struct FYarlungCanopyBeltConfig
{
    int32 SampleStride = 2;
    TArray<float> LateralBandsCm;
    int32 NearBandCount = 0;
    float NearOccupancy = 0.0f;
    float FarOccupancy = 0.0f;
    float TrackClearanceCm = 0.0f;
    float RiverClearanceCm = 0.0f;
    float MinHeightCm = 0.0f;
    float MaxHeightCm = 0.0f;
    float MaxSlope = 1.0f;
    float ScaleMin = 1.0f;
    float ScaleMax = 1.0f;
    float HeightOffsetCm = 0.0f;
};

struct FYarlungCliffBeltConfig
{
    int32 SampleStride = 2;
    TArray<float> LateralBandsCm;
    float Occupancy = 0.0f;
    float TrackClearanceCm = 0.0f;
    float RiverClearanceCm = 0.0f;
    float MinHeightCm = 0.0f;
    float MaxHeightCm = 0.0f;
    float MinSlope = 0.0f;
    float MaxSlope = 1.0f;
    float ScaleMin = 1.0f;
    float ScaleMax = 1.0f;
    float HeightOffsetCm = 0.0f;
    float AlongJitterCm = 0.0f;
    float LateralJitterCm = 0.0f;
    float YawJitterDegrees = 0.0f;
    int32 RiverWallSampleStride = 8;
    TArray<float> RiverWallLateralBandsCm;
    float RiverWallOccupancy = 0.0f;
    float RiverWallScaleMin = 1.0f;
    float RiverWallScaleMax = 1.0f;
    float RiverWallHeightOffsetCm = 0.0f;
    float RiverWallAlongJitterCm = 0.0f;
    float RiverWallLateralJitterCm = 0.0f;
    float RiverWallYawJitterDegrees = 0.0f;
};

struct FYarlungGroundCoverBeltConfig
{
    int32 SampleStride = 1;
    TArray<float> LateralBandsCm;
    float Occupancy = 0.0f;
    float TrackClearanceCm = 0.0f;
    float AlongJitterCm = 0.0f;
    float LateralJitterCm = 0.0f;
};

struct FYarlungWaterConfig
{
    FString SurfaceMaterialPath;
    float WidthScale = 1.0f;
    float MinWidthCm = 0.0f;
    float MaxWidthCm = 0.0f;
};

struct FYarlungAssetConfig
{
    TArray<FYarlungSceneryComponentConfig> SceneryComponents;
    TMap<FString, FYarlungScatterKindConfig> ScatterKinds;
    FYarlungCanopyBeltConfig CanopyBelt;
    FYarlungCliffBeltConfig CliffBelt;
    FYarlungGroundCoverBeltConfig GroundCoverBelt;
    FYarlungWaterConfig Water;
};

namespace YarlungAssets
{
const FYarlungAssetConfig& Config();
}
