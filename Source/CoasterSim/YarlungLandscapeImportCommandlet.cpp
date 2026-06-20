#include "YarlungLandscapeImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungRiverActor.h"
#include "YarlungSceneryActor.h"
#include "YarlungMeshTerrainActor.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainRelief.h"
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
constexpr int32 YarlungHeightmapSize = 1009;
constexpr float YarlungMinX = -337778.431f;
constexpr float YarlungMaxX = 337778.431f;
constexpr float YarlungMinY = -416981.551f;
constexpr float YarlungMaxY = 416981.551f;
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
    const int32 ClampedX = FMath::Clamp(XIndex, 0, YarlungHeightmapSize - 1);
    const int32 ClampedY = FMath::Clamp(YIndex, 0, YarlungHeightmapSize - 1);
    return YarlungTerrain::HeightValueToCm(HeightData[ClampedY * YarlungHeightmapSize + ClampedX]);
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
    const float U = FMath::Clamp((X - YarlungMinX) / (YarlungMaxX - YarlungMinX), 0.0f, 1.0f);
    const float V = FMath::Clamp((Y - YarlungMinY) / (YarlungMaxY - YarlungMinY), 0.0f, 1.0f);
    return YarlungHeightAtSourceGrid(
        HeightData,
        U * static_cast<float>(YarlungHeightmapSize - 1),
        V * static_cast<float>(YarlungHeightmapSize - 1));
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

FLinearColor YarlungColorAtPosition(float X, float Y, float Height, float RockMask)
{
    const float Height01 = YarlungTerrain::NormalizeEncodedHeightCm(Height);
    const float RiverDistance = FMath::Abs(Y - YarlungTerrain::RiverCenterY(X));
    const float River = FMath::Clamp(1.0f - RiverDistance / 26000.0f, 0.0f, 1.0f);
    const float Forest = YarlungTerrain::Smooth01((RiverDistance - 26000.0f) / 90000.0f) * (1.0f - YarlungTerrain::Smooth01((Height01 - 0.43f) / 0.20f));
    const float Noise = YarlungValueNoise(X, Y);

    FLinearColor Base(0.24f + Noise * 0.04f, 0.36f + Noise * 0.08f, 0.29f + Noise * 0.04f, 1.0f);
    const FLinearColor Rock(0.36f + Height01 * 0.18f, 0.43f + Height01 * 0.14f, 0.40f + Height01 * 0.10f, 1.0f);
    const FLinearColor ForestColor(0.06f + Noise * 0.05f, 0.25f + Noise * 0.14f, 0.11f + Noise * 0.05f, 1.0f);
    const FLinearColor RiverColor(0.50f, 0.78f, 0.74f, 1.0f);
    const FLinearColor Snow(0.72f, 0.78f, 0.78f, 1.0f);

    Base = FMath::Lerp(Base, Rock, FMath::Clamp(Height01 * 0.95f, 0.0f, 0.65f));
    Base = FMath::Lerp(Base, ForestColor, FMath::Clamp(Forest, 0.0f, 0.82f));
    Base = FMath::Lerp(Base, RiverColor, River * 0.72f);
    Base = FMath::Lerp(Base, Snow, YarlungTerrain::Smooth01((Height01 - 0.88f) / 0.08f));
    const float RockBreakup = 0.5f + 0.5f * FMath::Sin(X * 0.0019f - Y * 0.0023f + Height * 0.0041f);
    const FLinearColor WetRock(0.22f + RockBreakup * 0.06f, 0.29f + RockBreakup * 0.06f, 0.27f + RockBreakup * 0.05f, 1.0f);
    Base = FMath::Lerp(Base, WetRock, FMath::Clamp(RockMask * 0.72f, 0.0f, 0.72f));
    return Base;
}

