#include "YarlungLandscapeImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungRiverField.h"
#include "YarlungSceneryActor.h"
#include "YarlungMeshTerrainActor.h"
#include "YarlungCorridorProfile.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainRelief.h"
#include "YarlungTrackCsv.h"
#include "YarlungViewCorridor.h"
#include "YarlungWaterBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "Misc/FileHelper.h"
#include "StaticMeshAttributes.h"
#include "UObject/Linker.h"

#endif

#if WITH_EDITOR
namespace
{
// Heightmap dimensions + world bounds come from YarlungTerrain::Config()
// (Config/yarlung-terrain.json), shared with the Python pipeline and scenery.
const TCHAR* YarlungCorridorTerrainMeshPackagePath = TEXT("/Game/Generated/YarlungLandscape/SM_YarlungCorridorTerrain");
const TCHAR* YarlungCorridorTerrainMeshAssetName = TEXT("SM_YarlungCorridorTerrain");
const TCHAR* YarlungCorridorTerrainMeshObjectPath = TEXT("/Game/Generated/YarlungLandscape/SM_YarlungCorridorTerrain.SM_YarlungCorridorTerrain");

float YarlungCubicBsplineInterp(float P0, float P1, float P2, float P3, float T)
{
    const float OneMinusT = 1.0f - T;
    const float W0 = OneMinusT * OneMinusT * OneMinusT / 6.0f;
    const float W1 = (3.0f * T * T * T - 6.0f * T * T + 4.0f) / 6.0f;
    const float W2 = (-3.0f * T * T * T + 3.0f * T * T + 3.0f * T + 1.0f) / 6.0f;
    const float W3 = T * T * T / 6.0f;
    return P0 * W0 + P1 * W1 + P2 * W2 + P3 * W3;
}

float YarlungValueNoise(float X, float Y)
{
    return FMath::Frac(FMath::Sin(X * 0.00173f + Y * 0.00291f) * 43758.5453f);
}

float YarlungSignedValueNoise(float X, float Y)
{
    return YarlungValueNoise(X, Y) * 2.0f - 1.0f;
}

float YarlungHeightAtGrid(const TArray<uint16>& HeightData, int32 XIndex, int32 YIndex)
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    const int32 ClampedX = FMath::Clamp(XIndex, 0, Size - 1);
    const int32 ClampedY = FMath::Clamp(YIndex, 0, Size - 1);
    return YarlungTerrain::HeightValueToCm(HeightData[ClampedY * Size + ClampedX]);
}

float YarlungHeightAtSourceGrid(const TArray<uint16>& HeightData, float SourceX, float SourceY)
{
    const int32 BaseX = FMath::FloorToInt(SourceX);
    const int32 BaseY = FMath::FloorToInt(SourceY);
    const float TX = SourceX - static_cast<float>(BaseX);
    const float TY = SourceY - static_cast<float>(BaseY);
    float Rows[4] = {};
    float MinNeighbor = TNumericLimits<float>::Max();
    float MaxNeighbor = -TNumericLimits<float>::Max();

    for (int32 Row = 0; Row < 4; ++Row)
    {
        float Samples[4] = {};
        for (int32 Column = 0; Column < 4; ++Column)
        {
            Samples[Column] = YarlungHeightAtGrid(HeightData, BaseX + Column - 1, BaseY + Row - 1);
            MinNeighbor = FMath::Min(MinNeighbor, Samples[Column]);
            MaxNeighbor = FMath::Max(MaxNeighbor, Samples[Column]);
        }
        Rows[Row] = YarlungCubicBsplineInterp(Samples[0], Samples[1], Samples[2], Samples[3], TX);
    }

    return FMath::Clamp(
        YarlungCubicBsplineInterp(Rows[0], Rows[1], Rows[2], Rows[3], TY),
        MinNeighbor,
        MaxNeighbor);
}

float YarlungHeightAtWorldXY(const TArray<uint16>& HeightData, float X, float Y)
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const float U = FMath::Clamp((X - Tc.MinXCm) / (Tc.MaxXCm - Tc.MinXCm), 0.0f, 1.0f);
    const float V = FMath::Clamp((Y - Tc.MinYCm) / (Tc.MaxYCm - Tc.MinYCm), 0.0f, 1.0f);
    return YarlungHeightAtSourceGrid(
        HeightData,
        U * static_cast<float>(Tc.GridSize - 1),
        V * static_cast<float>(Tc.GridSize - 1));
}

