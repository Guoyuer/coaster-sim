#include "YarlungSceneryActor.h"

#include "YarlungAssetConfig.h"
#include "YarlungCorridorProfile.h"
#include "YarlungRiverField.h"
#include "YarlungTerrainProfile.h"
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
// Heightmap dimensions, world bounds, and height encoding all come from
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
        UnderstoryClumps->SetVisibility(false, true);
        CliffRockFacesA->SetVisibility(false, true);
        CliffRockFacesB->SetVisibility(false, true);
        CliffRockFacesC->SetVisibility(false, true);
        CliffRockFacesD->SetVisibility(false, true);
        CliffRockFacesE->SetVisibility(false, true);
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
    TArray<uint16> HeightData;
    if (!LoadOutboundTrack(TrackSamples) || !LoadHeightmap(HeightData))
    {
        ClearAllInstances();
        return;
    }

    BuildScatter(TrackSamples, HeightData, AssetConfig);
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
    if (Name == TEXT("UnderstoryClumps")) { return UnderstoryClumps; }
    if (Name == TEXT("CliffRockFacesA")) { return CliffRockFacesA; }
    if (Name == TEXT("CliffRockFacesB")) { return CliffRockFacesB; }
    if (Name == TEXT("CliffRockFacesC")) { return CliffRockFacesC; }
    if (Name == TEXT("CliffRockFacesD")) { return CliffRockFacesD; }
    if (Name == TEXT("CliffRockFacesE")) { return CliffRockFacesE; }
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
    UnderstoryClumps->ClearInstances();
    CliffRockFacesA->ClearInstances();
    CliffRockFacesB->ClearInstances();
    CliffRockFacesC->ClearInstances();
    CliffRockFacesD->ClearInstances();
    CliffRockFacesE->ClearInstances();
    ForestShrubsA->ClearInstances();
    ForestShrubsB->ClearInstances();
    CanopyTreesA->ClearInstances();
    CanopyTreesB->ClearInstances();
    CanopyTreesC->ClearInstances();
}

bool AYarlungSceneryActor::LoadOutboundTrack(TArray<FYarlungSceneryTrackSample>& OutSamples) const
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
        const bool bScenerySection =
            Row.Section.Equals(TEXT("Lift"), ESearchCase::IgnoreCase) ||
            Row.Section.Equals(TEXT("Outbound"), ESearchCase::IgnoreCase) ||
            Row.Section.Equals(TEXT("Turnaround"), ESearchCase::IgnoreCase) ||
            Row.Section.Equals(TEXT("Return"), ESearchCase::IgnoreCase) ||
            Row.Section.Equals(TEXT("Launch"), ESearchCase::IgnoreCase);
        if (bScenerySection)
        {
            FYarlungSceneryTrackSample Sample;
            Sample.Position = Row.PositionCm;
            Sample.Section = Row.Section;
            OutSamples.Add(Sample);
        }
    }

    if (OutSamples.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few ride-corridor samples for Yarlung scenery scatter: %d"), OutSamples.Num());
        return false;
    }

    return true;
}

bool AYarlungSceneryActor::LoadHeightmap(TArray<uint16>& OutHeightData) const
{
    const FString Path = FPaths::ProjectContentDir() / HeightmapRelativePath;
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung heightmap for scenery scatter: %s"), *Path);
        return false;
    }

    const int32 HeightmapSize = YarlungTerrain::Config().GridSize;
    const int32 ExpectedByteCount = HeightmapSize * HeightmapSize * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung scenery heightmap has %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return false;
    }

    OutHeightData.SetNumUninitialized(HeightmapSize * HeightmapSize);
    FMemory::Memcpy(OutHeightData.GetData(), RawBytes.GetData(), RawBytes.Num());
    return true;
}

float AYarlungSceneryActor::SampleHeightCm(const TArray<uint16>& HeightData, float X, float Y) const
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const int32 HeightmapSize = Tc.GridSize;
    const float GridX = FMath::Clamp((X - Tc.MinXCm) / (Tc.MaxXCm - Tc.MinXCm) * (HeightmapSize - 1), 0.0f, static_cast<float>(HeightmapSize - 1));
    const float GridY = FMath::Clamp((Y - Tc.MinYCm) / (Tc.MaxYCm - Tc.MinYCm) * (HeightmapSize - 1), 0.0f, static_cast<float>(HeightmapSize - 1));
    const int32 X0 = FMath::FloorToInt(GridX);
    const int32 Y0 = FMath::FloorToInt(GridY);
    const int32 X1 = FMath::Min(HeightmapSize - 1, X0 + 1);
    const int32 Y1 = FMath::Min(HeightmapSize - 1, Y0 + 1);
    const float Tx = GridX - X0;
    const float Ty = GridY - Y0;

    const auto At = [&HeightData, HeightmapSize](int32 SampleX, int32 SampleY)
    {
        return YarlungTerrain::HeightValueToCm(HeightData[SampleY * HeightmapSize + SampleX]);
    };

    const float A = FMath::Lerp(At(X0, Y0), At(X1, Y0), Tx);
    const float B = FMath::Lerp(At(X0, Y1), At(X1, Y1), Tx);
    return FMath::Lerp(A, B, Ty);
}