bool LoadYarlungTerrainTrackPoints(TArray<YarlungViewCorridor::FTrackPoint>& OutTrackPoints)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("Generated/YarlungLandscape/YarlungTrack.csv");
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read generated Yarlung track for terrain displacement: %s"), *Path);
        return false;
    }

    OutTrackPoints.Reset();
    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        TArray<FString> Columns;
        Lines[LineIndex].ParseIntoArray(Columns, TEXT(","), true);
        if (Columns.Num() < 7)
        {
            continue;
        }

        YarlungViewCorridor::FTrackPoint Point;
        Point.Position = FVector2D(FCString::Atof(*Columns[1]), FCString::Atof(*Columns[2]));
        OutTrackPoints.Add(Point);
    }

    if (OutTrackPoints.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few generated track points for terrain displacement: %d"), OutTrackPoints.Num());
        return false;
    }

    return true;
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

    constexpr int32 RingsPerTrackSegment = 4;
    constexpr int32 LanesPerSide = 72;
    constexpr int32 LaneCount = LanesPerSide * 2 + 1;
    constexpr float AlongStepCm = 650.0f;
    constexpr float HalfWidthCm = 90000.0f;
    constexpr float InnerFlattenHalfWidthCm = 4200.0f;
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

    const auto SampleTrackPosition = [&TrackPoints](int32 RingIndex) -> FVector2D
    {
        const int32 SegmentIndex = (RingIndex / RingsPerTrackSegment) % TrackPoints.Num();
        const int32 NextIndex = (SegmentIndex + 1) % TrackPoints.Num();
        const float T = static_cast<float>(RingIndex % RingsPerTrackSegment) / static_cast<float>(RingsPerTrackSegment);
        return FMath::Lerp(TrackPoints[SegmentIndex].Position, TrackPoints[NextIndex].Position, T);
    };

    int32 DisplacedVertexCount = 0;
    float MaxAbsDisplacementCm = 0.0f;
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
            float DisplacementCm = YarlungTerrainRelief::ComputeReliefCm(
                Position2D,
                BaseHeight,
                BaseNormal,
                TrackDistance,
                ViewCorridorMask);

            const float NearTrackBlend = 1.0f - YarlungTerrain::Smooth01((FMath::Abs(SignedOffsetCm) - InnerFlattenHalfWidthCm) / 9000.0f);
            const float Height = FMath::Lerp(BaseHeight + DisplacementCm, TrackBaseHeight + 650.0f, NearTrackBlend);
            const int32 VertexIndex = RingIndex * LaneCount + LaneIndex;
            Positions[VertexIndex] = FVector(Position2D.X, Position2D.Y, Height + 25.0f);
            Us[VertexIndex] = static_cast<float>(RingIndex) * AlongStepCm / 120000.0f;
            Vs[VertexIndex] = (LaneT + 1.0f) * 0.5f;
            if (FMath::Abs(DisplacementCm) > 1.0f)
            {
                ++DisplacedVertexCount;
                MaxAbsDisplacementCm = FMath::Max(MaxAbsDisplacementCm, FMath::Abs(DisplacementCm));
            }
        }
    }

    const auto PositionAt = [&Positions, RingCount, LaneCount](int32 RingIndex, int32 LaneIndex) -> const FVector&
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

    FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("YarlungCorridorTerrain"));
    TArray<FVertexID> Vertices;
    Vertices.Reserve(VertexCount);
    for (const FVector& Position : Positions)
    {
        Vertices.Add(Builder.AppendVertex(Position));
    }

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

    for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
    {
        const int32 NextRing = (RingIndex + 1) % RingCount;
        for (int32 LaneIndex = 0; LaneIndex < LaneCount - 1; ++LaneIndex)
        {
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
        TEXT("Built Nanite Yarlung corridor terrain: %s vertices=%d triangles=%d displaced_vertices=%d max_displacement_cm=%.1f nanite=%s"),
        *StaticMesh->GetPathName(),
        VertexCount,
        RingCount * (LaneCount - 1) * 2,
        DisplacedVertexCount,
        MaxAbsDisplacementCm,
        StaticMesh->IsNaniteEnabled() ? TEXT("true") : TEXT("false"));
    return StaticMesh;
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
    constexpr int32 HeightmapSize = 1009;

    const FString HeightmapPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectContentDir(),
        TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16")));
    const FString MapPackagePath = TEXT("/Game/Generated/YarlungLandscape/YarlungLandscape_Level");

    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *HeightmapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read landscape heightmap: %s"), *HeightmapPath);
        return 1;
    }

    const int32 ExpectedByteCount = HeightmapSize * HeightmapSize * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Heightmap has %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return 1;
    }

    TArray<uint16> HeightData;
    HeightData.SetNumUninitialized(HeightmapSize * HeightmapSize);
    FMemory::Memcpy(HeightData.GetData(), RawBytes.GetData(), RawBytes.Num());

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

    AYarlungMeshTerrainActor* CorridorTerrain = World->SpawnActor<AYarlungMeshTerrainActor>(
        AYarlungMeshTerrainActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (CorridorTerrain)
    {
        CorridorTerrain->SetActorLabel(TEXT("YarlungCorridorTerrainScenery"));
        if (UStaticMeshComponent* MeshComponent = CorridorTerrain->FindComponentByClass<UStaticMeshComponent>())
        {
            MeshComponent->SetStaticMesh(CorridorTerrainAsset);
        }
    }

    ACoasterRideActor* Ride = World->SpawnActor<ACoasterRideActor>(
        ACoasterRideActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (Ride)
    {
        Ride->SetActorLabel(TEXT("YarlungCoasterRide"));
    }

    AYarlungRiverActor* River = World->SpawnActor<AYarlungRiverActor>(
        AYarlungRiverActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (River)
    {
        River->SetActorLabel(TEXT("YarlungRiverScenery"));
    }

    AYarlungSceneryActor* Scenery = World->SpawnActor<AYarlungSceneryActor>(
        AYarlungSceneryActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (Scenery)
    {
        Scenery->SetActorLabel(TEXT("YarlungForestRockScenery"));
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
