#include "YarlungLandscapeImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungRiverActor.h"
#include "YarlungSceneryActor.h"
#include "YarlungCorridorProfile.h"
#include "YarlungMeshTerrainActor.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainRelief.h"
#include "YarlungTrackCsv.h"
#include "YarlungViewCorridor.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "Misc/FileHelper.h"
#include "StaticMeshAttributes.h"

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

float YarlungCorridorRockMask(const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints, float X, float Y, float Height, const FVector& BaseNormal)
{
    const float TrackDistance = YarlungViewCorridor::DistanceToTrackCm(TrackPoints, FVector2D(X, Y));
    const float NearFade = YarlungTerrain::Smooth01((TrackDistance - 1800.0f) / 6200.0f);
    const float FarFade = 1.0f - YarlungTerrain::Smooth01((TrackDistance - 260000.0f) / 140000.0f);
    const float DistanceMask = NearFade * FarFade;
    const float SlopeMask = YarlungTerrain::Smooth01(((1.0f - BaseNormal.Z) - 0.13f) / 0.38f);
    const float HeightMask = 1.0f - 0.55f * YarlungTerrain::Smooth01((Height - 515000.0f) / 120000.0f);
    return FMath::Clamp(DistanceMask * SlopeMask * HeightMask, 0.0f, 1.0f);
}

float YarlungCorridorLandformDeltaCm(const FVector2D& Center, float SignedOffsetCm, float BaseHeight)
{
    const float AbsOffset = FMath::Abs(SignedOffsetCm);
    const float WallMask = YarlungTerrain::Smooth01((AbsOffset - 16000.0f) / 42000.0f)
        * (1.0f - YarlungTerrain::Smooth01((AbsOffset - 85000.0f) / 12000.0f));
    if (WallMask <= 0.001f)
    {
        return 0.0f;
    }

    const float Height01 = YarlungTerrain::NormalizeEncodedHeightCm(BaseHeight);
    const float HeightMask = 1.0f - 0.55f * YarlungTerrain::Smooth01((Height01 - 0.52f) / 0.22f);
    const float Side = SignedOffsetCm >= 0.0f ? 1.0f : -1.0f;
    const float Along = Center.X * 0.00047f + Center.Y * 0.00031f;
    const float Across = AbsOffset * 0.000055f;

    const float Buttress =
        0.62f * FMath::Sin(Along + Side * 0.9f)
        + 0.38f * FMath::Sin(Along * 1.73f - Across * 0.7f + Side * 1.6f);
    const float RavineSignal = 0.5f + 0.5f * FMath::Sin(Along * 2.45f + Across * 1.35f + Side * 0.35f);
    const float Ravine = FMath::Pow(FMath::Clamp(RavineSignal, 0.0f, 1.0f), 5.0f);
    const float Talus = YarlungTerrain::Smooth01((AbsOffset - 22000.0f) / 26000.0f)
        * (1.0f - YarlungTerrain::Smooth01((AbsOffset - 68000.0f) / 24000.0f));

    const float Delta = Buttress * 1850.0f - Ravine * 2100.0f - Talus * 650.0f;
    return WallMask * HeightMask * Delta;
}

FLinearColor YarlungColorAtPosition(float X, float Y, float Height, float RockMask)
{
    const float Height01 = YarlungTerrain::NormalizeEncodedHeightCm(Height);
    const float RiverDistance = FMath::Abs(Y - YarlungTerrain::RiverCenterY(X));
    const float River = FMath::Clamp(1.0f - RiverDistance / YarlungTerrain::Config().RiverMaskHalfWidthCm, 0.0f, 1.0f);
    const float Forest = YarlungTerrain::Smooth01((RiverDistance - 18000.0f) / 115000.0f) * (1.0f - YarlungTerrain::Smooth01((Height01 - 0.60f) / 0.26f));
    const float Noise = YarlungValueNoise(X, Y);

    FLinearColor Base(0.075f + Noise * 0.020f, 0.145f + Noise * 0.035f, 0.120f + Noise * 0.025f, 1.0f);
    const FLinearColor Rock(0.105f + Height01 * 0.035f, 0.145f + Height01 * 0.035f, 0.140f + Height01 * 0.030f, 1.0f);
    const FLinearColor ForestColor(0.018f + Noise * 0.026f, 0.115f + Noise * 0.080f, 0.048f + Noise * 0.035f, 1.0f);
    const FLinearColor RiverColor(0.50f, 0.78f, 0.74f, 1.0f);
    const FLinearColor Snow(0.58f, 0.64f, 0.63f, 1.0f);

    Base = FMath::Lerp(Base, Rock, FMath::Clamp(Height01 * 0.50f, 0.0f, 0.36f));
    Base = FMath::Lerp(Base, ForestColor, FMath::Clamp(Forest, 0.0f, 0.82f));
    Base = FMath::Lerp(Base, RiverColor, River * 0.12f);
    Base = FMath::Lerp(Base, Snow, 0.12f * YarlungTerrain::Smooth01((Height01 - 0.992f) / 0.018f));
    const float RockBreakup = 0.5f + 0.5f * FMath::Sin(X * 0.0019f - Y * 0.0023f + Height * 0.0041f);
    const FLinearColor WetRock(0.095f + RockBreakup * 0.035f, 0.135f + RockBreakup * 0.045f, 0.130f + RockBreakup * 0.040f, 1.0f);
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(RockMask * 0.68f, 0.0f, 0.68f));
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