FVector AYarlungSceneryActor::SampleNormal(const TArray<uint16>& HeightData, float X, float Y) const
{
    constexpr float StepCm = 1800.0f;
    const float Left = SampleHeightCm(HeightData, X - StepCm, Y);
    const float Right = SampleHeightCm(HeightData, X + StepCm, Y);
    const float Down = SampleHeightCm(HeightData, X, Y - StepCm);
    const float Up = SampleHeightCm(HeightData, X, Y + StepCm);
    return FVector(Left - Right, Down - Up, StepCm * 2.0f).GetSafeNormal();
}

void AYarlungSceneryActor::BuildScatter(
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<uint16>& HeightData,
    const FYarlungAssetConfig& AssetConfig)
{
    ClearAllInstances();

    FYarlungRiverField RiverField;
    FString RiverLoadError;
    if (!RiverField.LoadFromProjectContent(&RiverLoadError))
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung scenery requires generated river field: %s"), *RiverLoadError);
        return;
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
            AddScatterRule(Component, ComponentConfig, *KindConfig, TrackSamples, HeightData, RiverField);
            break;
        case EYarlungSceneryPlacement::CanopyBelt:
            AddCanopyBelt(Component, AssetConfig.CanopyBelt, ComponentConfig.Seed, TrackSamples, HeightData, RiverField);
            break;
        case EYarlungSceneryPlacement::CliffBelt:
            AddCliffBelt(Component, AssetConfig.CliffBelt, ComponentConfig.Seed, TrackSamples, HeightData, RiverField);
            break;
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung scatter instances: rocks=%d understory=%d cliff_a=%d cliff_b=%d cliff_c=%d cliff_d=%d cliff_e=%d shrubs_a=%d shrubs_b=%d canopy_a=%d canopy_b=%d canopy_c=%d"),
        RockOutcrops->GetInstanceCount(),
        UnderstoryClumps->GetInstanceCount(),
        CliffRockFacesA->GetInstanceCount(),
        CliffRockFacesB->GetInstanceCount(),
        CliffRockFacesC->GetInstanceCount(),
        CliffRockFacesD->GetInstanceCount(),
        CliffRockFacesE->GetInstanceCount(),
        ForestShrubsA->GetInstanceCount(),
        ForestShrubsB->GetInstanceCount(),
        CanopyTreesA->GetInstanceCount(),
        CanopyTreesB->GetInstanceCount(),
        CanopyTreesC->GetInstanceCount());
}

bool AYarlungSceneryActor::TryResolvePlacement(
    const TArray<uint16>& HeightData,
    const FYarlungRiverField& RiverField,
    const FVector& Center,
    const FVector& Location2D,
    float SignedOffsetCm,
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

    const float BaseHeight = SampleHeightCm(HeightData, Location2D.X, Location2D.Y);
    const float TrackBaseHeight = SampleHeightCm(HeightData, Center.X, Center.Y);
    OutHeightCm = YarlungCorridorProfile::AuthoredHeightCm(
        FVector2D(Center.X, Center.Y),
        SignedOffsetCm,
        TrackBaseHeight,
        BaseHeight);
    if (OutHeightCm < MinHeightCm || OutHeightCm > MaxHeightCm)
    {
        return false;
    }

    OutNormal = SampleNormal(HeightData, Location2D.X, Location2D.Y);
    const float Slope = 1.0f - OutNormal.Z;
    return Slope >= MinSlope && Slope <= MaxSlope;
}

