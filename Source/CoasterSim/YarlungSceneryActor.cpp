#include "YarlungSceneryActor.h"

#include "YarlungAssetConfig.h"
#include "YarlungGeneratedPaths.h"
#include "YarlungRiverField.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainSurface.h"
#include "YarlungTrackCsv.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

namespace
{
// Source height grid dimensions, world bounds, and height encoding all come from
// YarlungTerrain::Config() (Config/yarlung-terrain.json) — the same source the
// Python pipeline and the import commandlet use.

float Hash01(float X, float Y)
{
    const float Value = FMath::Sin(X * 12.9898f + Y * 78.233f) * 43758.5453f;
    return FMath::Frac(Value);
}

FVector HorizontalForward(const TArray<FYarlungSceneryTrackSample>& Samples, int32 Index)
{
    const int32 Prev = FMath::Max(0, Index - 1);
    const int32 Next = FMath::Min(Samples.Num() - 1, Index + 1);
    FVector Forward = Samples[Next].Position - Samples[Prev].Position;
    Forward.Z = 0.0f;
    return Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal();
}

FVector RightFromForward(const FVector& Forward)
{
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    return Right.IsNearlyZero() ? FVector::RightVector : Right;
}

FQuat SurfaceAlignedRotation(const FVector& Normal, float YawDegrees)
{
    const FQuat SurfaceTilt = FQuat::FindBetweenNormals(FVector::UpVector, Normal.GetSafeNormal());
    const FQuat Yaw = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
    return Yaw * SurfaceTilt;
}

float ScaledHorizontalRadiusCm(const UStaticMesh& Mesh, const FVector& Scale)
{
    const FVector Extent = Mesh.GetBounds().BoxExtent;
    return FMath::Max(Extent.X * FMath::Abs(Scale.X), Extent.Y * FMath::Abs(Scale.Y));
}

float YawFacingDirection(const FVector2D& Direction)
{
    const FVector2D SafeDirection = Direction.IsNearlyZero() ? FVector2D(1.0f, 0.0f) : Direction.GetSafeNormal();
    return FMath::RadiansToDegrees(FMath::Atan2(SafeDirection.Y, SafeDirection.X));
}

bool ProjectOntoTrackCorridor(
    const TArray<FYarlungSceneryTrackSample>& Samples,
    const FVector2D& Location,
    FVector& OutCenter,
    float& OutSignedOffsetCm)
{
    if (Samples.Num() < 2)
    {
        return false;
    }

    float BestDistanceSquared = TNumericLimits<float>::Max();
    FVector BestCenter = Samples[0].Position;
    FVector2D BestRight(0.0f, 1.0f);

    for (int32 Index = 0; Index + 1 < Samples.Num(); ++Index)
    {
        const FVector& A3 = Samples[Index].Position;
        const FVector& B3 = Samples[Index + 1].Position;
        const FVector2D A(A3.X, A3.Y);
        const FVector2D B(B3.X, B3.Y);
        const FVector2D Segment = B - A;
        const float SegmentLengthSquared = Segment.SizeSquared();
        if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float T = FMath::Clamp(FVector2D::DotProduct(Location - A, Segment) / SegmentLengthSquared, 0.0f, 1.0f);
        const FVector2D Closest = A + Segment * T;
        const float DistanceSquared = FVector2D::DistSquared(Location, Closest);
        if (DistanceSquared < BestDistanceSquared)
        {
            const FVector2D Tangent = Segment.GetSafeNormal();
            BestDistanceSquared = DistanceSquared;
            BestCenter = FMath::Lerp(A3, B3, T);
            BestRight = FVector2D(-Tangent.Y, Tangent.X);
        }
    }

    if (BestDistanceSquared >= TNumericLimits<float>::Max())
    {
        return false;
    }

    OutCenter = BestCenter;
    OutSignedOffsetCm = FVector2D::DotProduct(Location - FVector2D(BestCenter.X, BestCenter.Y), BestRight);
    return true;
}

}

AYarlungSceneryActor::AYarlungSceneryActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    RockOutcrops = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RockOutcrops"));
    RockOutcrops->SetupAttachment(SceneRoot);
    RockOutcrops->SetCastShadow(true);
    RockOutcrops->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RiverbankBoulders = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RiverbankBoulders"));
    RiverbankBoulders->SetupAttachment(SceneRoot);
    RiverbankBoulders->SetCastShadow(true);
    RiverbankBoulders->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ScreeBoulders = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ScreeBoulders"));
    ScreeBoulders->SetupAttachment(SceneRoot);
    ScreeBoulders->SetCastShadow(true);
    ScreeBoulders->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UnderstoryClumps = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("UnderstoryClumps"));
    UnderstoryClumps->SetupAttachment(SceneRoot);
    UnderstoryClumps->SetCastShadow(false);
    UnderstoryClumps->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CliffRockFacesA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CliffRockFacesA"));
    CliffRockFacesA->SetupAttachment(SceneRoot);
    CliffRockFacesA->SetCastShadow(true);
    CliffRockFacesA->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CliffRockFacesB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CliffRockFacesB"));
    CliffRockFacesB->SetupAttachment(SceneRoot);
    CliffRockFacesB->SetCastShadow(true);
    CliffRockFacesB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CliffRockFacesC = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CliffRockFacesC"));
    CliffRockFacesC->SetupAttachment(SceneRoot);
    CliffRockFacesC->SetCastShadow(true);
    CliffRockFacesC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CliffRockFacesD = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CliffRockFacesD"));
    CliffRockFacesD->SetupAttachment(SceneRoot);
    CliffRockFacesD->SetCastShadow(true);
    CliffRockFacesD->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CliffRockFacesE = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CliffRockFacesE"));
    CliffRockFacesE->SetupAttachment(SceneRoot);
    CliffRockFacesE->SetCastShadow(true);
    CliffRockFacesE->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CliffRockFacesF = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CliffRockFacesF"));
    CliffRockFacesF->SetupAttachment(SceneRoot);
    CliffRockFacesF->SetCastShadow(true);
    CliffRockFacesF->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SlopeRockWallA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SlopeRockWallA"));
    SlopeRockWallA->SetupAttachment(SceneRoot);
    SlopeRockWallA->SetCastShadow(true);
    SlopeRockWallA->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    SlopeRockWallB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SlopeRockWallB"));
    SlopeRockWallB->SetupAttachment(SceneRoot);
    SlopeRockWallB->SetCastShadow(true);
    SlopeRockWallB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ForestShrubsA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ForestShrubsA"));
    ForestShrubsA->SetupAttachment(SceneRoot);
    ForestShrubsA->SetCastShadow(false);
    ForestShrubsA->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ForestShrubsB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ForestShrubsB"));
    ForestShrubsB->SetupAttachment(SceneRoot);
    ForestShrubsB->SetCastShadow(false);
    ForestShrubsB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CanopyTreesA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanopyTreesA"));
    CanopyTreesA->SetupAttachment(SceneRoot);
    CanopyTreesA->SetCastShadow(true);
    CanopyTreesA->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CanopyTreesB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanopyTreesB"));
    CanopyTreesB->SetupAttachment(SceneRoot);
    CanopyTreesB->SetCastShadow(true);
    CanopyTreesB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CanopyTreesC = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CanopyTreesC"));
    CanopyTreesC->SetupAttachment(SceneRoot);
    CanopyTreesC->SetCastShadow(true);
    CanopyTreesC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void AYarlungSceneryActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildScenery();
}