// Corridor mesh tessellation constants (shared by surface generation + triangle emission).
constexpr int32 RingsPerTrackSegment = 4;
constexpr int32 LanesPerSide = 72;
constexpr int32 LaneCount = LanesPerSide * 2 + 1;
constexpr float AlongStepCm = 650.0f;
constexpr float HalfWidthCm = 125000.0f;
constexpr float TrackTerrainVoidHalfWidthCm = 68000.0f;

struct FCorridorMeshStats
{
    int32 DisplacedVertexCount = 0;
    float MaxAbsDisplacementCm = 0.0f;
    float MaxAbsAuthoredDeltaCm = 0.0f;
};

// Phase 1: authored profile + relief-displaced corridor surface positions and UVs.
void ComputeCorridorPositions(
    const TArray<uint16>& HeightData,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    TArray<FVector>& Positions,
    TArray<float>& Us,
    TArray<float>& Vs,
    FCorridorMeshStats& Stats)
{
    const int32 RingCount = TrackPoints.Num() * RingsPerTrackSegment;

    const auto SampleTrackPosition = [&TrackPoints](int32 RingIndex) -> FVector2D
    {
        const int32 SegmentIndex = (RingIndex / RingsPerTrackSegment) % TrackPoints.Num();
        const int32 NextIndex = (SegmentIndex + 1) % TrackPoints.Num();
        const float T = static_cast<float>(RingIndex % RingsPerTrackSegment) / static_cast<float>(RingsPerTrackSegment);
        return FMath::Lerp(TrackPoints[SegmentIndex].Position, TrackPoints[NextIndex].Position, T);
    };

    for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
    {
        const FVector2D Previous = SampleTrackPosition((RingIndex + RingCount - 1) % RingCount);
        const FVector2D Center = SampleTrackPosition(RingIndex);
        const FVector2D Next = SampleTrackPosition((RingIndex + 1) % RingCount);
        const FVector2D Tangent = (Next - Previous).GetSafeNormal();
        const FVector2D Right(Tangent.Y, -Tangent.X);
        const float TrackBaseHeight = YarlungHeightAtWorldXY(HeightData, Center.X, Center.Y);

        for (int32 LaneIndex = 0; LaneIndex < LaneCount; ++LaneIndex)
        {
            const float LaneT = static_cast<float>(LaneIndex - LanesPerSide) / static_cast<float>(LanesPerSide);
            const float SignedOffsetCm = LaneT * HalfWidthCm;
            const FVector2D Position2D = Center + Right * SignedOffsetCm;
            const float BaseHeight = YarlungHeightAtWorldXY(HeightData, Position2D.X, Position2D.Y);
            const FVector BaseNormal = YarlungSmoothNormalAtWorldXY(HeightData, Position2D.X, Position2D.Y);
            const float TrackDistance = FMath::Abs(SignedOffsetCm);
            const float ViewCorridorMask = YarlungViewCorridor::ComputeMask(TrackPoints, Position2D);
            const float DisplacementCm = YarlungTerrainRelief::ComputeReliefCm(
                Position2D,
                BaseHeight,
                BaseNormal,
                TrackDistance,
                ViewCorridorMask);
            const float LandformCm = YarlungCorridorLandformDeltaCm(Center, SignedOffsetCm, BaseHeight);
            const float AuthoredProfileHeight = YarlungCorridorProfile::AuthoredHeightCm(Center, SignedOffsetCm, TrackBaseHeight, BaseHeight);
            const float ClosestTrackDistance = YarlungViewCorridor::DistanceToTrackCm(TrackPoints, Position2D);
            const float OverlapSuppression = YarlungTerrain::Smooth01((TrackDistance - ClosestTrackDistance - 3000.0f) / 9000.0f);
            const float AuthoredBaseHeight = FMath::Lerp(AuthoredProfileHeight, BaseHeight, OverlapSuppression);
            const float AuthoredDeltaCm = AuthoredBaseHeight - BaseHeight;

            const float NearTrackBlend = YarlungCorridorProfile::NearTrackBlend(FMath::Abs(SignedOffsetCm));
            const float RideEnvelopeHeight = YarlungCorridorProfile::RideEnvelopeHeightCm(TrackBaseHeight);
            const float Height = FMath::Lerp(AuthoredBaseHeight + DisplacementCm + LandformCm, RideEnvelopeHeight, NearTrackBlend);
            const int32 VertexIndex = RingIndex * LaneCount + LaneIndex;
            Positions[VertexIndex] = FVector(Position2D.X, Position2D.Y, Height + 25.0f);
            Us[VertexIndex] = static_cast<float>(RingIndex) * AlongStepCm / 120000.0f;
            Vs[VertexIndex] = (LaneT + 1.0f) * 0.5f;
            if (FMath::Abs(DisplacementCm) > 1.0f)
            {
                ++Stats.DisplacedVertexCount;
                Stats.MaxAbsDisplacementCm = FMath::Max(Stats.MaxAbsDisplacementCm, FMath::Abs(DisplacementCm));
            }
            Stats.MaxAbsAuthoredDeltaCm = FMath::Max(Stats.MaxAbsAuthoredDeltaCm, FMath::Abs(AuthoredDeltaCm));
        }
    }
}