FVector YarlungSmoothNormalAtWorldXY(const TArray<uint16>& HeightData, float X, float Y)
{
    constexpr float SampleSpacingCm = 900.0f;
    const float Left = YarlungHeightAtWorldXY(HeightData, X - SampleSpacingCm, Y);
    const float Right = YarlungHeightAtWorldXY(HeightData, X + SampleSpacingCm, Y);
    const float Down = YarlungHeightAtWorldXY(HeightData, X, Y - SampleSpacingCm);
    const float Up = YarlungHeightAtWorldXY(HeightData, X, Y + SampleSpacingCm);
    return FVector(Left - Right, Down - Up, SampleSpacingCm * 4.0f).GetSafeNormal();
}

float YarlungCanyonWetRockMask(float X, float Y, float Height, const FVector& BaseNormal, const FYarlungRiverField& RiverField)
{
    const float SlopeMask = YarlungTerrain::Smooth01(((1.0f - BaseNormal.Z) - 0.13f) / 0.38f);
    const float HeightMask = 1.0f - 0.55f * YarlungTerrain::Smooth01((Height - 515000.0f) / 120000.0f);
    const float RiverDistance = RiverField.DistanceCm(FVector2D(X, Y));
    const float DistanceMask = YarlungTerrain::Smooth01((RiverDistance - 28000.0f) / 95000.0f)
        * (1.0f - YarlungTerrain::Smooth01((RiverDistance - 320000.0f) / 120000.0f));
    const float Strata = 0.5f + 0.5f * FMath::Sin(X * 0.0019f - Y * 0.0023f + Height * 0.0041f);
    return FMath::Clamp(DistanceMask * SlopeMask * HeightMask * (0.72f + Strata * 0.42f), 0.0f, 1.0f);
}

float YarlungRavineMask(float X, float Y, const FYarlungRiverField& RiverField)
{
    const float RiverDistance = RiverField.DistanceCm(FVector2D(X, Y));
    const float Along = X / 210000.0f + Y / 567000.0f;
    const float Across = RiverDistance / 86000.0f;
    const float Signal = 0.5f + 0.5f * FMath::Sin(Along * 11.2f + Across * 2.65f - 0.9f);
    return FMath::Pow(FMath::Clamp(Signal, 0.0f, 1.0f), 5.0f);
}