void AYarlungSceneryActor::BeginPlay()
{
    Super::BeginPlay();
    RebuildScenery();

    if (FParse::Param(FCommandLine::Get(), TEXT("YarlungHideScenery")))
    {
        SetActorHiddenInGame(true);
        RockOutcrops->SetVisibility(false, true);
        RiverbankBoulders->SetVisibility(false, true);
        ScreeBoulders->SetVisibility(false, true);
        UnderstoryClumps->SetVisibility(false, true);
        CliffRockFacesA->SetVisibility(false, true);
        CliffRockFacesB->SetVisibility(false, true);
        CliffRockFacesC->SetVisibility(false, true);
        CliffRockFacesD->SetVisibility(false, true);
        CliffRockFacesE->SetVisibility(false, true);
        CliffRockFacesF->SetVisibility(false, true);
        SlopeRockWallA->SetVisibility(false, true);
        SlopeRockWallB->SetVisibility(false, true);
        ForestShrubsA->SetVisibility(false, true);
        ForestShrubsB->SetVisibility(false, true);
        CanopyTreesA->SetVisibility(false, true);
        CanopyTreesB->SetVisibility(false, true);
        CanopyTreesC->SetVisibility(false, true);
    }
}

void AYarlungSceneryActor::RebuildScenery()
{
    const FYarlungAssetConfig& AssetConfig = YarlungAssets::Config();
    ConfigureMeshesFromAssets(AssetConfig);

    TArray<FYarlungSceneryTrackSample> TrackSamples;
    TArray<uint16> EncodedHeights;
    if (!LoadSceneryTrack(TrackSamples) || !LoadCorridorSourceHeights(EncodedHeights))
    {
        UE_LOG(LogTemp, Fatal, TEXT("Yarlung scenery requires generated track and corridor source height inputs."));
    }

    BuildScatter(TrackSamples, EncodedHeights, AssetConfig);
    ApplyMaterials(AssetConfig);
}

void AYarlungSceneryActor::ConfigureMeshesFromAssets(const FYarlungAssetConfig& AssetConfig)
{
    for (const FYarlungSceneryComponentConfig& ComponentConfig : AssetConfig.SceneryComponents)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = ComponentByName(ComponentConfig.Name);
        if (!Component)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Yarlung scenery asset config references unknown component: %s"), *ComponentConfig.Name);
        }

        if (ComponentConfig.MeshPath.IsEmpty())
        {
            Component->SetStaticMesh(nullptr);
            continue;
        }

        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ComponentConfig.MeshPath);
        if (!Mesh)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Unable to load Yarlung scenery mesh for %s: %s"), *ComponentConfig.Name, *ComponentConfig.MeshPath);
        }
        Component->SetStaticMesh(Mesh);
    }
}

UHierarchicalInstancedStaticMeshComponent* AYarlungSceneryActor::ComponentByName(const FString& Name) const
{
    if (Name == TEXT("RockOutcrops")) { return RockOutcrops; }
    if (Name == TEXT("RiverbankBoulders")) { return RiverbankBoulders; }
    if (Name == TEXT("ScreeBoulders")) { return ScreeBoulders; }
    if (Name == TEXT("UnderstoryClumps")) { return UnderstoryClumps; }
    if (Name == TEXT("CliffRockFacesA")) { return CliffRockFacesA; }
    if (Name == TEXT("CliffRockFacesB")) { return CliffRockFacesB; }
    if (Name == TEXT("CliffRockFacesC")) { return CliffRockFacesC; }
    if (Name == TEXT("CliffRockFacesD")) { return CliffRockFacesD; }
    if (Name == TEXT("CliffRockFacesE")) { return CliffRockFacesE; }
    if (Name == TEXT("CliffRockFacesF")) { return CliffRockFacesF; }
    if (Name == TEXT("SlopeRockWallA")) { return SlopeRockWallA; }
    if (Name == TEXT("SlopeRockWallB")) { return SlopeRockWallB; }
    if (Name == TEXT("ForestShrubsA")) { return ForestShrubsA; }
    if (Name == TEXT("ForestShrubsB")) { return ForestShrubsB; }
    if (Name == TEXT("CanopyTreesA")) { return CanopyTreesA; }
    if (Name == TEXT("CanopyTreesB")) { return CanopyTreesB; }
    if (Name == TEXT("CanopyTreesC")) { return CanopyTreesC; }
    return nullptr;
}

void AYarlungSceneryActor::ClearAllInstances()
{
    RockOutcrops->ClearInstances();
    RiverbankBoulders->ClearInstances();
    ScreeBoulders->ClearInstances();
    UnderstoryClumps->ClearInstances();
    CliffRockFacesA->ClearInstances();
    CliffRockFacesB->ClearInstances();
    CliffRockFacesC->ClearInstances();
    CliffRockFacesD->ClearInstances();
    CliffRockFacesE->ClearInstances();
    CliffRockFacesF->ClearInstances();
    SlopeRockWallA->ClearInstances();
    SlopeRockWallB->ClearInstances();
    ForestShrubsA->ClearInstances();
    ForestShrubsB->ClearInstances();
    CanopyTreesA->ClearInstances();
    CanopyTreesB->ClearInstances();
    CanopyTreesC->ClearInstances();
}

