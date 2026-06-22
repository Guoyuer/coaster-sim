#include "YarlungCorridorImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungAssetConfig.h"
#include "YarlungRiverField.h"
#include "YarlungRiverSurfaceBuilder.h"
#include "YarlungSceneryActor.h"
#include "YarlungMeshTerrainActor.h"
#include "YarlungGeneratedPaths.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainSurface.h"
#include "YarlungTrackCsv.h"
#include "YarlungViewCorridor.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
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
// Source height grid dimensions + world bounds come from YarlungTerrain::Config()
// (Config/yarlung-terrain.json), shared with the Python pipeline and scenery.
const TCHAR* YarlungCorridorTerrainMeshPackagePath = YarlungGeneratedPaths::CorridorTerrainMeshPackagePath;
const TCHAR* YarlungCorridorTerrainMeshAssetName = YarlungGeneratedPaths::CorridorTerrainMeshAssetName;
const TCHAR* YarlungCorridorTerrainMeshObjectPath = YarlungGeneratedPaths::CorridorTerrainMeshObjectPath;

float YarlungValueNoise(float X, float Y)
{
    return FMath::Frac(FMath::Sin(X * 0.00173f + Y * 0.00291f) * 43758.5453f);
}

float YarlungCanyonWetRockMask(float X, float Y, float Height, const FVector& SurfaceNormal, const FYarlungRiverField& RiverField)
{
    const float SlopeMask = YarlungTerrain::Smooth01(((1.0f - SurfaceNormal.Z) - 0.08f) / 0.30f);
    const float HeightMask = 1.0f - 0.55f * YarlungTerrain::Smooth01((Height - 515000.0f) / 120000.0f);
    const float RiverDistance = RiverField.DistanceCm(FVector2D(X, Y));
    const float DistanceMask = YarlungTerrain::Smooth01((RiverDistance - 16000.0f) / 115000.0f)
        * (1.0f - YarlungTerrain::Smooth01((RiverDistance - 360000.0f) / 150000.0f));
    const float Strata = 0.5f + 0.5f * FMath::Sin(X * 0.0019f - Y * 0.0023f + Height * 0.0041f);
    return FMath::Clamp(DistanceMask * SlopeMask * HeightMask * (0.82f + Strata * 0.50f), 0.0f, 1.0f);
}

float YarlungRavineMask(float X, float Y, const FYarlungRiverField& RiverField)
{
    const float RiverDistance = RiverField.DistanceCm(FVector2D(X, Y));
    const float Along = X / 210000.0f + Y / 567000.0f;
    const float Across = RiverDistance / 86000.0f;
    const float Signal = 0.5f + 0.5f * FMath::Sin(Along * 11.2f + Across * 2.65f - 0.9f);
    return FMath::Pow(FMath::Clamp(Signal, 0.0f, 1.0f), 5.0f);
}

float YarlungTerrainBreakup(float X, float Y, float Height)
{
    const float Broad = 0.5f + 0.5f * FMath::Sin(X * 0.00042f - Y * 0.00058f + Height * 0.00072f);
    const float Mid = 0.5f + 0.5f * FMath::Sin(X * 0.00115f + Y * 0.00173f - Height * 0.0011f);
    const float Fine = YarlungValueNoise(X * 2.9f + Height * 0.17f, Y * 2.1f - Height * 0.13f);
    return FMath::Clamp(Broad * 0.45f + Mid * 0.35f + Fine * 0.20f, 0.0f, 1.0f);
}