FLinearColor YarlungColorAtPosition(float X, float Y, float Height, const FVector& Normal, float RockMask, const FYarlungRiverField& RiverField)
{
    const float Height01 = YarlungTerrain::NormalizeEncodedHeightCm(Height);
    const float RiverDistance = RiverField.DistanceCm(FVector2D(X, Y));
    const float Slope = 1.0f - Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector).Z;
    const float MidSlope = YarlungTerrain::Smooth01((Slope - 0.08f) / 0.24f);
    const float SteepSlope = YarlungTerrain::Smooth01((Slope - 0.22f) / 0.30f);
    const float ForestElevation = 1.0f - 0.62f * YarlungTerrain::Smooth01((Height01 - 0.76f) / 0.18f);
    const float ForestDistance = YarlungTerrain::Smooth01((RiverDistance - 9000.0f) / 76000.0f)
        * (1.0f - YarlungTerrain::Smooth01((RiverDistance - 430000.0f) / 190000.0f));
    const float BroadCanopyPatch = 0.5f + 0.5f * FMath::Sin(X * 0.00088f - Y * 0.00116f + Height * 0.0011f);
    const float FineCanopyPatch = YarlungValueNoise(X * 1.7f, Y * 1.7f);
    const float CanopyMask = YarlungTerrain::Smooth01((BroadCanopyPatch * 0.72f + FineCanopyPatch * 0.28f - 0.22f) / 0.48f);
    const float Forest = FMath::Clamp(
        ForestDistance * ForestElevation * FMath::Lerp(0.82f, 1.16f, MidSlope) * FMath::Lerp(0.72f, 1.0f, CanopyMask),
        0.0f,
        1.0f);
    const float Ravine = MidSlope * YarlungRavineMask(X, Y, RiverField);
    const float RavineStreak = FMath::Pow(
        FMath::Clamp(0.5f + 0.5f * FMath::Sin(X * 0.0025f - Y * 0.0014f + Height * 0.0038f), 0.0f, 1.0f),
        4.0f);
    const float Noise = YarlungValueNoise(X, Y);

    const FLinearColor DeepForest(0.010f + Noise * 0.010f, 0.052f + Noise * 0.036f, 0.022f + Noise * 0.020f, 1.0f);
    const FLinearColor SunForest(0.026f + Noise * 0.020f, 0.122f + Noise * 0.068f, 0.042f + Noise * 0.036f, 1.0f);
    const FLinearColor WeatheredRock(0.064f + Height01 * 0.032f, 0.072f + Height01 * 0.034f, 0.070f + Height01 * 0.030f, 1.0f);
    const FLinearColor WetRock(0.034f + Noise * 0.028f, 0.046f + Noise * 0.032f, 0.042f + Noise * 0.030f, 1.0f);
    const FLinearColor Scree(0.118f + Noise * 0.038f, 0.116f + Noise * 0.036f, 0.102f + Noise * 0.032f, 1.0f);
    const FLinearColor RavineColor(0.010f, 0.022f, 0.020f, 1.0f);
    const FLinearColor Snow(0.66f, 0.70f, 0.69f, 1.0f);

    FLinearColor Base = FMath::Lerp(DeepForest, SunForest, CanopyMask * (1.0f - SteepSlope * 0.45f));
    Base = FMath::Lerp(WeatheredRock, Base, Forest);
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(RockMask * 0.82f + SteepSlope * RavineStreak * 0.22f, 0.0f, 0.90f));
    Base = FMath::Lerp(Base, Scree, FMath::Clamp(SteepSlope * (1.0f - Forest) * (0.12f + RavineStreak * 0.16f), 0.0f, 0.26f));
    Base = FMath::Lerp(Base, RavineColor, FMath::Clamp(Ravine * 0.58f, 0.0f, 0.68f));
    Base = FMath::Lerp(Base, Snow, 0.18f * YarlungTerrain::Smooth01((Height01 - 0.992f) / 0.016f));
    return Base;
}

bool LoadYarlungTerrainTrackPoints(TArray<YarlungViewCorridor::FTrackPoint>& OutTrackPoints)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("Generated/YarlungLandscape/YarlungTrack.csv");
    TArray<FYarlungTrackRow> Rows;
    FString Error;
    if (!YarlungTrackCsv::Load(Path, Rows, &Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read generated Yarlung track for terrain displacement: %s"), *Error);
        return false;
    }

    OutTrackPoints.Reset();
    OutTrackPoints.Reserve(Rows.Num());
    for (const FYarlungTrackRow& Row : Rows)
    {
        YarlungViewCorridor::FTrackPoint Point;
        Point.Position = FVector2D(Row.PositionCm.X, Row.PositionCm.Y);
        OutTrackPoints.Add(Point);
    }

    if (OutTrackPoints.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few generated track points for terrain displacement: %d"), OutTrackPoints.Num());
        return false;
    }

    return true;
}

bool FindNearestTrackProfileFrame(
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FVector2D& Position,
    FVector2D& OutCenter,
    float& OutSignedOffsetCm)
{
    if (TrackPoints.Num() < 2)
    {
        return false;
    }

    float BestSquared = TNumericLimits<float>::Max();
    FVector2D BestCenter = TrackPoints[0].Position;
    FVector2D BestTangent = FVector2D(1.0f, 0.0f);

    for (int32 Index = 0; Index < TrackPoints.Num(); ++Index)
    {
        const FVector2D A = TrackPoints[Index].Position;
        const FVector2D B = TrackPoints[(Index + 1) % TrackPoints.Num()].Position;
        const FVector2D AB = B - A;
        const float LengthSquared = AB.SizeSquared();
        if (LengthSquared <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float T = FMath::Clamp(FVector2D::DotProduct(Position - A, AB) / LengthSquared, 0.0f, 1.0f);
        const FVector2D Closest = A + AB * T;
        const float Squared = FVector2D::DistSquared(Position, Closest);
        if (Squared < BestSquared)
        {
            BestSquared = Squared;
            BestCenter = Closest;
            BestTangent = AB.GetSafeNormal();
        }
    }

    const FVector2D Relative = Position - BestCenter;
    OutCenter = BestCenter;
    OutSignedOffsetCm = BestTangent.X * Relative.Y - BestTangent.Y * Relative.X;
    return BestSquared < TNumericLimits<float>::Max();
}

// Continuous canyon base mesh. The track-view corridor is still the visual
// priority, but the foundation must be a non-self-intersecting world surface.
// A track-swept strip folded over itself on loops/turns and clipped the camera.
constexpr int32 BaseTerrainStride = 1;
struct FBaseTerrainMeshStats
{
    int32 DisplacedVertexCount = 0;
    float MaxAbsDisplacementCm = 0.0f;
    int32 RiverCarvedVertexCount = 0;
    float MaxRiverCarveCm = 0.0f;
};

int32 BaseTerrainSampleCount()
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    return (Size - 1) / BaseTerrainStride + 1;
}