bool AYarlungSceneryActor::LoadSceneryTrack(TArray<FYarlungSceneryTrackSample>& OutSamples) const
{
    const FString Path = FPaths::ProjectContentDir() / TrackCsvRelativePath;
    TArray<FYarlungTrackRow> Rows;
    FString Error;
    if (!YarlungTrackCsv::Load(Path, Rows, &Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung scenery track CSV: %s"), *Error);
        return false;
    }

    OutSamples.Reset();
    for (const FYarlungTrackRow& Row : Rows)
    {
        FYarlungSceneryTrackSample Sample;
        Sample.Position = Row.PositionCm;
        Sample.Section = Row.Section;
        OutSamples.Add(Sample);
    }

    if (OutSamples.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few ride-corridor samples for Yarlung scenery scatter: %d"), OutSamples.Num());
        return false;
    }

    return true;
}

bool AYarlungSceneryActor::LoadCorridorSourceHeights(TArray<uint16>& OutEncodedHeights) const
{
    const FString Path = FPaths::ProjectContentDir() / HeightmapRelativePath;
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung corridor source heights for scenery scatter: %s"), *Path);
        return false;
    }

    const int32 HeightmapSize = YarlungTerrain::Config().GridSize;
    const int32 ExpectedByteCount = HeightmapSize * HeightmapSize * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung scenery source heights have %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return false;
    }

    OutEncodedHeights.SetNumUninitialized(HeightmapSize * HeightmapSize);
    FMemory::Memcpy(OutEncodedHeights.GetData(), RawBytes.GetData(), RawBytes.Num());
    return true;
}

void AYarlungSceneryActor::BuildScatter(
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<uint16>& EncodedHeights,
    const FYarlungAssetConfig& AssetConfig)
{
    ClearAllInstances();

    FYarlungRiverField RiverField;
    FString RiverLoadError;
    if (!RiverField.LoadGeneratedCsv(&RiverLoadError))
    {
        UE_LOG(LogTemp, Fatal, TEXT("Yarlung scenery requires generated river field: %s"), *RiverLoadError);
    }

    TArray<YarlungViewCorridor::FTrackPoint> TerrainTrackPoints;
    TerrainTrackPoints.Reserve(TrackSamples.Num());
    for (const FYarlungSceneryTrackSample& Sample : TrackSamples)
    {
        YarlungViewCorridor::FTrackPoint Point;
        Point.Position = FVector2D(Sample.Position.X, Sample.Position.Y);
        TerrainTrackPoints.Add(Point);
    }

    for (const FYarlungSceneryComponentConfig& ComponentConfig : AssetConfig.SceneryComponents)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = ComponentByName(ComponentConfig.Name);
        const FYarlungScatterKindConfig* KindConfig = AssetConfig.ScatterKinds.Find(ComponentConfig.Kind);
        if (!Component)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Yarlung scenery build has no component named %s"), *ComponentConfig.Name);
        }
        if (!KindConfig)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Yarlung scenery component %s references unknown kind %s"), *ComponentConfig.Name, *ComponentConfig.Kind);
        }

        switch (ComponentConfig.Placement)
        {
        case EYarlungSceneryPlacement::Scatter:
            AddScatterRule(Component, ComponentConfig, *KindConfig, TrackSamples, TerrainTrackPoints, EncodedHeights, RiverField);
            break;
        case EYarlungSceneryPlacement::CliffBelt:
            AddCliffBelt(Component, AssetConfig.CliffBelt, ComponentConfig.Seed, TrackSamples, TerrainTrackPoints, EncodedHeights, RiverField);
            break;
        case EYarlungSceneryPlacement::SurfaceCover:
        {
            const FYarlungSurfaceCoverProfileConfig* Profile = AssetConfig.SurfaceCoverProfiles.Find(ComponentConfig.SurfaceCoverProfileName);
            if (!Profile)
            {
                UE_LOG(LogTemp, Fatal, TEXT("Surface cover component '%s' references missing profile: %s"), *ComponentConfig.Name, *ComponentConfig.SurfaceCoverProfileName);
            }
            AddSurfaceCoverLayer(Component, ComponentConfig, *Profile, TrackSamples, TerrainTrackPoints, EncodedHeights, RiverField);
            break;
        }
        case EYarlungSceneryPlacement::RockWallSource:
            break;
        }
    }

    AddRockWallSegments(AssetConfig, TrackSamples, TerrainTrackPoints, EncodedHeights, RiverField);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung scatter instances: rocks=%d riverbank=%d scree=%d understory=%d cliff_a=%d cliff_b=%d cliff_c=%d cliff_d=%d cliff_e=%d cliff_f=%d slope_rock_a=%d slope_rock_b=%d shrubs_a=%d shrubs_b=%d canopy_a=%d canopy_b=%d canopy_c=%d"),
        RockOutcrops->GetInstanceCount(),
        RiverbankBoulders->GetInstanceCount(),
        ScreeBoulders->GetInstanceCount(),
        UnderstoryClumps->GetInstanceCount(),
        CliffRockFacesA->GetInstanceCount(),
        CliffRockFacesB->GetInstanceCount(),
        CliffRockFacesC->GetInstanceCount(),
        CliffRockFacesD->GetInstanceCount(),
        CliffRockFacesE->GetInstanceCount(),
        CliffRockFacesF->GetInstanceCount(),
        SlopeRockWallA->GetInstanceCount(),
        SlopeRockWallB->GetInstanceCount(),
        ForestShrubsA->GetInstanceCount(),
        ForestShrubsB->GetInstanceCount(),
        CanopyTreesA->GetInstanceCount(),
        CanopyTreesB->GetInstanceCount(),
        CanopyTreesC->GetInstanceCount());
}

