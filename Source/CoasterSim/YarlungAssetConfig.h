#pragma once

#include "CoreMinimal.h"

enum class EYarlungSceneryPlacement : uint8
{
    Scatter,
    SurfaceCover,
    RockWallSource
};

enum class EYarlungSurfaceCoverAnchor : uint8
{
    Track,
    River
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
    FString SurfaceCoverProfileName;
    EYarlungSceneryPlacement Placement = EYarlungSceneryPlacement::Scatter;
    int32 Count = 0;
    float Seed = 0.0f;
    bool bUseTint = false;
    FLinearColor Tint = FLinearColor::White;
};

struct FYarlungSurfaceCoverProfileConfig
{
    FString Name;
    EYarlungSurfaceCoverAnchor Anchor = EYarlungSurfaceCoverAnchor::Track;
    int32 SampleStride = 1;
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
    bool bAlignToSurface = false;
    bool bFaceRiver = false;
};

struct FYarlungRockWallProfileConfig
{
    FString Name;
    float LateralMinCm = 0.0f;
    float LateralMaxCm = 0.0f;
    float LateralStepCm = 0.0f;
    float TrackClearanceCm = 0.0f;
    float RiverClearanceCm = 0.0f;
    float MinHeightCm = 0.0f;
    float MaxHeightCm = 0.0f;
    float MinSlope = 0.0f;
    float MaxSlope = 1.0f;
    float ScaleMin = 1.0f;
    float ScaleMax = 1.0f;
    float HeightOffsetCm = 0.0f;
    float EmbedDepthCm = 0.0f;
    float AlongJitterCm = 0.0f;
    float LateralJitterCm = 0.0f;
    float YawJitterDegrees = 0.0f;
    bool bAlignToSurface = false;
};

struct FYarlungRockWallSegmentConfig
{
    FString Name;
    FString ProfileName;
    FString ComponentName;
    int32 StartSampleIndex = 0;
    int32 EndSampleIndex = 0;
    int32 SampleStride = 1;
    float Side = 1.0f;
    float Seed = 0.0f;
};

struct FYarlungWaterConfig
{
    FString SurfaceMaterialPath;
};

struct FYarlungAssetConfig
{
    TArray<FYarlungSceneryComponentConfig> SceneryComponents;
    TMap<FString, FYarlungScatterKindConfig> ScatterKinds;
    TMap<FString, FYarlungSurfaceCoverProfileConfig> SurfaceCoverProfiles;
    TMap<FString, FYarlungRockWallProfileConfig> RockWallProfiles;
    TArray<FYarlungRockWallSegmentConfig> RockWallSegments;
    FYarlungWaterConfig Water;
};

namespace YarlungAssets
{
const FYarlungAssetConfig& Config();
}