float BaseTerrainWorldX(int32 XIndex)
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const float U = static_cast<float>(XIndex * BaseTerrainStride) / static_cast<float>(Tc.GridSize - 1);
    return FMath::Lerp(Tc.MinXCm, Tc.MaxXCm, U);
}

float BaseTerrainWorldY(int32 YIndex)
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const float V = static_cast<float>(YIndex * BaseTerrainStride) / static_cast<float>(Tc.GridSize - 1);
    return FMath::Lerp(Tc.MinYCm, Tc.MaxYCm, V);
}

int32 BaseTerrainVertexIndex(int32 XIndex, int32 YIndex, int32 SampleCount)
{
    return YIndex * SampleCount + XIndex;
}

float CarveRiverChannelCm(
    float HeightCm,
    const FVector2D& Position,
    const FYarlungRiverField& RiverField,
    FBaseTerrainMeshStats& Stats)
{
    const FYarlungRiverQuery River = RiverField.QueryNearest(Position);
    if (!River.bIsValid)
    {
        return HeightCm;
    }

    const float InnerBankCm = River.WaterHalfWidthCm + 3600.0f;
    const float OuterBankCm = River.WaterHalfWidthCm + 36000.0f;
    if (River.DistanceCm >= OuterBankCm)
    {
        return HeightCm;
    }

    const float BankT = YarlungTerrain::Smooth01((River.DistanceCm - InnerBankCm) / (OuterBankCm - InnerBankCm));
    const float CarveAlpha = 1.0f - BankT;
    const float BankRiseCm = FMath::Max(0.0f, River.DistanceCm - River.WaterHalfWidthCm) * 0.36f;
    const float TargetHeightCm = River.WaterSurfaceZCm - 520.0f + BankRiseCm;
    const float CarvedHeightCm = FMath::Min(HeightCm, FMath::Lerp(HeightCm, TargetHeightCm, CarveAlpha));
    const float CarveCm = HeightCm - CarvedHeightCm;
    if (CarveCm > 1.0f)
    {
        ++Stats.RiverCarvedVertexCount;
        Stats.MaxRiverCarveCm = FMath::Max(Stats.MaxRiverCarveCm, CarveCm);
    }
    return CarvedHeightCm;
}