// Phase 2: central-difference vertex normals + per-vertex landform colour.
void ComputeCorridorNormalsAndColors(
    const TArray<uint16>& HeightData,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const TArray<FVector>& Positions,
    TArray<FVector>& Normals,
    TArray<FLinearColor>& Colors)
{
    const int32 RingCount = TrackPoints.Num() * RingsPerTrackSegment;

    const auto PositionAt = [&Positions, RingCount](int32 RingIndex, int32 LaneIndex) -> const FVector&
    {
        const int32 ClampedRing = (RingIndex + RingCount) % RingCount;
        const int32 ClampedLane = FMath::Clamp(LaneIndex, 0, LaneCount - 1);
        return Positions[ClampedRing * LaneCount + ClampedLane];
    };

    for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
    {
        for (int32 LaneIndex = 0; LaneIndex < LaneCount; ++LaneIndex)
        {
            const int32 VertexIndex = RingIndex * LaneCount + LaneIndex;
            const FVector Along = PositionAt(RingIndex + 1, LaneIndex) - PositionAt(RingIndex - 1, LaneIndex);
            const FVector Across = PositionAt(RingIndex, LaneIndex + 1) - PositionAt(RingIndex, LaneIndex - 1);
            FVector Normal = FVector::CrossProduct(Across, Along).GetSafeNormal();
            if (Normal.Z < 0.0f)
            {
                Normal *= -1.0f;
            }
            Normals[VertexIndex] = Normal.IsNearlyZero() ? FVector::UpVector : Normal;
            const FVector& Position = Positions[VertexIndex];
            const FVector BaseNormal = YarlungSmoothNormalAtWorldXY(HeightData, Position.X, Position.Y);
            const float RockMask = YarlungCorridorRockMask(TrackPoints, Position.X, Position.Y, Position.Z, BaseNormal);
            Colors[VertexIndex] = YarlungColorAtPosition(Position.X, Position.Y, Position.Z, RockMask);
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
        Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
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

    const int32 RingCount = TrackPoints.Num() * RingsPerTrackSegment;
    const int32 VertexCount = RingCount * LaneCount;

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

    FCorridorMeshStats Stats;
    ComputeCorridorPositions(HeightData, TrackPoints, Positions, Us, Vs, Stats);
    ComputeCorridorNormalsAndColors(HeightData, TrackPoints, Positions, Normals, Colors);

    FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("YarlungCorridorTerrain"));
    TArray<FVertexID> Vertices;
    Vertices.Reserve(VertexCount);
    for (const FVector& Position : Positions)
    {
        Vertices.Add(Builder.AppendVertex(Position));
    }

    const auto LaneOffsetCm = [](int32 LaneIndex) -> float
    {
        const float LaneT = static_cast<float>(LaneIndex - LanesPerSide) / static_cast<float>(LanesPerSide);
        return LaneT * HalfWidthCm;
    };

    const auto ShouldEmitTerrainQuad = [&LaneOffsetCm, &Positions, &TrackPoints](int32 RingIndex, int32 NextRing, int32 LaneIndex) -> bool
    {
        const float QuadMidOffsetCm = 0.5f * (LaneOffsetCm(LaneIndex) + LaneOffsetCm(LaneIndex + 1));
        if (FMath::Abs(QuadMidOffsetCm) < TrackTerrainVoidHalfWidthCm)
        {
            return false;
        }

        const FVector& P00 = Positions[RingIndex * LaneCount + LaneIndex];
        const FVector& P01 = Positions[RingIndex * LaneCount + LaneIndex + 1];
        const FVector& P10 = Positions[NextRing * LaneCount + LaneIndex];
        const FVector& P11 = Positions[NextRing * LaneCount + LaneIndex + 1];
        const FVector2D QuadCenter(
            (P00.X + P01.X + P10.X + P11.X) * 0.25f,
            (P00.Y + P01.Y + P10.Y + P11.Y) * 0.25f);
        const float ClosestTrackDistanceCm = YarlungViewCorridor::DistanceToTrackCm(TrackPoints, QuadCenter);
        return ClosestTrackDistanceCm >= TrackTerrainVoidHalfWidthCm;
    };

    const auto AppendCorridorTriangle = [&](int32 Ring0, int32 Lane0, int32 Ring1, int32 Lane1, int32 Ring2, int32 Lane2)
    {
        const int32 I0 = Ring0 * LaneCount + Lane0;
        const int32 I1 = Ring1 * LaneCount + Lane1;
        const int32 I2 = Ring2 * LaneCount + Lane2;
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

    int32 SkippedNearTrackQuadCount = 0;
    for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
    {
        const int32 NextRing = (RingIndex + 1) % RingCount;
        for (int32 LaneIndex = 0; LaneIndex < LaneCount - 1; ++LaneIndex)
        {
            if (!ShouldEmitTerrainQuad(RingIndex, NextRing, LaneIndex))
            {
                ++SkippedNearTrackQuadCount;
                continue;
            }
            AppendCorridorTriangle(RingIndex, LaneIndex, NextRing, LaneIndex, NextRing, LaneIndex + 1);
            AppendCorridorTriangle(RingIndex, LaneIndex, NextRing, LaneIndex + 1, RingIndex, LaneIndex + 1);
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

    if (!UEditorLoadingAndSavingUtils::SavePackages({ MeshPackage }, false))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to save Yarlung corridor terrain asset: %s"), YarlungCorridorTerrainMeshPackagePath);
        return nullptr;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Nanite Yarlung corridor terrain: %s vertices=%d triangles=%d skipped_near_track_quads=%d displaced_vertices=%d max_displacement_cm=%.1f max_authored_delta_cm=%.1f nanite=%s"),
        *StaticMesh->GetPathName(),
        VertexCount,
        (RingCount * (LaneCount - 1) - SkippedNearTrackQuadCount) * 2,
        SkippedNearTrackQuadCount,
        Stats.DisplacedVertexCount,
        Stats.MaxAbsDisplacementCm,
        Stats.MaxAbsAuthoredDeltaCm,
        StaticMesh->IsNaniteEnabled() ? TEXT("true") : TEXT("false"));
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

void SpawnYarlungWorldActors(UWorld* World, UStaticMesh* CorridorTerrainAsset)
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

    if (AActor* Terrain = Spawn(AYarlungMeshTerrainActor::StaticClass(), TEXT("YarlungCorridorTerrainScenery")))
    {
        if (UStaticMeshComponent* MeshComponent = Terrain->FindComponentByClass<UStaticMeshComponent>())
        {
            MeshComponent->SetStaticMesh(CorridorTerrainAsset);
        }
    }

    Spawn(ACoasterRideActor::StaticClass(), TEXT("YarlungCoasterRide"));
    Spawn(AYarlungRiverActor::StaticClass(), TEXT("YarlungRiverScenery"));
    Spawn(AYarlungSceneryActor::StaticClass(), TEXT("YarlungForestRockScenery"));
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

    TArray<uint16> HeightData;
    if (!LoadYarlungHeightmap(HeightData))
    {
        return 1;
    }

    UStaticMesh* CorridorTerrainAsset = BuildYarlungCorridorTerrainStaticMesh(HeightData);
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

    SpawnYarlungWorldActors(World, CorridorTerrainAsset);

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