bool AYarlungSceneryActor::TryResolvePlacement(
    const TArray<uint16>& EncodedHeights,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const FYarlungRiverField& RiverField,
    const FVector& Location2D,
    float RiverClearanceCm,
    float MinHeightCm,
    float MaxHeightCm,
    float MinSlope,
    float MaxSlope,
    float& OutHeightCm,
    FVector& OutNormal) const
{
    const YarlungTerrain::FConfig& Bounds = YarlungTerrain::Config();
    if (Location2D.X < Bounds.MinXCm || Location2D.X > Bounds.MaxXCm || Location2D.Y < Bounds.MinYCm || Location2D.Y > Bounds.MaxYCm)
    {
        return false;
    }

    if (RiverField.DistanceCm(FVector2D(Location2D.X, Location2D.Y)) < RiverClearanceCm)
    {
        return false;
    }

    OutHeightCm = YarlungTerrainSurface::SurfaceZCm(
        EncodedHeights,
        TerrainTrackPoints,
        RiverField,
        FVector2D(Location2D.X, Location2D.Y));
    if (OutHeightCm < MinHeightCm || OutHeightCm > MaxHeightCm)
    {
        return false;
    }

    OutNormal = YarlungTerrainSurface::SurfaceNormal(
        EncodedHeights,
        TerrainTrackPoints,
        RiverField,
        FVector2D(Location2D.X, Location2D.Y));
    const float Slope = 1.0f - OutNormal.Z;
    return Slope >= MinSlope && Slope <= MaxSlope;
}