// Phase 1: continuous DEM-led surface positions and UVs.
void ComputeBaseTerrainPositions(
    const TArray<uint16>& HeightData,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    TArray<FVector>& Positions,
    TArray<float>& Us,
    TArray<float>& Vs,
    FBaseTerrainMeshStats& Stats)
{
    const int32 SampleCount = BaseTerrainSampleCount();

    for (int32 YIndex = 0; YIndex < SampleCount; ++YIndex)
    {
        const float Y = BaseTerrainWorldY(YIndex);
        for (int32 XIndex = 0; XIndex < SampleCount; ++XIndex)
        {
            const float X = BaseTerrainWorldX(XIndex);
            const FVector2D Position2D(X, Y);
            const float BaseHeight = YarlungHeightAtWorldXY(HeightData, Position2D.X, Position2D.Y);
            const FVector BaseNormal = YarlungSmoothNormalAtWorldXY(HeightData, Position2D.X, Position2D.Y);
            const float TrackDistance = YarlungViewCorridor::DistanceToTrackCm(TrackPoints, Position2D);
            const float ViewCorridorMask = YarlungViewCorridor::ComputeMask(TrackPoints, Position2D);
            FVector2D ProfileCenter = Position2D;
            float SignedOffsetCm = 0.0f;
            const bool bHasProfileFrame = FindNearestTrackProfileFrame(TrackPoints, Position2D, ProfileCenter, SignedOffsetCm);
            const float TrackBaseHeight = bHasProfileFrame
                ? YarlungHeightAtWorldXY(HeightData, ProfileCenter.X, ProfileCenter.Y)
                : BaseHeight;
            const float ProfileHeight = bHasProfileFrame
                ? YarlungCorridorProfile::AuthoredHeightCm(ProfileCenter, SignedOffsetCm, TrackBaseHeight, BaseHeight)
                : BaseHeight;
            const float ProfileBlend = FMath::Clamp(ViewCorridorMask, 0.0f, 1.0f)
                * YarlungTerrain::Smooth01((TrackDistance - 30000.0f) / 52000.0f);
            const float AuthoredProfileHeight = BaseHeight + (ProfileHeight - BaseHeight) * 0.48f;
            const float RiverDistance = RiverField.DistanceCm(Position2D);
            const float DisplacementCm = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
                Position2D,
                AuthoredProfileHeight,
                BaseNormal,
                TrackDistance,
                RiverDistance,
                ViewCorridorMask);

            const int32 VertexIndex = BaseTerrainVertexIndex(XIndex, YIndex, SampleCount);
            const float HeightBeforeRiverCarve = FMath::Lerp(BaseHeight, AuthoredProfileHeight, ProfileBlend) + DisplacementCm;
            const float Height = CarveRiverChannelCm(HeightBeforeRiverCarve, Position2D, RiverField, Stats);
            Positions[VertexIndex] = FVector(Position2D.X, Position2D.Y, Height + 25.0f);
            Us[VertexIndex] = static_cast<float>(XIndex) / static_cast<float>(SampleCount - 1);
            Vs[VertexIndex] = static_cast<float>(YIndex) / static_cast<float>(SampleCount - 1);
            if (FMath::Abs(DisplacementCm) > 1.0f)
            {
                ++Stats.DisplacedVertexCount;
                Stats.MaxAbsDisplacementCm = FMath::Max(Stats.MaxAbsDisplacementCm, FMath::Abs(DisplacementCm));
            }
        }
    }
}

// Phase 2: central-difference vertex normals + per-vertex landform colour.
void ComputeBaseTerrainNormalsAndColors(
    const TArray<uint16>& HeightData,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    const TArray<FVector>& Positions,
    TArray<FVector>& Normals,
    TArray<FLinearColor>& Colors)
{
    const int32 SampleCount = BaseTerrainSampleCount();

    const auto PositionAt = [&Positions, SampleCount](int32 XIndex, int32 YIndex) -> const FVector&
    {
        const int32 ClampedX = FMath::Clamp(XIndex, 0, SampleCount - 1);
        const int32 ClampedY = FMath::Clamp(YIndex, 0, SampleCount - 1);
        return Positions[BaseTerrainVertexIndex(ClampedX, ClampedY, SampleCount)];
    };

    for (int32 YIndex = 0; YIndex < SampleCount; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < SampleCount; ++XIndex)
        {
            const int32 VertexIndex = BaseTerrainVertexIndex(XIndex, YIndex, SampleCount);
            const FVector AlongX = PositionAt(XIndex + 1, YIndex) - PositionAt(XIndex - 1, YIndex);
            const FVector AlongY = PositionAt(XIndex, YIndex + 1) - PositionAt(XIndex, YIndex - 1);
            FVector Normal = FVector::CrossProduct(AlongY, AlongX).GetSafeNormal();
            if (Normal.Z < 0.0f)
            {
                Normal *= -1.0f;
            }
            Normals[VertexIndex] = Normal.IsNearlyZero() ? FVector::UpVector : Normal;
            const FVector& Position = Positions[VertexIndex];
            const FVector BaseNormal = YarlungSmoothNormalAtWorldXY(HeightData, Position.X, Position.Y);
            const float RockMask = YarlungCanyonWetRockMask(Position.X, Position.Y, Position.Z, BaseNormal, RiverField);
            Colors[VertexIndex] = YarlungColorAtPosition(Position.X, Position.Y, Position.Z, Normals[VertexIndex], RockMask, RiverField);
        }
    }
}