void AYarlungSceneryActor::AddScatterRule(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungSceneryComponentConfig& ComponentConfig,
    const FYarlungScatterKindConfig& KindConfig,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<uint16>& HeightData,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

    const bool bBoulder = ComponentConfig.Kind == TEXT("boulder");
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
        const float SignedOffsetCm = Side * LateralCm;

        float Height = 0.0f;
        FVector Normal = FVector::UpVector;
        if (!TryResolvePlacement(
            HeightData,
            RiverField,
            Center,
            Location2D,
            SignedOffsetCm,
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
            ? FVector(ScaleBase * FMath::Lerp(0.7f, 1.8f, Hash01(Index * 9.0f, 1.0f)), ScaleBase, ScaleBase * FMath::Lerp(0.45f, 1.15f, Hash01(Index * 7.0f, 2.0f)))
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

void AYarlungSceneryActor::AddCanopyBelt(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungCanopyBeltConfig& Belt,
    float Seed,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<uint16>& HeightData,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }

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
                const float Occupancy = BandIndex < Belt.NearBandCount ? Belt.NearOccupancy : Belt.FarOccupancy;
                const float Keep = Hash01(SampleIndex * 1.977f + BandIndex * 6.113f + Seed, Side > 0.0f ? 0.19f : 0.73f);
                if (Keep > Occupancy)
                {
                    continue;
                }

                const float AlongJitterCm = FMath::Lerp(-2200.0f, 2200.0f, Hash01(SampleIndex * 3.717f + BandIndex + Seed, 2.0f));
                const float LateralJitterCm = FMath::Lerp(-3600.0f, 3600.0f, Hash01(SampleIndex * 4.513f + BandIndex + Seed, 5.0f));
                const float LateralCm = FMath::Max(0.0f, Belt.LateralBandsCm[BandIndex] + LateralJitterCm);
                const float SignedOffsetCm = Side * LateralCm;
                const FVector Location2D = Center + Forward * AlongJitterCm + Right * SignedOffsetCm;

                float Height = 0.0f;
                FVector Normal = FVector::UpVector;
                if (!TryResolvePlacement(
                    HeightData,
                    RiverField,
                    Center,
                    Location2D,
                    SignedOffsetCm,
                    Belt.RiverClearanceCm,
                    Belt.MinHeightCm,
                    Belt.MaxHeightCm,
                    0.0f,
                    Belt.MaxSlope,
                    Height,
                    Normal))
                {
                    continue;
                }

                const float Yaw = Hash01(SampleIndex * 7.071f + BandIndex + Seed, 97.0f) * 360.0f;
                const float ScaleBase = FMath::Lerp(Belt.ScaleMin, Belt.ScaleMax, Hash01(SampleIndex * 5.317f + BandIndex + Seed, 13.0f));
                const float XYJitter = FMath::Lerp(0.86f, 1.42f, Hash01(SampleIndex * 2.911f + BandIndex + Seed, 17.0f));
                const float ZJitter = FMath::Lerp(0.72f, 1.28f, Hash01(SampleIndex * 3.151f + BandIndex + Seed, 23.0f));
                const FVector Scale(ScaleBase * XYJitter, ScaleBase * FMath::Lerp(0.82f, 1.22f, Keep), ScaleBase * ZJitter);
                const FVector Location(Location2D.X, Location2D.Y, Height + Belt.HeightOffsetCm);
                const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
                Component->AddInstance(FTransform(Rotation, Location, Scale));
            }
        }
    }
}

void AYarlungSceneryActor::AddCliffBelt(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const FYarlungCliffBeltConfig& Belt,
    float Seed,
    const TArray<FYarlungSceneryTrackSample>& TrackSamples,
    const TArray<uint16>& HeightData,
    const FYarlungRiverField& RiverField)
{
    if (!Component || !Component->GetStaticMesh())
    {
        return;
    }
    const float MeshRadiusCm = Component->GetStaticMesh()->GetBounds().SphereRadius;

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
                    HeightData,
                    RiverField,
                    Center,
                    Location2D,
                    SignedOffsetCm,
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

                const float FaceInwardYaw = FMath::RadiansToDegrees(FMath::Atan2((-Side * Right).Y, (-Side * Right).X));
                const float Yaw = FaceInwardYaw + FMath::Lerp(-Belt.YawJitterDegrees, Belt.YawJitterDegrees, Hash01(SampleIndex * 3.017f + BandIndex + Seed, 23.0f));
                const float ScaleBase = FMath::Lerp(Belt.ScaleMin, Belt.ScaleMax, Hash01(SampleIndex * 6.971f + BandIndex + Seed, 31.0f));
                const FVector Scale(
                    ScaleBase * FMath::Lerp(0.95f, 2.15f, Hash01(SampleIndex * 7.0f + BandIndex + Seed, 41.0f)),
                    ScaleBase * FMath::Lerp(0.42f, 0.92f, Hash01(SampleIndex * 9.0f + BandIndex + Seed, 43.0f)),
                    ScaleBase * FMath::Lerp(0.85f, 1.65f, Hash01(SampleIndex * 5.0f + BandIndex + Seed, 47.0f)));
                const FVector Location(Location2D.X, Location2D.Y, Height + Belt.HeightOffsetCm);
                const float ScaledRadiusCm = MeshRadiusCm * Scale.GetMax();
                if (FVector::Dist(Location, Center) - ScaledRadiusCm < Belt.TrackClearanceCm)
                {
                    continue;
                }
                Component->AddInstance(FTransform(SurfaceAlignedRotation(Normal, Yaw), Location, Scale));
            }
        }
    }
}

void AYarlungSceneryActor::ApplyMaterials(const FYarlungAssetConfig& AssetConfig)
{
    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));
    const auto ApplyTint = [TintMaterial](UHierarchicalInstancedStaticMeshComponent* Component, const FLinearColor& Color)
    {
        if (!Component || !TintMaterial)
        {
            UE_LOG(LogTemp, Fatal, TEXT("Yarlung tint material setup failed for scenery component."));
        }

        Component->SetMaterial(0, TintMaterial);
        UMaterialInstanceDynamic* Material = Component->CreateAndSetMaterialInstanceDynamic(0);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("Color"), Color);
            Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
            Material->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
            Material->SetScalarParameterValue(TEXT("Specular"), 0.04f);
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
