#include "YarlungCorridorImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungAssetConfig.h"
#include "YarlungRiverField.h"
#include "YarlungSceneryActor.h"
#include "YarlungMeshTerrainActor.h"
#include "YarlungCorridorProfile.h"
#include "YarlungGeneratedPaths.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainRelief.h"
#include "YarlungTrackCsv.h"
#include "YarlungViewCorridor.h"
#include "YarlungWaterBuilder.h"
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

float YarlungHeightAtGrid(const TArray<uint16>& EncodedHeights, int32 XIndex, int32 YIndex)
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    const int32 ClampedX = FMath::Clamp(XIndex, 0, Size - 1);
    const int32 ClampedY = FMath::Clamp(YIndex, 0, Size - 1);
    return YarlungTerrain::HeightValueToCm(EncodedHeights[ClampedY * Size + ClampedX]);
}

float YarlungHeightAtSourceGrid(const TArray<uint16>& EncodedHeights, float SourceX, float SourceY)
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
            Samples[Column] = YarlungHeightAtGrid(EncodedHeights, BaseX + Column - 1, BaseY + Row - 1);
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

float YarlungHeightAtWorldXY(const TArray<uint16>& EncodedHeights, float X, float Y)
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const float U = FMath::Clamp((X - Tc.MinXCm) / (Tc.MaxXCm - Tc.MinXCm), 0.0f, 1.0f);
    const float V = FMath::Clamp((Y - Tc.MinYCm) / (Tc.MaxYCm - Tc.MinYCm), 0.0f, 1.0f);
    return YarlungHeightAtSourceGrid(
        EncodedHeights,
        U * static_cast<float>(Tc.GridSize - 1),
        V * static_cast<float>(Tc.GridSize - 1));
}