UStaticMesh* BuildYarlungCorridorTerrainStaticMesh(const TArray<uint16>& HeightData)
{
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/Generated/Materials/M_YarlungMeshTerrain.M_YarlungMeshTerrain"));
    if (!Material)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing required mesh terrain material: /Game/Generated/Materials/M_YarlungMeshTerrain.M_YarlungMeshTerrain"));
        return nullptr;
    }

    TArray<YarlungViewCorridor::FTrackPoint> TrackPoints;
    if (!LoadYarlungTerrainTrackPoints(TrackPoints))
    {
        return nullptr;
    }

    UPackage* MeshPackage = CreatePackage(YarlungCorridorTerrainMeshPackagePath);
    MeshPackage->FullyLoad();
    MeshPackage->Modify();

    if (UObject* Existing = StaticFindObject(UStaticMesh::StaticClass(), MeshPackage, YarlungCorridorTerrainMeshAssetName))
    {
        ResetLoaders(MeshPackage);
        Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch);
    }

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
        MeshPackage,
        FName(YarlungCorridorTerrainMeshAssetName),
        RF_Public | RF_Standalone);
    StaticMesh->InitResources();
    StaticMesh->SetLightingGuid();
    StaticMesh->SetLightMapResolution(64);
    StaticMesh->SetLightMapCoordinateIndex(0);
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("YarlungCorridorTerrain"), TEXT("YarlungCorridorTerrain")));
    StaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);

    FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
    SourceModel.BuildSettings.bRecomputeNormals = false;
    SourceModel.BuildSettings.bRecomputeTangents = false;
    SourceModel.BuildSettings.bRemoveDegenerates = false;
    SourceModel.BuildSettings.bUseHighPrecisionTangentBasis = true;
    SourceModel.BuildSettings.bUseFullPrecisionUVs = true;

    FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
    FStaticMeshAttributes(*MeshDescription).Register();

    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(MeshDescription);
    Builder.SetNumUVLayers(1);

    const int32 SampleCount = BaseTerrainSampleCount();
    const int32 VertexCount = SampleCount * SampleCount;

    TArray<FVector> Positions;
    TArray<FVector> Normals;
    TArray<FLinearColor> Colors;
    TArray<float> Us;
    TArray<float> Vs;
    Positions.SetNumUninitialized(VertexCount);
    Normals.SetNumUninitialized(VertexCount);
    Colors.SetNumUninitialized(VertexCount);
    Us.SetNumUninitialized(VertexCount);
    Vs.SetNumUninitialized(VertexCount);

    FYarlungRiverField RiverField;
    FString RiverLoadError;
    if (!RiverField.LoadFromProjectContent(&RiverLoadError))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read generated Yarlung river field for terrain mesh: %s"), *RiverLoadError);
        return nullptr;
    }

    FBaseTerrainMeshStats Stats;
    ComputeBaseTerrainPositions(HeightData, TrackPoints, RiverField, Positions, Us, Vs, Stats);
    ComputeBaseTerrainNormalsAndColors(HeightData, TrackPoints, RiverField, Positions, Normals, Colors);

    FLinearColor MinColor(
        TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max(),
        TNumericLimits<float>::Max(),
        1.0f);
    FLinearColor MaxColor(
        TNumericLimits<float>::Lowest(),
        TNumericLimits<float>::Lowest(),
        TNumericLimits<float>::Lowest(),
        1.0f);
    FVector3d SumColor = FVector3d::ZeroVector;
    int32 NonWhiteColorCount = 0;
    int32 DarkGreenColorCount = 0;
    for (const FLinearColor& Color : Colors)
    {
        MinColor.R = FMath::Min(MinColor.R, Color.R);
        MinColor.G = FMath::Min(MinColor.G, Color.G);
        MinColor.B = FMath::Min(MinColor.B, Color.B);
        MaxColor.R = FMath::Max(MaxColor.R, Color.R);
        MaxColor.G = FMath::Max(MaxColor.G, Color.G);
        MaxColor.B = FMath::Max(MaxColor.B, Color.B);
        SumColor += FVector3d(Color.R, Color.G, Color.B);
        if (!Color.Equals(FLinearColor::White, 0.001f))
        {
            ++NonWhiteColorCount;
        }
        if (Color.G > Color.R * 1.35f && Color.G > Color.B * 1.15f && Color.G < 0.20f)
        {
            ++DarkGreenColorCount;
        }
    }
    const FVector3d AvgColor = VertexCount > 0 ? SumColor / static_cast<double>(VertexCount) : FVector3d::ZeroVector;
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung terrain source color stats: min=(%.3f,%.3f,%.3f) avg=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f) nonwhite=%d/%d dark_green=%d/%d"),
        MinColor.R,
        MinColor.G,
        MinColor.B,
        AvgColor.X,
        AvgColor.Y,
        AvgColor.Z,
        MaxColor.R,
        MaxColor.G,
        MaxColor.B,
        NonWhiteColorCount,
        VertexCount,
        DarkGreenColorCount,
        VertexCount);

    FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("YarlungCorridorTerrain"));
    TArray<FVertexID> Vertices;
    Vertices.Reserve(VertexCount);
    for (const FVector& Position : Positions)
    {
        Vertices.Add(Builder.AppendVertex(Position));
    }

    const auto AppendTerrainTriangle = [&](int32 X0, int32 Y0, int32 X1, int32 Y1, int32 X2, int32 Y2)
    {
        const int32 I0 = BaseTerrainVertexIndex(X0, Y0, SampleCount);
        const int32 I1 = BaseTerrainVertexIndex(X1, Y1, SampleCount);
        const int32 I2 = BaseTerrainVertexIndex(X2, Y2, SampleCount);
        const FVertexInstanceID VI0 = Builder.AppendInstance(Vertices[I0]);
        const FVertexInstanceID VI1 = Builder.AppendInstance(Vertices[I1]);
        const FVertexInstanceID VI2 = Builder.AppendInstance(Vertices[I2]);
        const FVertexInstanceID Instances[3] = { VI0, VI1, VI2 };
        const int32 Indices[3] = { I0, I1, I2 };

        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const int32 VertexIndex = Indices[Corner];
            Builder.SetInstanceTangentSpace(Instances[Corner], Normals[VertexIndex], FVector::XAxisVector, 1.0f);
            Builder.SetInstanceUV(Instances[Corner], FVector2D(Us[VertexIndex], Vs[VertexIndex]), 0);
            const FLinearColor& Color = Colors[VertexIndex];
            Builder.SetInstanceColor(Instances[Corner], FVector4f(Color.R, Color.G, Color.B, Color.A));
        }

        Builder.AppendTriangle(VI0, VI1, VI2, PolygonGroup);
    };

    for (int32 YIndex = 0; YIndex < SampleCount - 1; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < SampleCount - 1; ++XIndex)
        {
            AppendTerrainTriangle(XIndex, YIndex, XIndex, YIndex + 1, XIndex + 1, YIndex + 1);
            AppendTerrainTriangle(XIndex, YIndex, XIndex + 1, YIndex + 1, XIndex + 1, YIndex);
        }
    }

    StaticMesh->CommitMeshDescription(0);

    FMeshNaniteSettings NaniteSettings = StaticMesh->GetNaniteSettings();
    NaniteSettings.bEnabled = true;
    NaniteSettings.KeepPercentTriangles = 1.0f;
    NaniteSettings.FallbackPercentTriangles = 1.0f;
    StaticMesh->SetNaniteSettings(NaniteSettings);

    FMeshSectionInfo SectionInfo = StaticMesh->GetSectionInfoMap().Get(0, 0);
    SectionInfo.MaterialIndex = 0;
    SectionInfo.bEnableCollision = false;
    StaticMesh->GetSectionInfoMap().Set(0, 0, SectionInfo);
    StaticMesh->GetOriginalSectionInfoMap().Set(0, 0, SectionInfo);

    StaticMesh->Build();
    StaticMesh->PostEditChange();
    StaticMesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(StaticMesh);

    bool bHasRenderColorVertexData = false;
    if (const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData())
    {
        if (RenderData->LODResources.Num() > 0)
        {
            bHasRenderColorVertexData = RenderData->LODResources[0].bHasColorVertexData;
        }
    }
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung terrain render color buffer: has_color_vertex_data=%s"),
        bHasRenderColorVertexData ? TEXT("true") : TEXT("false"));

    if (!UEditorLoadingAndSavingUtils::SavePackages({ MeshPackage }, false))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to save Yarlung corridor terrain asset: %s"), YarlungCorridorTerrainMeshPackagePath);
        return nullptr;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Nanite Yarlung continuous canyon terrain: %s vertices=%d triangles=%d displaced_vertices=%d max_displacement_cm=%.1f river_carved_vertices=%d max_river_carve_cm=%.1f nanite=%s"),
        *StaticMesh->GetPathName(),
        VertexCount,
        (SampleCount - 1) * (SampleCount - 1) * 2,
        Stats.DisplacedVertexCount,
        Stats.MaxAbsDisplacementCm,
        Stats.RiverCarvedVertexCount,
        Stats.MaxRiverCarveCm,
        StaticMesh->IsNaniteEnabled() ? TEXT("true") : TEXT("false"));
    return StaticMesh;
}

