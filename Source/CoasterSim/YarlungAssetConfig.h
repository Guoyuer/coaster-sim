#pragma once

#include "CoreMinimal.h"

enum class EYarlungSceneryPlacement : uint8
{
    Scatter,
    CanopyBelt,
    CliffBelt
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
};

struct FYarlungWaterConfig
{
    FString RiverMaterialPath;
    FString SurfaceMaterialPath;
    int32 ZoneRenderTargetResolution = 1024;
    float ZoneExtentScale = 0.55f;
    float DefaultDepthCm = 0.0f;
    float BaseVelocityCmPerSec = 0.0f;
    float FlowVelocityJitterCmPerSec = 0.0f;
    float WidthScale = 1.0f;
    float MinWidthCm = 0.0f;
    float MaxWidthCm = 0.0f;
    float ShapeDilation = 0.0f;
    float AudioIntensity = 0.0f;
};

struct FYarlungAssetConfig
{
    TArray<FYarlungSceneryComponentConfig> SceneryComponents;
    TMap<FString, FYarlungScatterKindConfig> ScatterKinds;
    FYarlungCanopyBeltConfig CanopyBelt;
    FYarlungCliffBeltConfig CliffBelt;
    FYarlungWaterConfig Water;
};

namespace YarlungAssets
{
const FYarlungAssetConfig& Config();
}