FVector YarlungSmoothNormalAtWorldXY(const TArray<uint16>& EncodedHeights, float X, float Y)
{
    constexpr float SampleSpacingCm = 900.0f;
    const float Left = YarlungHeightAtWorldXY(EncodedHeights, X - SampleSpacingCm, Y);
    const float Right = YarlungHeightAtWorldXY(EncodedHeights, X + SampleSpacingCm, Y);
    const float Down = YarlungHeightAtWorldXY(EncodedHeights, X, Y - SampleSpacingCm);
    const float Up = YarlungHeightAtWorldXY(EncodedHeights, X, Y + SampleSpacingCm);
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

    const FLinearColor DeepForest(0.010f + Noise * 0.008f, 0.050f + Noise * 0.036f, 0.024f + Noise * 0.022f, 1.0f);
    const FLinearColor SunForest(0.018f + Noise * 0.014f, 0.104f + Noise * 0.058f, 0.044f + Noise * 0.034f, 1.0f);
    const FLinearColor WeatheredRock(0.044f + Height01 * 0.020f, 0.048f + Height01 * 0.020f, 0.047f + Height01 * 0.018f, 1.0f);
    const FLinearColor WetRock(0.020f + Noise * 0.016f, 0.029f + Noise * 0.022f, 0.030f + Noise * 0.020f, 1.0f);
    const FLinearColor MossRock(0.012f + Noise * 0.014f, 0.066f + Noise * 0.040f, 0.034f + Noise * 0.026f, 1.0f);
    const FLinearColor Scree(0.054f + Noise * 0.030f, 0.054f + Noise * 0.028f, 0.050f + Noise * 0.026f, 1.0f);
    const FLinearColor RavineColor(0.006f, 0.014f, 0.014f, 1.0f);
    const FLinearColor Snow(0.66f, 0.70f, 0.69f, 1.0f);

    FLinearColor Base = FMath::Lerp(DeepForest, SunForest, CanopyMask * (1.0f - SteepSlope * 0.45f));
    Base = FMath::Lerp(WeatheredRock, Base, Forest);
    Base = FMath::Lerp(Base, MossRock, FMath::Clamp(MossBank * 0.36f, 0.0f, 0.36f));
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(WetBank * 0.44f, 0.0f, 0.44f));
    Base = FMath::Lerp(Base, Scree, FMath::Clamp(WetBank * (0.04f + Noise * 0.08f), 0.0f, 0.12f));
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(RockMask * 0.82f + SteepSlope * RavineStreak * 0.22f, 0.0f, 0.90f));
    Base = FMath::Lerp(Base, Scree, FMath::Clamp(SteepSlope * (1.0f - Forest) * (0.12f + RavineStreak * 0.16f), 0.0f, 0.26f));
    Base = FMath::Lerp(Base, RavineColor, FMath::Clamp(Ravine * 0.58f, 0.0f, 0.68f));
    Base = FMath::Lerp(Base, Snow, 0.08f * YarlungTerrain::Smooth01((Height01 - 0.997f) / 0.012f));
    const float ContrastGain = FMath::Lerp(0.78f, 1.18f, Breakup);
    Base.R *= ContrastGain;
    Base.G *= FMath::Lerp(0.84f, 1.14f, Breakup);
    Base.B *= FMath::Lerp(0.80f, 1.08f, Breakup);
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

    const float InnerBankCm = River.WaterHalfWidthCm + 1000.0f;
    const float OuterBankCm = River.WaterHalfWidthCm + 12000.0f;
    if (River.DistanceCm >= OuterBankCm)
    {
        return HeightCm;
    }

    const float BankT = YarlungTerrain::Smooth01((River.DistanceCm - InnerBankCm) / (OuterBankCm - InnerBankCm));
    const float CarveAlpha = 1.0f - BankT;
    const float BankRiseCm = FMath::Max(0.0f, River.DistanceCm - River.WaterHalfWidthCm) * 0.90f;
    const float TargetHeightCm = River.WaterSurfaceZCm - 80.0f + BankRiseCm;
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
            const float BaseHeight = YarlungHeightAtWorldXY(EncodedHeights, Position2D.X, Position2D.Y);
            const FVector BaseNormal = YarlungSmoothNormalAtWorldXY(EncodedHeights, Position2D.X, Position2D.Y);
            const float TrackDistance = YarlungViewCorridor::DistanceToTrackCm(TrackPoints, Position2D);
            const float ViewCorridorMask = YarlungViewCorridor::ComputeMask(TrackPoints, Position2D);
            FVector2D ProfileCenter = Position2D;
            float SignedOffsetCm = 0.0f;
            const bool bHasProfileFrame = FindNearestTrackProfileFrame(TrackPoints, Position2D, ProfileCenter, SignedOffsetCm);
            const float TrackBaseHeight = bHasProfileFrame
                ? YarlungHeightAtWorldXY(EncodedHeights, ProfileCenter.X, ProfileCenter.Y)
                : BaseHeight;
            const float CorridorProfileHeight = bHasProfileFrame
                ? YarlungCorridorProfile::CorridorTerrainHeightCm(ProfileCenter, SignedOffsetCm, TrackBaseHeight, BaseHeight)
                : BaseHeight;
            const float ProfileBlend = FMath::Clamp(ViewCorridorMask, 0.0f, 1.0f)
                * YarlungTerrain::Smooth01((TrackDistance - 30000.0f) / 52000.0f);
            const float RelaxedProfileHeight = BaseHeight + (CorridorProfileHeight - BaseHeight) * 0.48f;
            const float RiverDistance = RiverField.DistanceCm(Position2D);
            const float DisplacementCm = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
                Position2D,
                RelaxedProfileHeight,
                BaseNormal,
                TrackDistance,
                RiverDistance,
                ViewCorridorMask);

            const int32 VertexIndex = BaseTerrainVertexIndex(XIndex, YIndex, SampleCount);
            const float HeightBeforeRiverCarve = FMath::Lerp(BaseHeight, RelaxedProfileHeight, ProfileBlend) + DisplacementCm;
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
            const FVector BaseNormal = YarlungSmoothNormalAtWorldXY(EncodedHeights, Position.X, Position.Y);
            const float RockMask = YarlungCanyonWetRockMask(Position.X, Position.Y, Position.Z, BaseNormal, RiverField);
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