UStaticMesh* LoadExistingYarlungCorridorTerrainStaticMesh()
{
    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, YarlungCorridorTerrainMeshObjectPath);
    if (!StaticMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to reuse missing Yarlung corridor terrain asset: %s"), YarlungCorridorTerrainMeshObjectPath);
        return nullptr;
    }
    UE_LOG(LogTemp, Display, TEXT("Reusing existing Yarlung corridor terrain: %s"), *StaticMesh->GetPathName());
    return StaticMesh;
}

bool LoadYarlungHeightmap(TArray<uint16>& OutHeightData)
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    const FString HeightmapPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectContentDir(),
        TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16")));

    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *HeightmapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read landscape heightmap: %s"), *HeightmapPath);
        return false;
    }

    const int32 ExpectedByteCount = Size * Size * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Heightmap has %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return false;
    }

    OutHeightData.SetNumUninitialized(Size * Size);
    FMemory::Memcpy(OutHeightData.GetData(), RawBytes.GetData(), RawBytes.Num());
    return true;
}

bool SpawnYarlungWorldActors(UWorld* World, UStaticMesh* CorridorTerrainAsset)
{
    const auto Spawn = [World](UClass* Class, const TCHAR* Label) -> AActor*
    {
        AActor* Actor = World->SpawnActor<AActor>(Class, FVector::ZeroVector, FRotator::ZeroRotator);
        if (Actor)
        {
            Actor->SetActorLabel(Label);
        }
        return Actor;
    };

    AActor* Terrain = Spawn(AYarlungMeshTerrainActor::StaticClass(), TEXT("YarlungCorridorTerrainScenery"));
    if (!Terrain)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung corridor terrain actor"));
        return false;
    }
    if (UStaticMeshComponent* MeshComponent = Terrain->FindComponentByClass<UStaticMeshComponent>())
    {
        MeshComponent->SetStaticMesh(CorridorTerrainAsset);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung corridor terrain actor has no static mesh component"));
        return false;
    }

    if (!Spawn(ACoasterRideActor::StaticClass(), TEXT("YarlungCoasterRide")))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung coaster ride actor"));
        return false;
    }
    if (!YarlungWaterBuilder::SpawnYarlungWater(World))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn required Yarlung UE Water actors"));
        return false;
    }
    if (!Spawn(AYarlungSceneryActor::StaticClass(), TEXT("YarlungForestRockScenery")))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung forest/rock scenery actor"));
        return false;
    }
    return true;
}
}
#endif