void AYarlungSceneryActor::AddScatterRule(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungSceneryComponentConfig& ComponentConfig,
    const FYarlungScatterKindConfig& KindConfig,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const TArray<uint16>& EncodedHeights,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    const bool bBoulder = ComponentConfig.Kind.Contains(TEXT("boulder"), ESearchCase::IgnoreCase);
    const bool bCliff = ComponentConfig.Kind == TEXT("cliff");
    const bool bCanopyTree = ComponentConfig.Kind == TEXT("canopy_tree");
    const float Seed = ComponentConfig.Seed;

    for (int32 Index = 0; Index < ComponentConfig.Count; ++Index)
    {
        const float AlongHash = bCliff
            ? FMath::Frac((Index + Hash01(Index * 3.171f + Seed, 23.0f) * 0.42f) / FMath::Max(1.0f, static_cast<float>(ComponentConfig.Count)))
            : Hash01(Index * 3.171f + Seed, bBoulder ? 17.0f : (bCanopyTree ? 27.0f : 29.0f));
        const int32 SampleIndex = FMath::Clamp(
            FMath::FloorToInt(AlongHash * static_cast<float>(TrackSamples.Num() - 1)),
            0,
            TrackSamples.Num() - 2);
        const FYarlungSceneryTrackSample& A = TrackSamples[SampleIndex];
        const FYarlungSceneryTrackSample& B = TrackSamples[SampleIndex + 1];
        const float LocalT = Hash01(Index * 8.311f + Seed, bBoulder ? 4.0f : 11.0f);
        const FVector Center = FMath::Lerp(A.Position, B.Position, LocalT);
        const FVector Forward = HorizontalForward(TrackSamples, SampleIndex);
        const FVector Right = RightFromForward(Forward);

        const float Side = bCliff
            ? (((Index + FMath::FloorToInt(Seed)) % 2) == 0 ? 1.0f : -1.0f)
            : (Hash01(Index * 5.991f + Seed, 3.0f) < KindConfig.SideBias ? 1.0f : -1.0f);
        const float DistanceHash = Hash01(Index * 1.731f + Seed, bBoulder ? 41.0f : (bCliff ? 47.0f : (bCanopyTree ? 49.0f : 53.0f)));
        const float CliffBand = bCliff ? (static_cast<float>((Index / 2) % 5) + 0.20f + 0.42f * DistanceHash) / 5.0f : 0.0f;
        const float DistanceT = bCliff
            ? FMath::Clamp(CliffBand, 0.0f, 1.0f)
            : FMath::Pow(DistanceHash, KindConfig.DistancePower);
        const float LateralCm = FMath::Lerp(KindConfig.MinLateralCm, KindConfig.MaxLateralCm, DistanceT);
        const float AlongJitterCm = bCliff
            ? FMath::Lerp(-1300.0f, 1300.0f, Hash01(Index * 2.117f + Seed, 71.0f))
            : FMath::Lerp(-2400.0f, 2400.0f, Hash01(Index * 2.117f + Seed, 71.0f));
        const FVector Location2D = Center + Forward * AlongJitterCm + Right * Side * LateralCm;

        float Height = 0.0f;
        FVector Normal = FVector::UpVector;
        if (!TryResolvePlacement(
            EncodedHeights,
            TerrainTrackPoints,
            RiverField,
            Location2D,
            KindConfig.RiverClearanceCm,
            KindConfig.MinHeightCm,
            KindConfig.MaxHeightCm,
            KindConfig.MinSlope,
            KindConfig.MaxSlope,
            Height,
            Normal))
        {
            continue;
        }

        const float Yaw = bCliff
            ? FMath::RadiansToDegrees(FMath::Atan2((-Side * Right).Y, (-Side * Right).X)) + FMath::Lerp(-24.0f, 24.0f, Hash01(Index * 3.1f + Seed, 8.0f))
            : Hash01(Index * 4.121f + Seed, 97.0f) * 360.0f;
        const float ScaleBase = FMath::Lerp(KindConfig.ScaleMin, KindConfig.ScaleMax, Hash01(Index * 6.617f + Seed, 13.0f));
        const FVector Scale = bBoulder
            ? FVector(ScaleBase * FMath::Lerp(0.7f, 1.8f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase, ScaleBase * FMath::Lerp(0.45f, 1.15f, Hash01(Index * 7.0f + Seed, 2.0f)))
            : (bCliff
                ? FVector(ScaleBase * FMath::Lerp(0.8f, 1.9f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.45f, 1.2f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.65f, 1.45f, Hash01(Index * 5.0f + Seed, 3.0f)))
                : (bCanopyTree
                    ? FVector(ScaleBase * FMath::Lerp(0.85f, 1.55f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.85f, 1.55f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.80f, 1.35f, Hash01(Index * 5.0f + Seed, 3.0f)))
                    : FVector(ScaleBase * FMath::Lerp(0.8f, 1.55f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.75f, 1.3f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.75f, 1.8f, Hash01(Index * 5.0f + Seed, 3.0f)))));
        const FVector Location(Location2D.X, Location2D.Y, Height + KindConfig.HeightOffsetCm);
        const FQuat Rotation = KindConfig.bAlignToSurface
            ? SurfaceAlignedRotation(Normal, Yaw)
            : FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
        Component->AddInstance(FTransform(Rotation, Location, Scale));
    }
}

void AYarlungSceneryActor::AddSurfaceCoverLayer(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungSceneryComponentConfig& ComponentConfig,
    const FYarlungSurfaceCoverProfileConfig& Profile,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const TArray<uint16>& EncodedHeights,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    const bool bScree = ComponentConfig.Kind.Contains(TEXT("scree"), ESearchCase::IgnoreCase);
    const bool bCanopy = ComponentConfig.Kind == TEXT("canopy_tree");
    const UStaticMesh& Mesh = *Component->GetStaticMesh();
    const float Seed = ComponentConfig.Seed;
    int32 PlacedCount = 0;

    const auto AddAtLocation = [&](
        int32 SampleIndex,
        int32 BandIndex,
        float Side,
        const FVector& Center,
        const FVector& Forward,
        const FVector& Right)
    {
        const float Keep = Hash01(SampleIndex * 1.517f + BandIndex * 5.913f + Seed, Side > 0.0f ? 29.0f : 31.0f);
        if (Keep > Profile.Occupancy)
        {
            return;
        }

        const float AlongJitterCm = FMath::Lerp(-Profile.AlongJitterCm, Profile.AlongJitterCm, Hash01(SampleIndex * 2.317f + BandIndex + Seed, 37.0f));
        const float LateralJitterCm = FMath::Lerp(-Profile.LateralJitterCm, Profile.LateralJitterCm, Hash01(SampleIndex * 3.719f + BandIndex + Seed, 41.0f));
        const float LateralCm = FMath::Max(0.0f, Profile.LateralBandsCm[BandIndex] + LateralJitterCm);
        const float SignedOffsetCm = Side * LateralCm;
        const FVector Location2D = Center + Forward * AlongJitterCm + Right * SignedOffsetCm;

        float Height = 0.0f;
        FVector Normal = FVector::UpVector;
        if (!TryResolvePlacement(
            EncodedHeights,
            TerrainTrackPoints,
            RiverField,
            Location2D,
            Profile.RiverClearanceCm,
            Profile.MinHeightCm,
            Profile.MaxHeightCm,
            Profile.MinSlope,
            Profile.MaxSlope,
            Height,
            Normal))
        {
            return;
        }

        float Yaw = Hash01(SampleIndex * 7.071f + BandIndex + Seed, 43.0f) * 360.0f;
        if (Profile.bFaceRiver)
        {
            const FYarlungRiverQuery RiverQuery = RiverField.QueryNearest(FVector2D(Location2D.X, Location2D.Y));
            const FVector2D FaceRiverDirection(
                RiverQuery.Row.PositionCm.X - Location2D.X,
                RiverQuery.Row.PositionCm.Y - Location2D.Y);
            if (!FaceRiverDirection.IsNearlyZero())
            {
                Yaw = YawFacingDirection(FaceRiverDirection);
            }
        }
        Yaw += FMath::Lerp(-Profile.YawJitterDegrees, Profile.YawJitterDegrees, Hash01(SampleIndex * 11.071f + BandIndex + Seed, 44.0f));

        const float ScaleBase = FMath::Lerp(Profile.ScaleMin, Profile.ScaleMax, Hash01(SampleIndex * 5.317f + BandIndex + Seed, 47.0f));
        const FVector Scale = bScree
            ? FVector(
                ScaleBase * FMath::Lerp(0.75f, 1.90f, Hash01(SampleIndex * 4.0f + BandIndex + Seed, 53.0f)),
                ScaleBase * FMath::Lerp(0.65f, 1.45f, Hash01(SampleIndex * 6.0f + BandIndex + Seed, 59.0f)),
                ScaleBase * FMath::Lerp(0.28f, 0.76f, Hash01(SampleIndex * 8.0f + BandIndex + Seed, 61.0f)))
            : (bCanopy
                ? FVector(
                    ScaleBase * FMath::Lerp(0.92f, 1.55f, Hash01(SampleIndex * 4.0f + BandIndex + Seed, 53.0f)),
                    ScaleBase * FMath::Lerp(0.92f, 1.55f, Hash01(SampleIndex * 6.0f + BandIndex + Seed, 59.0f)),
                    ScaleBase * FMath::Lerp(0.82f, 1.30f, Hash01(SampleIndex * 8.0f + BandIndex + Seed, 61.0f)))
                : FVector(
                    ScaleBase * FMath::Lerp(0.86f, 1.30f, Hash01(SampleIndex * 4.0f + BandIndex + Seed, 53.0f)),
                    ScaleBase * FMath::Lerp(0.82f, 1.24f, Hash01(SampleIndex * 6.0f + BandIndex + Seed, 59.0f)),
                    ScaleBase * FMath::Lerp(0.62f, 1.18f, Hash01(SampleIndex * 8.0f + BandIndex + Seed, 61.0f))));
        const FVector Location(Location2D.X, Location2D.Y, Height + Profile.HeightOffsetCm);
        const float ScaledRadiusCm = ScaledHorizontalRadiusCm(Mesh, Scale);
        FVector TrackCenter = FVector::ZeroVector;
        float SignedTrackOffsetCm = 0.0f;
        if (!ProjectOntoTrackCorridor(TrackSamples, FVector2D(Location2D.X, Location2D.Y), TrackCenter, SignedTrackOffsetCm))
        {
            return;
        }
        if (FVector2D::Distance(FVector2D(Location2D.X, Location2D.Y), FVector2D(TrackCenter.X, TrackCenter.Y)) - ScaledRadiusCm < Profile.TrackClearanceCm)
        {
            return;
        }
        const FQuat Rotation = Profile.bAlignToSurface
            ? SurfaceAlignedRotation(Normal, Yaw)
            : FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
        Component->AddInstance(FTransform(Rotation, Location, Scale));
        ++PlacedCount;
    };

    if (Profile.Anchor == EYarlungSurfaceCoverAnchor::Track)
    {
        for (int32 SampleIndex = 0; SampleIndex < TrackSamples.Num() - 1; SampleIndex += FMath::Max(1, Profile.SampleStride))
        {
            const FYarlungSceneryTrackSample& A = TrackSamples[SampleIndex];
            const FYarlungSceneryTrackSample& B = TrackSamples[SampleIndex + 1];
            const FVector Center = (A.Position + B.Position) * 0.5f;
            const FVector Forward = HorizontalForward(TrackSamples, SampleIndex);
            const FVector Right = RightFromForward(Forward);

            for (float Side : { -1.0f, 1.0f })
            {
                for (int32 BandIndex = 0; BandIndex < Profile.LateralBandsCm.Num(); ++BandIndex)
                {
                    AddAtLocation(SampleIndex, BandIndex, Side, Center, Forward, Right);
                }
            }
        }
    }
    else
    {
        const TArray<FYarlungRiverRow>& RiverRows = RiverField.GetRows();
        for (int32 RiverIndex = 1; RiverIndex + 1 < RiverRows.Num(); RiverIndex += FMath::Max(1, Profile.SampleStride))
        {
            const FYarlungRiverRow& Row = RiverRows[RiverIndex];
            const FVector2D Prev(RiverRows[RiverIndex - 1].PositionCm.X, RiverRows[RiverIndex - 1].PositionCm.Y);
            const FVector2D Next(RiverRows[RiverIndex + 1].PositionCm.X, RiverRows[RiverIndex + 1].PositionCm.Y);
            const FVector2D Tangent2D = (Next - Prev).GetSafeNormal();
            if (Tangent2D.IsNearlyZero())
            {
                continue;
            }

            const FVector Center(Row.PositionCm.X, Row.PositionCm.Y, Row.PositionCm.Z);
            const FVector Forward(Tangent2D.X, Tangent2D.Y, 0.0f);
            const FVector Right(-Tangent2D.Y, Tangent2D.X, 0.0f);
            for (float Side : { -1.0f, 1.0f })
            {
                for (int32 BandIndex = 0; BandIndex < Profile.LateralBandsCm.Num(); ++BandIndex)
                {
                    AddAtLocation(RiverIndex, BandIndex, Side, Center, Forward, Right);
                }
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Yarlung surface cover component '%s' profile=%s instances=%d"), *ComponentConfig.Name, *Profile.Name, PlacedCount);
}

void AYarlungSceneryActor::AddRockWallSegments(
    const FYarlungAssetConfig& AssetConfig,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const TArray<uint16>& EncodedHeights,
    const FYarlungRiverField& RiverField)
{
    int32 TotalInstances = 0;
    for (const FYarlungRockWallSegmentConfig& Segment : AssetConfig.RockWallSegments)
    {
        const FYarlungRockWallProfileConfig* Profile = AssetConfig.RockWallProfiles.Find(Segment.ProfileName);
        if (!Profile)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Rock-wall segment '%s' references missing profile: %s"), *Segment.Name, *Segment.ProfileName);
        }
        int32 GroupInstances = 0;
        AddRockWallSegment(Segment, *Profile, TrackSamples, TerrainTrackPoints, EncodedHeights, RiverField, GroupInstances);
        if (GroupInstances <= 0)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Rock-wall segment '%s' placed zero instances."), *Segment.Name);
        }
        TotalInstances += GroupInstances;
        UE_LOG(LogTemp, Display, TEXT("Yarlung rock-wall segment '%s' profile=%s instances=%d"), *Segment.Name, *Segment.ProfileName, GroupInstances);
    }
    UE_LOG(LogTemp, Display, TEXT("Yarlung rock-wall segments total instances=%d"), TotalInstances);
}

void AYarlungSceneryActor::AddRockWallSegment(
    const FYarlungRockWallSegmentConfig& Segment,
    const FYarlungRockWallProfileConfig& Profile,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const TArray<uint16>& EncodedHeights,
    const FYarlungRiverField& RiverField,
    int32& InOutPlacedCount)
{
    UHierarchicalInstancedStaticMeshComponent* Component = ComponentByName(Segment.ComponentName);
    if (!Component || !Component->GetStaticMesh())
    {
        UE_LOG(LogTemp, Fatal, TEXT("Rock-wall segment '%s' references missing component or mesh: %s"), *Segment.Name, *Segment.ComponentName);
    }

    const UStaticMesh& Mesh = *Component->GetStaticMesh();
    const int32 StartSample = FMath::Clamp(Segment.StartSampleIndex, 0, FMath::Max(0, TrackSamples.Num() - 2));
    const int32 EndSample = FMath::Clamp(Segment.EndSampleIndex, StartSample + 1, FMath::Max(1, TrackSamples.Num() - 1));
    const float Side = Segment.Side;

    for (int32 SampleIndex = StartSample; SampleIndex < EndSample; SampleIndex += FMath::Max(1, Segment.SampleStride))
    {
        const FYarlungSceneryTrackSample& A = TrackSamples[SampleIndex];
        const FYarlungSceneryTrackSample& B = TrackSamples[SampleIndex + 1];
        const FVector Center = (A.Position + B.Position) * 0.5f;
        const FVector Forward = HorizontalForward(TrackSamples, SampleIndex);
        const FVector Right = RightFromForward(Forward);

        int32 LaneIndex = 0;
        const float LaneStaggerCm = (SampleIndex % 2 == 0) ? 0.0f : Profile.LateralStepCm * 0.45f;
        for (float LateralCm = Profile.LateralMinCm + LaneStaggerCm; LateralCm <= Profile.LateralMaxCm; LateralCm += Profile.LateralStepCm)
        {
            const float HashBase = SampleIndex * 19.371f + LaneIndex * 7.193f + Segment.Seed;
            const float AlongJitterCm = FMath::Lerp(-Profile.AlongJitterCm, Profile.AlongJitterCm, Hash01(HashBase, 503.0f));
            const float LateralJitterCm = FMath::Lerp(-Profile.LateralJitterCm, Profile.LateralJitterCm, Hash01(HashBase, 509.0f));
            const FVector Location2D = Center + Forward * AlongJitterCm + Right * Side * FMath::Max(0.0f, LateralCm + LateralJitterCm);

            float Height = 0.0f;
            FVector SurfaceNormal = FVector::UpVector;
            if (!TryResolvePlacement(
                EncodedHeights,
                TerrainTrackPoints,
                RiverField,
                Location2D,
                Profile.RiverClearanceCm,
                Profile.MinHeightCm,
                Profile.MaxHeightCm,
                Profile.MinSlope,
                Profile.MaxSlope,
                Height,
                SurfaceNormal))
            {
                ++LaneIndex;
                continue;
            }

            const FYarlungRiverQuery RiverQuery = RiverField.QueryNearest(FVector2D(Location2D.X, Location2D.Y));
            const FVector2D FaceRiverDirection(
                RiverQuery.Row.PositionCm.X - Location2D.X,
                RiverQuery.Row.PositionCm.Y - Location2D.Y);
            const FVector2D FallbackInwardDirection(-Side * Right.X, -Side * Right.Y);
            const float FaceInwardYaw = YawFacingDirection(
                FaceRiverDirection.IsNearlyZero() ? FallbackInwardDirection : FaceRiverDirection);
            const float Yaw = FaceInwardYaw + FMath::Lerp(-Profile.YawJitterDegrees, Profile.YawJitterDegrees, Hash01(HashBase, 521.0f));

            const float ScaleBase = FMath::Lerp(Profile.ScaleMin, Profile.ScaleMax, Hash01(HashBase, 541.0f));
            const FVector Scale(
                ScaleBase * FMath::Lerp(1.65f, 3.35f, Hash01(HashBase, 547.0f)),
                ScaleBase * FMath::Lerp(0.52f, 0.94f, Hash01(HashBase, 557.0f)),
                ScaleBase * FMath::Lerp(0.95f, 2.05f, Hash01(HashBase, 563.0f)));
            const FVector Location(Location2D.X, Location2D.Y, Height + Profile.HeightOffsetCm - Profile.EmbedDepthCm);
            const float ScaledRadiusCm = ScaledHorizontalRadiusCm(Mesh, Scale);
            if (FVector::Dist(Location, Center) - ScaledRadiusCm < Profile.TrackClearanceCm)
            {
                ++LaneIndex;
                continue;
            }

            Component->AddInstance(FTransform(SurfaceAlignedRotation(SurfaceNormal, Yaw), Location, Scale));
            ++InOutPlacedCount;
            ++LaneIndex;
        }
    }
}

void AYarlungSceneryActor::AddCliffBelt(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungCliffBeltConfig& Belt,
    float Seed,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const TArray<uint16>& EncodedHeights,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }
    const UStaticMesh& Mesh = *Component->GetStaticMesh();

    for (int32 SampleIndex = 0; SampleIndex < TrackSamples.Num() - 1; SampleIndex += FMath::Max(1, Belt.SampleStride))
    {
        const FYarlungSceneryTrackSample& A = TrackSamples[SampleIndex];
        const FYarlungSceneryTrackSample& B = TrackSamples[SampleIndex + 1];
        const FVector Center = (A.Position + B.Position) * 0.5f;
        const FVector Forward = HorizontalForward(TrackSamples, SampleIndex);
        const FVector Right = RightFromForward(Forward);

        for (float Side : { -1.0f, 1.0f })
        {
            for (int32 BandIndex = 0; BandIndex < Belt.LateralBandsCm.Num(); ++BandIndex)
            {
                const float Keep = Hash01(SampleIndex * 1.271f + BandIndex * 8.991f + Seed, Side > 0.0f ? 5.13f : 7.67f);
                if (Keep > Belt.Occupancy)
                {
                    continue;
                }

                const float AlongJitterCm = FMath::Lerp(-Belt.AlongJitterCm, Belt.AlongJitterCm, Hash01(SampleIndex * 2.381f + BandIndex + Seed, 11.0f));
                const float LateralJitterCm = FMath::Lerp(-Belt.LateralJitterCm, Belt.LateralJitterCm, Hash01(SampleIndex * 4.619f + BandIndex + Seed, 17.0f));
                const float LateralCm = FMath::Max(0.0f, Belt.LateralBandsCm[BandIndex] + LateralJitterCm);
                const float SignedOffsetCm = Side * LateralCm;
                const FVector Location2D = Center + Forward * AlongJitterCm + Right * SignedOffsetCm;

                float Height = 0.0f;
                FVector Normal = FVector::UpVector;
                if (!TryResolvePlacement(
                    EncodedHeights,
                    TerrainTrackPoints,
                    RiverField,
                    Location2D,
                    Belt.RiverClearanceCm,
                    Belt.MinHeightCm,
                    Belt.MaxHeightCm,
                    Belt.MinSlope,
                    Belt.MaxSlope,
                    Height,
                    Normal))
                {
                    continue;
                }

                const FYarlungRiverQuery RiverQuery = RiverField.QueryNearest(FVector2D(Location2D.X, Location2D.Y));
                const FVector2D FaceRiverDirection(
                    RiverQuery.Row.PositionCm.X - Location2D.X,
                    RiverQuery.Row.PositionCm.Y - Location2D.Y);
                const FVector2D FallbackInwardDirection(-Side * Right.X, -Side * Right.Y);
                const float FaceInwardYaw = YawFacingDirection(
                    FaceRiverDirection.IsNearlyZero() ? FallbackInwardDirection : FaceRiverDirection);
                const float Yaw = FaceInwardYaw + FMath::Lerp(-Belt.YawJitterDegrees, Belt.YawJitterDegrees, Hash01(SampleIndex * 3.017f + BandIndex + Seed, 23.0f));
                const float ScaleBase = FMath::Lerp(Belt.ScaleMin, Belt.ScaleMax, Hash01(SampleIndex * 6.971f + BandIndex + Seed, 31.0f));
                const FVector Scale(
                    ScaleBase * FMath::Lerp(0.95f, 2.15f, Hash01(SampleIndex * 7.0f + BandIndex + Seed, 41.0f)),
                    ScaleBase * FMath::Lerp(0.42f, 0.92f, Hash01(SampleIndex * 9.0f + BandIndex + Seed, 43.0f)),
                    ScaleBase * FMath::Lerp(0.85f, 1.65f, Hash01(SampleIndex * 5.0f + BandIndex + Seed, 47.0f)));
                const FVector Location(Location2D.X, Location2D.Y, Height + Belt.HeightOffsetCm);
                const float ScaledRadiusCm = ScaledHorizontalRadiusCm(Mesh, Scale);
                if (FVector::Dist(Location, Center) - ScaledRadiusCm < Belt.TrackClearanceCm)
                {
                    continue;
                }
                Component->AddInstance(FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)), Location, Scale));
            }
        }
    }

    AddRiverWallCliffs(Component, Belt, Seed, TrackSamples, TerrainTrackPoints, EncodedHeights, RiverField);
}