// Explicit sloped river-surface ribbon. UE Water's WaterZone renders the river
// surface as a flat plane at the zone Z, which the descending valley floor buries
// for most of the route. This mesh follows the riverbed spline at a shallow
// authored surface height and stays inside the carved thalweg instead of reading
// as a raised floodplain slab.
const TCHAR* YarlungRiverSurfaceMeshPackagePath = YarlungGeneratedPaths::RiverSurfaceMeshPackagePath;
const TCHAR* YarlungRiverSurfaceMeshAssetName = YarlungGeneratedPaths::RiverSurfaceMeshAssetName;

UStaticMesh* BuildYarlungRiverSurfaceStaticMesh(const FYarlungRiverField& RiverField)
{
    const FYarlungWaterConfig& WaterConfig = YarlungAssets::Config().Water;
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr,
        *WaterConfig.SurfaceMaterialPath);
    if (!Material)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Missing required river surface material: %s"), *WaterConfig.SurfaceMaterialPath);
    }

    const TArray<FYarlungRiverRow>& Rows = RiverField.GetRows();
    if (Rows.Num() < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few river rows to build river surface: %d"), Rows.Num());
        return nullptr;
    }

    UPackage* MeshPackage = CreatePackage(YarlungRiverSurfaceMeshPackagePath);
    MeshPackage->FullyLoad();
    MeshPackage->Modify();
    if (UObject* Existing = StaticFindObject(UStaticMesh::StaticClass(), MeshPackage, YarlungRiverSurfaceMeshAssetName))
    {
        ResetLoaders(MeshPackage);
        Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch);
    }

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
        MeshPackage,
        FName(YarlungRiverSurfaceMeshAssetName),
        RF_Public | RF_Standalone);
    StaticMesh->InitResources();
    StaticMesh->SetLightingGuid();
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("YarlungRiverSurface"), TEXT("YarlungRiverSurface")));
    StaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);

    FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
    SourceModel.BuildSettings.bRecomputeNormals = true;
    SourceModel.BuildSettings.bRecomputeTangents = true;
    SourceModel.BuildSettings.bRemoveDegenerates = false;
    SourceModel.BuildSettings.bUseFullPrecisionUVs = true;

    FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
    FStaticMeshAttributes(*MeshDescription).Register();
    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(MeshDescription);
    Builder.SetNumUVLayers(1);

    // Multiple cross-river lanes let vertex color carry bank foam and rapid streaks.
    const int32 RowCount = Rows.Num();
    constexpr int32 CrossSamples = 33;
    TArray<FVertexID> VertexIds;
    TArray<FVector2D> VertexUvs;
    TArray<FVector4f> VertexColors;
    VertexIds.SetNumUninitialized(RowCount * CrossSamples);
    VertexUvs.SetNumUninitialized(RowCount * CrossSamples);
    VertexColors.SetNumUninitialized(RowCount * CrossSamples);
    float MinSurfaceZ = TNumericLimits<float>::Max();
    float MaxSurfaceZ = -TNumericLimits<float>::Max();
    int32 FoamVertexCount = 0;
    const auto VertexIndex = [](int32 RowIndex, int32 CrossIndex)
    {
        return RowIndex * CrossSamples + CrossIndex;
    };
    for (int32 Index = 0; Index < RowCount; ++Index)
    {
        const FVector Center = Rows[Index].PositionCm;
        const FVector Next = Rows[FMath::Min(Index + 1, RowCount - 1)].PositionCm;
        const FVector Prev = Rows[FMath::Max(Index - 1, 0)].PositionCm;
        FVector2D Tangent = FVector2D(Next.X - Prev.X, Next.Y - Prev.Y).GetSafeNormal();
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector2D(1.0f, 0.0f);
        }
        const FVector2D Normal(-Tangent.Y, Tangent.X);
        // Keep the ribbon inside the flat-carved channel so its banks clear terrain.
        const float HalfWidth = FMath::Clamp(Rows[Index].HalfWidthCm * 0.34f, 3600.0f, 8200.0f);
        const float Flow = FMath::Clamp(Rows[Index].Flow, 0.0f, 1.0f);
        const float SegmentDropCm = FMath::Abs(Next.Z - Prev.Z);
        const float SegmentLengthCm = FMath::Max(1.0f, FVector2D(Next.X - Prev.X, Next.Y - Prev.Y).Size());
        const float SlopeRapid = YarlungTerrain::Smooth01((SegmentDropCm / SegmentLengthCm - 0.018f) / 0.055f);
        for (int32 CrossIndex = 0; CrossIndex < CrossSamples; ++CrossIndex)
        {
            const float Across01 = static_cast<float>(CrossIndex) / static_cast<float>(CrossSamples - 1);
            const float AcrossSigned = 1.0f - Across01 * 2.0f;
            const float EdgeFoam = YarlungTerrain::Smooth01((FMath::Abs(AcrossSigned) - 0.78f) / 0.18f);
            const float CenterRapid = (1.0f - YarlungTerrain::Smooth01(FMath::Abs(AcrossSigned) / 0.34f))
                * (0.45f + 0.55f * SlopeRapid);
            const float BrokenStripe = FMath::Pow(
                FMath::Clamp(0.5f + 0.5f * FMath::Sin(Flow * 140.0f + AcrossSigned * 15.0f), 0.0f, 1.0f),
                4.0f);
            const float FastStreak = FMath::Pow(
                FMath::Clamp(0.5f + 0.5f * FMath::Sin(Flow * 760.0f + AcrossSigned * 31.0f), 0.0f, 1.0f),
                5.0f);
            const float CrossWake = FMath::Pow(1.0f - FMath::Abs(AcrossSigned), 2.2f);
            const float Foam = FMath::Clamp(
                EdgeFoam * (0.06f + 0.08f * BrokenStripe)
                    + CenterRapid * (BrokenStripe * 0.18f + FastStreak * 0.26f),
                0.0f,
                0.48f);
            const float RippleZ = (22.0f + 34.0f * CrossWake) * FMath::Sin(Flow * 210.0f + AcrossSigned * 8.0f)
                + 14.0f * FMath::Sin(Flow * 390.0f - AcrossSigned * 17.0f)
                + 8.0f * FastStreak;
            const float SurfaceZ = Center.Z + FYarlungRiverField::DefaultWaterSurfaceLiftCm + RippleZ;
            MinSurfaceZ = FMath::Min(MinSurfaceZ, SurfaceZ);
            MaxSurfaceZ = FMath::Max(MaxSurfaceZ, SurfaceZ);

            const FVector Position(
                Center.X + Normal.X * HalfWidth * AcrossSigned,
                Center.Y + Normal.Y * HalfWidth * AcrossSigned,
                SurfaceZ);
            const int32 Id = VertexIndex(Index, CrossIndex);
            VertexIds[Id] = Builder.AppendVertex(Position);
            VertexUvs[Id] = FVector2D(Across01, Flow * 18.0f);

            const FLinearColor DeepWater(0.006f, 0.060f, 0.070f, 1.0f);
            const FLinearColor GlacialGreen(0.026f, 0.170f, 0.145f, 1.0f);
            const FLinearColor AeratedFoam(0.66f, 0.78f, 0.70f, 1.0f);
            const float ChannelTint = 0.18f + 0.20f * BrokenStripe + 0.18f * (1.0f - CrossWake);
            const FLinearColor WaterColor = FMath::Lerp(DeepWater, GlacialGreen, FMath::Clamp(ChannelTint, 0.0f, 0.52f));
            const FLinearColor FinalColor = FMath::Lerp(WaterColor, AeratedFoam, Foam);
            VertexColors[Id] = FVector4f(FinalColor.R, FinalColor.G, FinalColor.B, 1.0f);
            if (Foam > 0.12f)
            {
                ++FoamVertexCount;
            }
        }
    }

    FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("YarlungRiverSurface"));
    const FVector SurfaceNormal = FVector::UpVector;
    const auto AppendTri = [&](int32 A, int32 B, int32 C)
    {
        const FVertexInstanceID IA = Builder.AppendInstance(VertexIds[A]);
        const FVertexInstanceID IB = Builder.AppendInstance(VertexIds[B]);
        const FVertexInstanceID IC = Builder.AppendInstance(VertexIds[C]);
        const FVertexInstanceID Instances[3] = { IA, IB, IC };
        const FVector2D Uvs[3] = { VertexUvs[A], VertexUvs[B], VertexUvs[C] };
        const FVector4f Colors[3] = { VertexColors[A], VertexColors[B], VertexColors[C] };
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Builder.SetInstanceTangentSpace(Instances[Corner], SurfaceNormal, FVector::XAxisVector, 1.0f);
            Builder.SetInstanceUV(Instances[Corner], Uvs[Corner], 0);
            Builder.SetInstanceColor(Instances[Corner], Colors[Corner]);
        }
        Builder.AppendTriangle(IA, IB, IC, PolygonGroup);
    };

    for (int32 Index = 0; Index + 1 < RowCount; ++Index)
    {
        for (int32 CrossIndex = 0; CrossIndex + 1 < CrossSamples; ++CrossIndex)
        {
            const int32 A = VertexIndex(Index, CrossIndex);
            const int32 B = VertexIndex(Index + 1, CrossIndex);
            const int32 C = VertexIndex(Index + 1, CrossIndex + 1);
            const int32 D = VertexIndex(Index, CrossIndex + 1);
            // Wound counter-clockwise seen from above so the surface faces up.
            AppendTri(A, B, C);
            AppendTri(A, C, D);
        }
    }

    StaticMesh->CommitMeshDescription(0);

    FMeshSectionInfo SectionInfo = StaticMesh->GetSectionInfoMap().Get(0, 0);
    SectionInfo.MaterialIndex = 0;
    SectionInfo.bEnableCollision = false;
    StaticMesh->GetSectionInfoMap().Set(0, 0, SectionInfo);
    StaticMesh->GetOriginalSectionInfoMap().Set(0, 0, SectionInfo);

    StaticMesh->Build();
    StaticMesh->PostEditChange();
    StaticMesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(StaticMesh);

    if (!UEditorLoadingAndSavingUtils::SavePackages({ MeshPackage }, false))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to save Yarlung river surface asset: %s"), YarlungRiverSurfaceMeshPackagePath);
        return nullptr;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Yarlung river surface ribbon: %s rows=%d cross_lanes=%d foam_vertices=%d surface_z_cm=%.0f..%.0f"),
        *StaticMesh->GetPathName(),
        RowCount,
        CrossSamples,
        FoamVertexCount,
        MinSurfaceZ,
        MaxSurfaceZ);
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
    if (!YarlungWaterBuilder::SpawnYarlungWater(World))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn required Yarlung UE Water actors"));
        return false;
    }

    // Visible hero water: an explicit sloped river surface that follows the bed,
    // so the water is never buried by the descending valley floor. UE Water's
    // dynamic GPU water collapses flat for this river (113 m drop, 363 m wide in a
    // single zone), so this mesh is the actual rendered water surface.
    {
        FYarlungRiverField RiverSurfaceField;
        FString RiverSurfaceError;
        if (!RiverSurfaceField.LoadGeneratedCsv(&RiverSurfaceError))
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to read river field for river surface mesh: %s"), *RiverSurfaceError);
            return false;
        }
        UStaticMesh* RiverSurfaceMesh = BuildYarlungRiverSurfaceStaticMesh(RiverSurfaceField);
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