FLinearColor YarlungColorAtPosition(float X, float Y, float Height, const FVector& Normal, float RockMask, const FYarlungRiverField& RiverField)
{
    const float Height01 = YarlungTerrain::NormalizeEncodedHeightCm(Height);
    const float RiverDistance = RiverField.DistanceCm(FVector2D(X, Y));
    const float Slope = 1.0f - Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector).Z;
    const float MidSlope = YarlungTerrain::Smooth01((Slope - 0.08f) / 0.24f);
    const float SteepSlope = YarlungTerrain::Smooth01((Slope - 0.22f) / 0.30f);
    const float ForestElevation = 1.0f - 0.36f * YarlungTerrain::Smooth01((Height01 - 0.88f) / 0.16f);
    const float ForestDistance = YarlungTerrain::Smooth01((RiverDistance - 1200.0f) / 38000.0f)
        * (1.0f - YarlungTerrain::Smooth01((RiverDistance - 430000.0f) / 190000.0f));
    const float WetBank = (1.0f - YarlungTerrain::Smooth01((RiverDistance - 6000.0f) / 18000.0f))
        * (1.0f - YarlungTerrain::Smooth01((Height01 - 0.86f) / 0.14f));
    const float MossBank = (1.0f - YarlungTerrain::Smooth01((RiverDistance - 12000.0f) / 48000.0f))
        * (1.0f - YarlungTerrain::Smooth01((Height01 - 0.90f) / 0.10f));
    const float BroadCanopyPatch = 0.5f + 0.5f * FMath::Sin(X * 0.00088f - Y * 0.00116f + Height * 0.0011f);
    const float FineCanopyPatch = YarlungValueNoise(X * 1.7f, Y * 1.7f);
    const float CanopyMask = YarlungTerrain::Smooth01((BroadCanopyPatch * 0.72f + FineCanopyPatch * 0.28f - 0.22f) / 0.48f);
    const float Breakup = YarlungTerrainBreakup(X, Y, Height);
    const float MacroPatchNoise = FMath::Clamp(
        YarlungValueNoise(X * 0.052f + Height * 0.018f, Y * 0.041f - Height * 0.013f) * 0.62f
            + YarlungValueNoise(X * 0.118f - Height * 0.011f, Y * 0.093f + Height * 0.017f) * 0.38f,
        0.0f,
        1.0f);
    const float CanyonSlopeBand = YarlungTerrain::Smooth01((RiverDistance - 18000.0f) / 90000.0f)
        * (1.0f - YarlungTerrain::Smooth01((RiverDistance - 300000.0f) / 150000.0f));
    const float SlopePatch = FMath::Clamp(
        CanyonSlopeBand
            * YarlungTerrain::Smooth01((Slope - 0.025f) / 0.20f)
            * YarlungTerrain::Smooth01((MacroPatchNoise - 0.30f) / 0.44f),
        0.0f,
        1.0f);
    const float ScreePatch = FMath::Clamp(
        CanyonSlopeBand
            * SteepSlope
            * YarlungTerrain::Smooth01((MacroPatchNoise - 0.58f) / 0.30f),
        0.0f,
        1.0f);
    const float Forest = FMath::Clamp(
        ForestDistance
            * ForestElevation
            * FMath::Lerp(1.04f, 1.20f, MidSlope)
            * FMath::Lerp(0.92f, 1.0f, CanopyMask)
            * FMath::Lerp(1.0f, 0.46f, SteepSlope),
        0.0f,
        1.0f);
    const float Ravine = MidSlope * YarlungRavineMask(X, Y, RiverField);
    const float RavineStreak = FMath::Pow(
        FMath::Clamp(0.5f + 0.5f * FMath::Sin(X * 0.0025f - Y * 0.0014f + Height * 0.0038f), 0.0f, 1.0f),
        4.0f);
    const float Noise = YarlungValueNoise(X, Y);

    const FLinearColor DeepForest(0.018f + Noise * 0.012f, 0.080f + Noise * 0.050f, 0.038f + Noise * 0.030f, 1.0f);
    const FLinearColor SunForest(0.030f + Noise * 0.018f, 0.155f + Noise * 0.070f, 0.068f + Noise * 0.044f, 1.0f);
    const FLinearColor WeatheredRock(0.060f + Height01 * 0.026f, 0.066f + Height01 * 0.024f, 0.063f + Height01 * 0.020f, 1.0f);
    const FLinearColor WetRock(0.028f + Noise * 0.020f, 0.041f + Noise * 0.030f, 0.041f + Noise * 0.026f, 1.0f);
    const FLinearColor MossRock(0.022f + Noise * 0.018f, 0.105f + Noise * 0.052f, 0.050f + Noise * 0.034f, 1.0f);
    const FLinearColor Scree(0.074f + Noise * 0.038f, 0.074f + Noise * 0.034f, 0.068f + Noise * 0.030f, 1.0f);
    const FLinearColor RavineColor(0.006f, 0.014f, 0.014f, 1.0f);
    const FLinearColor Snow(0.66f, 0.70f, 0.69f, 1.0f);

    FLinearColor Base = FMath::Lerp(DeepForest, SunForest, CanopyMask * (1.0f - SteepSlope * 0.45f));
    Base = FMath::Lerp(WeatheredRock, Base, Forest);
    Base = FMath::Lerp(Base, MossRock, FMath::Clamp(MossBank * 0.36f, 0.0f, 0.36f));
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(WetBank * 0.44f, 0.0f, 0.44f));
    Base = FMath::Lerp(Base, Scree, FMath::Clamp(WetBank * (0.04f + Noise * 0.08f), 0.0f, 0.12f));
    Base = FMath::Lerp(Base, MossRock, FMath::Clamp(SlopePatch * (0.20f + (1.0f - SteepSlope) * 0.16f), 0.0f, 0.34f));
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(SlopePatch * (0.14f + SteepSlope * 0.24f), 0.0f, 0.36f));
    Base = FMath::Lerp(Base, Scree, FMath::Clamp(ScreePatch * 0.30f, 0.0f, 0.30f));
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(RockMask * 0.94f + SteepSlope * RavineStreak * 0.30f, 0.0f, 0.94f));
    Base = FMath::Lerp(Base, Scree, FMath::Clamp(SteepSlope * (1.0f - Forest) * (0.16f + RavineStreak * 0.20f), 0.0f, 0.34f));
    Base = FMath::Lerp(Base, RavineColor, FMath::Clamp(Ravine * 0.58f, 0.0f, 0.68f));
    Base = FMath::Lerp(Base, Snow, 0.08f * YarlungTerrain::Smooth01((Height01 - 0.997f) / 0.012f));
    const float RockSurfaceMask = FMath::Clamp(
        RockMask * 1.05f
            + SteepSlope * (1.0f - Forest) * 0.70f
            + WetBank * 0.34f
            + Ravine * 0.45f
            + SlopePatch * 0.34f
            + ScreePatch * 0.26f,
        0.0f,
        1.0f);
    const float ContrastGain = FMath::Lerp(0.78f, 1.18f, Breakup);
    Base.R *= ContrastGain;
    Base.G *= FMath::Lerp(0.84f, 1.14f, Breakup);
    Base.B *= FMath::Lerp(0.80f, 1.08f, Breakup);
    Base.A = RockSurfaceMask;
    return Base;
}