void AYarlungSceneryActor::AddRiverWallCliffs(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungCliffBeltConfig& Belt,
    float Seed,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
    const TArray<uint16>& EncodedHeights,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    const TArray<FYarlungRiverRow>& RiverRows = RiverField.GetRows();
    if (RiverRows.Num() < 3)
    {
        return;
    }

    const UStaticMesh& Mesh = *Component->GetStaticMesh();
    for (int32 RiverIndex = 1; RiverIndex + 1 < RiverRows.Num(); RiverIndex += FMath::Max(1, Belt.RiverWallSampleStride))
    {
        const FYarlungRiverRow& Row = RiverRows[RiverIndex];
        const FVector2D RiverCenter(Row.PositionCm.X, Row.PositionCm.Y);
        const FVector2D Prev(RiverRows[RiverIndex - 1].PositionCm.X, RiverRows[RiverIndex - 1].PositionCm.Y);
        const FVector2D Next(RiverRows[RiverIndex + 1].PositionCm.X, RiverRows[RiverIndex + 1].PositionCm.Y);
        const FVector2D Tangent = (Next - Prev).GetSafeNormal();
        if (Tangent.IsNearlyZero())
        {
            continue;
        }
        const FVector2D Right(-Tangent.Y, Tangent.X);

        for (float Side : { -1.0f, 1.0f })
        {
            for (int32 BandIndex = 0; BandIndex < Belt.RiverWallLateralBandsCm.Num(); ++BandIndex)
            {
                const float Keep = Hash01(RiverIndex * 1.119f + BandIndex * 5.771f + Seed, Side > 0.0f ? 61.0f : 73.0f);
                if (Keep > Belt.RiverWallOccupancy)
                {
                    continue;
                }

                const float AlongJitterCm = FMath::Lerp(-Belt.RiverWallAlongJitterCm, Belt.RiverWallAlongJitterCm, Hash01(RiverIndex * 2.871f + BandIndex + Seed, 79.0f));
                const float LateralJitterCm = FMath::Lerp(-Belt.RiverWallLateralJitterCm, Belt.RiverWallLateralJitterCm, Hash01(RiverIndex * 3.493f + BandIndex + Seed, 83.0f));
                const float LateralCm = FMath::Max(Belt.RiverClearanceCm, Belt.RiverWallLateralBandsCm[BandIndex] + LateralJitterCm);
                const FVector2D LocationXY = RiverCenter + Tangent * AlongJitterCm + Right * Side * LateralCm;

                FVector TrackCenter = FVector::ZeroVector;
                float SignedOffsetCm = 0.0f;
                if (!ProjectOntoTrackCorridor(TrackSamples, LocationXY, TrackCenter, SignedOffsetCm))
                {
                    continue;
                }

                float Height = 0.0f;
                FVector Normal = FVector::UpVector;
                const FVector Location2D(LocationXY.X, LocationXY.Y, 0.0f);
                if (!TryResolvePlacement(
                    EncodedHeights,
                    TerrainTrackPoints,
                    RiverField,
                    Location2D,
                    Belt.RiverClearanceCm,
                    Belt.MinHeightCm,
                    Belt.MaxHeightCm,
                    Belt.MinSlope,
                    Belt.MaxSlope,
                    Height,
                    Normal))
                {
                    continue;
                }

                const float FaceRiverYaw = YawFacingDirection(RiverCenter - LocationXY);
                const float Yaw = FaceRiverYaw + FMath::Lerp(-Belt.RiverWallYawJitterDegrees, Belt.RiverWallYawJitterDegrees, Hash01(RiverIndex * 4.113f + BandIndex + Seed, 89.0f));
                const float ScaleBase = FMath::Lerp(Belt.RiverWallScaleMin, Belt.RiverWallScaleMax, Hash01(RiverIndex * 6.331f + BandIndex + Seed, 97.0f));
                const FVector Scale(
                    ScaleBase * FMath::Lerp(1.15f, 2.65f, Hash01(RiverIndex * 5.0f + BandIndex + Seed, 101.0f)),
                    ScaleBase * FMath::Lerp(0.36f, 0.78f, Hash01(RiverIndex * 7.0f + BandIndex + Seed, 103.0f)),
                    ScaleBase * FMath::Lerp(0.95f, 1.85f, Hash01(RiverIndex * 9.0f + BandIndex + Seed, 107.0f)));
                const float ScaledRadiusCm = ScaledHorizontalRadiusCm(Mesh, Scale);
                const float HorizontalTrackDistanceCm = FVector2D::Distance(LocationXY, FVector2D(TrackCenter.X, TrackCenter.Y));
                if (HorizontalTrackDistanceCm - ScaledRadiusCm < Belt.TrackClearanceCm)
                {
                    continue;
                }

                const FVector Location(LocationXY.X, LocationXY.Y, Height + Belt.RiverWallHeightOffsetCm);
                Component->AddInstance(FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)), Location, Scale));
            }
        }
    }
}