UYarlungLandscapeImportCommandlet::UYarlungLandscapeImportCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UYarlungLandscapeImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    const FString MapPackagePath = TEXT("/Game/Generated/YarlungLandscape/YarlungLandscape_Level");
    const bool bSkipTerrainMeshBuild = Params.Contains(TEXT("SkipTerrainMeshBuild"), ESearchCase::IgnoreCase);

    UStaticMesh* CorridorTerrainAsset = nullptr;
    if (bSkipTerrainMeshBuild)
    {
        CorridorTerrainAsset = LoadExistingYarlungCorridorTerrainStaticMesh();
    }
    else
    {
        TArray<uint16> HeightData;
        if (!LoadYarlungHeightmap(HeightData))
        {
            return 1;
        }
        CorridorTerrainAsset = BuildYarlungCorridorTerrainStaticMesh(HeightData);
    }
    if (!CorridorTerrainAsset)
    {
        return 1;
    }

    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to create blank map"));
        return 1;
    }

    if (!SpawnYarlungWorldActors(World, CorridorTerrainAsset))
    {
        return 1;
    }

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPackagePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to save imported landscape map: %s"), *MapPackagePath);
        return 1;
    }

    FAssetRegistryModule::AssetCreated(World);
    UE_LOG(LogTemp, Display, TEXT("Imported Yarlung landscape map: %s"), *MapPackagePath);
    return 0;
#else
    UE_LOG(LogTemp, Error, TEXT("YarlungLandscapeImport commandlet requires an editor build."));
    return 1;
#endif
}