bool LoadYarlungTerrainTrackPoints(TArray<YarlungViewCorridor::FTrackPoint>& OutTrackPoints)
{
    const FString Path = YarlungGeneratedPaths::ProjectContentFile(YarlungGeneratedPaths::TrackCsvRelative);
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

// Continuous canyon base mesh. The track-view corridor is still the visual
// priority, but the foundation must be a non-self-intersecting world surface.
// A track-swept strip folded over itself on loops/turns and clipped the camera.
constexpr int32 BaseTerrainStride = 1;
using FBaseTerrainMeshStats = YarlungTerrainSurface::FSurfaceStats;

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

// Phase 1: continuous DEM-led surface positions and UVs.
void ComputeBaseTerrainPositions(
    const TArray<uint16>& EncodedHeights,
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
            const int32 VertexIndex = BaseTerrainVertexIndex(XIndex, YIndex, SampleCount);
            Positions[VertexIndex] = FVector(
                Position2D.X,
                Position2D.Y,
                YarlungTerrainSurface::SurfaceZCm(EncodedHeights, TrackPoints, RiverField, Position2D, &Stats));
            Us[VertexIndex] = static_cast<float>(XIndex) / static_cast<float>(SampleCount - 1);
            Vs[VertexIndex] = static_cast<float>(YIndex) / static_cast<float>(SampleCount - 1);
        }
    }
}

// Phase 2: central-difference vertex normals + per-vertex landform colour.
void ComputeBaseTerrainNormalsAndColors(
    const TArray<uint16>& EncodedHeights,
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
            const float RockMask = YarlungCanyonWetRockMask(Position.X, Position.Y, Position.Z, Normals[VertexIndex], RiverField);
            Colors[VertexIndex] = YarlungColorAtPosition(Position.X, Position.Y, Position.Z, Normals[VertexIndex], RockMask, RiverField);
        }
    }
}