void AYarlungSceneryActor::ApplyMaterials(const FYarlungAssetConfig& AssetConfig)
{
    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, YarlungGeneratedPaths::CoasterTintMaterialObjectPath);
    const auto ApplyTint = [TintMaterial](UHierarchicalInstancedStaticMeshComponent* Component, const FLinearColor& Color)
    {
        if (!Component || !TintMaterial)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Yarlung tint material setup failed for scenery component."));
        }

        const int32 SlotCount = Component->GetNumMaterials();
        if (SlotCount <= 0)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Yarlung tint target has no material slots: %s"), *Component->GetName());
        }

        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            Component->SetMaterial(SlotIndex, TintMaterial);
            UMaterialInstanceDynamic* Material = Component->CreateAndSetMaterialInstanceDynamic(SlotIndex);
            if (Material)
            {
                Material->SetVectorParameterValue(TEXT("Color"), Color);
                Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
                Material->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
                Material->SetScalarParameterValue(TEXT("Specular"), 0.04f);
            }
        }
    };

    for (const FYarlungSceneryComponentConfig& ComponentConfig : AssetConfig.SceneryComponents)
    {
        if (ComponentConfig.bUseTint)
        {
            ApplyTint(ComponentByName(ComponentConfig.Name), ComponentConfig.Tint);
        }
    }
}