UStaticMesh* BuildYarlungCorridorTerrainStaticMesh(const TArray<uint16>& EncodedHeights)
{
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, YarlungGeneratedPaths::MeshTerrainMaterialObjectPath);
    if (!Material)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Missing required mesh terrain material: %s"), YarlungGeneratedPaths::MeshTerrainMaterialObjectPath);
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
    if (!RiverField.LoadGeneratedCsv(&RiverLoadError))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read generated Yarlung river field for terrain mesh: %s"), *RiverLoadError);
        return nullptr;
    }

    FBaseTerrainMeshStats Stats;
    ComputeBaseTerrainPositions(EncodedHeights, TrackPoints, RiverField, Positions, Us, Vs, Stats);
    ComputeBaseTerrainNormalsAndColors(EncodedHeights, TrackPoints, RiverField, Positions, Normals, Colors);

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

bool LoadCorridorSourceHeights(TArray<uint16>& OutEncodedHeights)
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    const FString HeightmapPath = FPaths::ConvertRelativePathToFull(
        YarlungGeneratedPaths::ProjectContentFile(YarlungGeneratedPaths::HeightmapRelative));

    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *HeightmapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read corridor source heights: %s"), *HeightmapPath);
        return false;
    }

    const int32 ExpectedByteCount = Size * Size * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Corridor source heights have %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return false;
    }

    OutEncodedHeights.SetNumUninitialized(Size * Size);
    FMemory::Memcpy(OutEncodedHeights.GetData(), RawBytes.GetData(), RawBytes.Num());
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
    // Visible hero water: an explicit sloped river surface that follows the bed,
    // so the water is never buried by the descending valley floor.
    {
        FYarlungRiverField RiverSurfaceField;
        FString RiverSurfaceError;
        if (!RiverSurfaceField.LoadGeneratedCsv(&RiverSurfaceError))
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to read river field for river surface mesh: %s"), *RiverSurfaceError);
            return false;
        }
        UStaticMesh* RiverSurfaceMesh = YarlungRiverSurfaceBuilder::BuildStaticMesh(RiverSurfaceField);
        if (!RiverSurfaceMesh)
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to build Yarlung river surface mesh"));
            return false;
        }
        AStaticMeshActor* RiverSurface = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (!RiverSurface)
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung river surface actor"));
            return false;
        }
        RiverSurface->SetActorLabel(TEXT("YarlungRiverSurface"));
        RiverSurface->SetMobility(EComponentMobility::Static);
        if (UStaticMeshComponent* RiverSurfaceComponent = RiverSurface->GetStaticMeshComponent())
        {
            RiverSurfaceComponent->SetStaticMesh(RiverSurfaceMesh);
            RiverSurfaceComponent->SetMobility(EComponentMobility::Static);
        }
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

UYarlungCorridorImportCommandlet::UYarlungCorridorImportCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UYarlungCorridorImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    const FString MapPackagePath = YarlungGeneratedPaths::CorridorMapPackagePath;
    const bool bSkipTerrainMeshBuild = Params.Contains(TEXT("SkipTerrainMeshBuild"), ESearchCase::IgnoreCase);

    UStaticMesh* CorridorTerrainAsset = nullptr;
    if (bSkipTerrainMeshBuild)
    {
        CorridorTerrainAsset = LoadExistingYarlungCorridorTerrainStaticMesh();
    }
    else
    {
        TArray<uint16> EncodedHeights;
        if (!LoadCorridorSourceHeights(EncodedHeights))
        {
            return 1;
        }
        CorridorTerrainAsset = BuildYarlungCorridorTerrainStaticMesh(EncodedHeights);
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
        UE_LOG(LogTemp, Error, TEXT("Unable to save imported corridor map: %s"), *MapPackagePath);
        return 1;
    }

    FAssetRegistryModule::AssetCreated(World);
    UE_LOG(LogTemp, Display, TEXT("Imported Yarlung corridor map: %s"), *MapPackagePath);
    return 0;
#else
    UE_LOG(LogTemp, Error, TEXT("YarlungCorridorImport commandlet requires an editor build."));
    return 1;
#endif
}
