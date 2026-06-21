#include "YarlungSceneryActor.h"

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
#include "UObject/ConstructorHelpers.h"

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

    // AAA Megascans scanned assets (imported via Fab). These carry their own
    // high-res PBR material instances + Nanite, so we keep their materials rather
    // than overriding with the flat proxy tint. Paths include the Fab-generated
    // asset id; if re-imported with a different id these finders must be updated.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoulderMesh(TEXT("/Game/Fab/Megascans/3D/Beach_Boulder_uisjbbis/Medium/uisjbbis_tier_2/StaticMeshes/uisjbbis_tier_2.uisjbbis_tier_2"));
    if (BoulderMesh.Succeeded())
    {
        RockOutcrops->SetStaticMesh(BoulderMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> NordicCliffMesh(TEXT("/Game/Fab/Megascans/3D/Nordic_Forest_Cliff_Large_xibsff1/Raw/xibsff1_tier_0/StaticMeshes/xibsff1_tier_0.xibsff1_tier_0"));
    if (NordicCliffMesh.Succeeded())
    {
        CliffRockFacesA->SetStaticMesh(NordicCliffMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> QuarryCliffMesh(TEXT("/Game/Fab/Megascans/3D/Quarry_Cliff_uchwaffda/High/uchwaffda_tier_1/StaticMeshes/uchwaffda_tier_1.uchwaffda_tier_1"));
    if (QuarryCliffMesh.Succeeded())
    {
        CliffRockFacesB->SetStaticMesh(QuarryCliffMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubAMesh(TEXT("/Game/Generated/Models/Shrub03/shrub_03_1k.shrub_03_1k"));
    if (ShrubAMesh.Succeeded())
    {
        ForestShrubsA->SetStaticMesh(ShrubAMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ShrubBMesh(TEXT("/Game/Generated/Models/Shrub04/shrub_04_1k.shrub_04_1k"));
    if (ShrubBMesh.Succeeded())
    {
        ForestShrubsB->SetStaticMesh(ShrubBMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SpruceBranchAMesh(TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Branch_Norway_Spruce_01.Branch_Norway_Spruce_01"));
    if (SpruceBranchAMesh.Succeeded())
    {
        CanopyTreesA->SetStaticMesh(SpruceBranchAMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SpruceBranchBMesh(TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Branch_Norway_Spruce_05.Branch_Norway_Spruce_05"));
    if (SpruceBranchBMesh.Succeeded())
    {
        CanopyTreesB->SetStaticMesh(SpruceBranchBMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SpruceTopMesh(TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Branch_Norway_Spruce_Top_01.Branch_Norway_Spruce_Top_01"));
    if (SpruceTopMesh.Succeeded())
    {
        CanopyTreesC->SetStaticMesh(SpruceTopMesh.Object);
    }
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
        ForestShrubsA->SetVisibility(false, true);
        ForestShrubsB->SetVisibility(false, true);
        CanopyTreesA->SetVisibility(false, true);
        CanopyTreesB->SetVisibility(false, true);
        CanopyTreesC->SetVisibility(false, true);
    }
}

void AYarlungSceneryActor::RebuildScenery()
{
    TArray<FYarlungSceneryTrackSample> TrackSamples;
    TArray<uint16> HeightData;
    if (!LoadOutboundTrack(TrackSamples) || !LoadHeightmap(HeightData))
    {
        RockOutcrops->ClearInstances();
        UnderstoryClumps->ClearInstances();
        CliffRockFacesA->ClearInstances();
        CliffRockFacesB->ClearInstances();
        ForestShrubsA->ClearInstances();
        ForestShrubsB->ClearInstances();
        CanopyTreesA->ClearInstances();
        CanopyTreesB->ClearInstances();
        CanopyTreesC->ClearInstances();
        return;
    }

    BuildScatter(TrackSamples, HeightData);
    ApplyMaterials();
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

void AYarlungSceneryActor::BuildScatter(const TArray<FYarlungSceneryTrackSample>& TrackSamples, const TArray<uint16>& HeightData)
{
    RockOutcrops->ClearInstances();
    UnderstoryClumps->ClearInstances();
    CliffRockFacesA->ClearInstances();
    CliffRockFacesB->ClearInstances();
    ForestShrubsA->ClearInstances();
    ForestShrubsB->ClearInstances();
    CanopyTreesA->ClearInstances();
    CanopyTreesB->ClearInstances();
    CanopyTreesC->ClearInstances();

    FYarlungRiverField RiverField;
    FString RiverLoadError;
    if (!RiverField.LoadFromProjectContent(&RiverLoadError))
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung scenery requires generated river field: %s"), *RiverLoadError);
        return;
    }

    // Real Megascans Nanite cliff/boulder scatter (first AAA integration pass).
    // Counts/scales are a first estimate against unknown native asset size; tune
    // from the rendered result. Cliff count is per-component (A and B both use the
    // Nordic cliff), so total cliff instances ~= 2x CliffFaceCount.
    constexpr int32 RockCount = 90;
    constexpr int32 ClumpCount = 0;
    constexpr int32 CliffFaceCount = 72;
    constexpr int32 ShrubCount = 0;
    constexpr int32 RandomCanopyTreeCount = 280;
    constexpr float MinLateralCm = 2600.0f;
    constexpr float MaxLateralCm = 118000.0f;

    enum class EScatterKind
    {
        Boulder,
        CliffFace,
        CanopyTree,
        Shrub
    };

    const auto AddScatter = [&](UHierarchicalInstancedStaticMeshComponent* Component, int32 Count, EScatterKind Kind, float Seed)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            return;
        }

        for (int32 Index = 0; Index < Count; ++Index)
        {
            const bool bLargeRock = Kind == EScatterKind::Boulder;
            const bool bCliffFace = Kind == EScatterKind::CliffFace;
            const bool bCanopyTree = Kind == EScatterKind::CanopyTree;
            const bool bShrub = Kind == EScatterKind::Shrub;
            const float AlongHash = Hash01(Index * 3.171f + Seed, bLargeRock ? 17.0f : (bCliffFace ? 23.0f : (bCanopyTree ? 27.0f : 29.0f)));
            const int32 SampleIndex = FMath::Clamp(
                FMath::FloorToInt(AlongHash * static_cast<float>(TrackSamples.Num() - 1)),
                0,
                TrackSamples.Num() - 2);
            const FYarlungSceneryTrackSample& A = TrackSamples[SampleIndex];
            const FYarlungSceneryTrackSample& B = TrackSamples[SampleIndex + 1];
            const float LocalT = Hash01(Index * 8.311f + Seed, bLargeRock ? 4.0f : 11.0f);
            const FVector Center = FMath::Lerp(A.Position, B.Position, LocalT);
            const FVector Forward = HorizontalForward(TrackSamples, SampleIndex);
            const FVector Right = RightFromForward(Forward);

            const float SideBias = bCliffFace ? 0.70f : (bCanopyTree ? 0.62f : 0.58f);
            const float Side = Hash01(Index * 5.991f + Seed, 3.0f) < SideBias ? 1.0f : -1.0f;
            const float DistanceHash = Hash01(Index * 1.731f + Seed, bLargeRock ? 41.0f : (bCliffFace ? 47.0f : (bCanopyTree ? 49.0f : 53.0f)));
            const float DistanceT = FMath::Pow(DistanceHash, bLargeRock ? 0.82f : (bCliffFace ? 0.78f : (bCanopyTree ? 1.38f : 1.18f)));
            const float LocalMinLateralCm = bCliffFace ? 5200.0f : (bCanopyTree ? 26000.0f : (bShrub ? 3600.0f : MinLateralCm));
            const float LocalMaxLateralCm = bCliffFace ? 42000.0f : (bCanopyTree ? 126000.0f : (bShrub ? 108000.0f : MaxLateralCm));
            const float LateralCm = FMath::Lerp(LocalMinLateralCm, LocalMaxLateralCm, DistanceT);
            const float AlongJitterCm = FMath::Lerp(-2400.0f, 2400.0f, Hash01(Index * 2.117f + Seed, 71.0f));
            const FVector Location2D = Center + Forward * AlongJitterCm + Right * Side * LateralCm;
            const float SignedOffsetCm = Side * LateralCm;

            const YarlungTerrain::FConfig& Bounds = YarlungTerrain::Config();
            if (Location2D.X < Bounds.MinXCm || Location2D.X > Bounds.MaxXCm || Location2D.Y < Bounds.MinYCm || Location2D.Y > Bounds.MaxYCm)
            {
                continue;
            }

            const float RiverDistanceCm = RiverField.DistanceCm(FVector2D(Location2D.X, Location2D.Y));
            const float RiverClearanceCm = bCliffFace ? 96000.0f : (bLargeRock ? 72000.0f : (bCanopyTree ? 52000.0f : 42000.0f));
            if (RiverDistanceCm < RiverClearanceCm)
            {
                continue;
            }

            const float BaseHeight = SampleHeightCm(HeightData, Location2D.X, Location2D.Y);
            const float TrackBaseHeight = SampleHeightCm(HeightData, Center.X, Center.Y);
            const float Height = YarlungCorridorProfile::AuthoredHeightCm(
                FVector2D(Center.X, Center.Y),
                SignedOffsetCm,
                TrackBaseHeight,
                BaseHeight);
            const float MaxHeightCm = bShrub ? 455000.0f : (bCanopyTree ? 470000.0f : (bCliffFace ? 520000.0f : 430000.0f));
            if (Height < 262000.0f || Height > MaxHeightCm)
            {
                continue;
            }

            const FVector Normal = SampleNormal(HeightData, Location2D.X, Location2D.Y);
            const float Slope = 1.0f - Normal.Z;
            if (bLargeRock && Slope < 0.05f)
            {
                continue;
            }
            if (bCliffFace && Slope < 0.075f)
            {
                continue;
            }
            if (bCanopyTree && Slope > 0.62f)
            {
                continue;
            }

            const float Yaw = bCliffFace
                ? FMath::RadiansToDegrees(FMath::Atan2((-Side * Right).Y, (-Side * Right).X)) + FMath::Lerp(-24.0f, 24.0f, Hash01(Index * 3.1f + Seed, 8.0f))
                : Hash01(Index * 4.121f + Seed, 97.0f) * 360.0f;
            // These Megaplant inputs are branch/top static meshes, not whole-tree
            // meshes. Use them as mid-distance canopy clumps: larger than their
            // real branch scale so they break the smooth green terrain into forest
            // texture in first-person frames.
            const float ScaleBase = bLargeRock
                ? FMath::Lerp(1.2f, 4.0f, Hash01(Index * 6.617f, 13.0f))
                : (bCliffFace ? FMath::Lerp(9.0f, 20.0f, Hash01(Index * 6.617f + Seed, 13.0f)) : (bCanopyTree ? FMath::Lerp(10.0f, 24.0f, Hash01(Index * 6.617f + Seed, 13.0f)) : FMath::Lerp(6.5f, 22.0f, Hash01(Index * 6.617f + Seed, 13.0f))));
            const FVector Scale = bLargeRock
                ? FVector(ScaleBase * FMath::Lerp(0.7f, 1.8f, Hash01(Index * 9.0f, 1.0f)), ScaleBase, ScaleBase * FMath::Lerp(0.45f, 1.15f, Hash01(Index * 7.0f, 2.0f)))
                : (bCliffFace
                    ? FVector(ScaleBase * FMath::Lerp(0.8f, 1.9f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.45f, 1.2f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.65f, 1.45f, Hash01(Index * 5.0f + Seed, 3.0f)))
                    : (bCanopyTree
                        ? FVector(ScaleBase * FMath::Lerp(0.85f, 1.55f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.85f, 1.55f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.80f, 1.35f, Hash01(Index * 5.0f + Seed, 3.0f)))
                        : FVector(ScaleBase * FMath::Lerp(0.8f, 1.55f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.75f, 1.3f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.75f, 1.8f, Hash01(Index * 5.0f + Seed, 3.0f)))));
            const FVector Location(Location2D.X, Location2D.Y, Height + (bLargeRock ? 24.0f : (bCliffFace ? 18.0f : (bCanopyTree ? 85.0f : 6.0f))));
            const FQuat Rotation = bCanopyTree
                ? FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw))
                : SurfaceAlignedRotation(Normal, Yaw);
            Component->AddInstance(FTransform(Rotation, Location, Scale));
        }
    };

    const auto AddCanopyBelt = [&](UHierarchicalInstancedStaticMeshComponent* Component, float Seed)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            return;
        }

        // Dense midground belts beat route-wide random dots for the hero POV:
        // the refs read as continuous green forest mass with individual canopy
        // texture, not isolated speckles on smooth green material.
        constexpr int32 SampleStride = 2;
        constexpr float LateralBandsCm[] = { 26000.0f, 42000.0f, 62000.0f, 88000.0f, 118000.0f };

        for (int32 SampleIndex = 0; SampleIndex < TrackSamples.Num() - 1; SampleIndex += SampleStride)
        {
            const FYarlungSceneryTrackSample& A = TrackSamples[SampleIndex];
            const FYarlungSceneryTrackSample& B = TrackSamples[SampleIndex + 1];
            const FVector Center = (A.Position + B.Position) * 0.5f;
            const FVector Forward = HorizontalForward(TrackSamples, SampleIndex);
            const FVector Right = RightFromForward(Forward);

            for (float Side : { -1.0f, 1.0f })
            {
                for (int32 BandIndex = 0; BandIndex < UE_ARRAY_COUNT(LateralBandsCm); ++BandIndex)
                {
                    const float Occupancy = BandIndex < 3 ? 0.78f : 0.54f;
                    const float Keep = Hash01(SampleIndex * 1.977f + BandIndex * 6.113f + Seed, Side > 0.0f ? 0.19f : 0.73f);
                    if (Keep > Occupancy)
                    {
                        continue;
                    }

                    const float AlongJitterCm = FMath::Lerp(-2200.0f, 2200.0f, Hash01(SampleIndex * 3.717f + BandIndex + Seed, 2.0f));
                    const float LateralJitterCm = FMath::Lerp(-3600.0f, 3600.0f, Hash01(SampleIndex * 4.513f + BandIndex + Seed, 5.0f));
                    const float LateralCm = FMath::Max(22000.0f, LateralBandsCm[BandIndex] + LateralJitterCm);
                    const float SignedOffsetCm = Side * LateralCm;
                    const FVector Location2D = Center + Forward * AlongJitterCm + Right * SignedOffsetCm;

                    const YarlungTerrain::FConfig& Bounds = YarlungTerrain::Config();
                    if (Location2D.X < Bounds.MinXCm || Location2D.X > Bounds.MaxXCm || Location2D.Y < Bounds.MinYCm || Location2D.Y > Bounds.MaxYCm)
                    {
                        continue;
                    }

                    const float RiverDistanceCm = RiverField.DistanceCm(FVector2D(Location2D.X, Location2D.Y));
                    if (RiverDistanceCm < 56000.0f)
                    {
                        continue;
                    }

                    const float BaseHeight = SampleHeightCm(HeightData, Location2D.X, Location2D.Y);
                    const float TrackBaseHeight = SampleHeightCm(HeightData, Center.X, Center.Y);
                    const float Height = YarlungCorridorProfile::AuthoredHeightCm(
                        FVector2D(Center.X, Center.Y),
                        SignedOffsetCm,
                        TrackBaseHeight,
                        BaseHeight);
                    if (Height < 262000.0f || Height > 476000.0f)
                    {
                        continue;
                    }

                    const FVector Normal = SampleNormal(HeightData, Location2D.X, Location2D.Y);
                    const float Slope = 1.0f - Normal.Z;
                    if (Slope > 0.78f)
                    {
                        continue;
                    }

                    const float Yaw = Hash01(SampleIndex * 7.071f + BandIndex + Seed, 97.0f) * 360.0f;
                    const float ScaleBase = FMath::Lerp(14.0f, 34.0f, Hash01(SampleIndex * 5.317f + BandIndex + Seed, 13.0f));
                    const float XYJitter = FMath::Lerp(0.86f, 1.42f, Hash01(SampleIndex * 2.911f + BandIndex + Seed, 17.0f));
                    const float ZJitter = FMath::Lerp(0.72f, 1.28f, Hash01(SampleIndex * 3.151f + BandIndex + Seed, 23.0f));
                    const FVector Scale(ScaleBase * XYJitter, ScaleBase * FMath::Lerp(0.82f, 1.22f, Keep), ScaleBase * ZJitter);
                    const FVector Location(Location2D.X, Location2D.Y, Height + 120.0f);
                    const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
                    Component->AddInstance(FTransform(Rotation, Location, Scale));
                }
            }
        }
    };

    AddScatter(RockOutcrops, RockCount, EScatterKind::Boulder, 0.0f);
    AddScatter(UnderstoryClumps, ClumpCount, EScatterKind::Shrub, 3.0f);
    AddScatter(CliffRockFacesA, CliffFaceCount, EScatterKind::CliffFace, 17.0f);
    AddScatter(CliffRockFacesB, CliffFaceCount, EScatterKind::CliffFace, 31.0f);
    AddScatter(ForestShrubsA, ShrubCount, EScatterKind::Shrub, 43.0f);
    AddScatter(ForestShrubsB, ShrubCount, EScatterKind::Shrub, 59.0f);
    AddScatter(CanopyTreesA, RandomCanopyTreeCount, EScatterKind::CanopyTree, 71.0f);
    AddScatter(CanopyTreesB, RandomCanopyTreeCount, EScatterKind::CanopyTree, 83.0f);
    AddScatter(CanopyTreesC, RandomCanopyTreeCount, EScatterKind::CanopyTree, 97.0f);
    AddCanopyBelt(CanopyTreesA, 131.0f);
    AddCanopyBelt(CanopyTreesB, 149.0f);
    AddCanopyBelt(CanopyTreesC, 167.0f);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung scatter instances: rocks=%d understory=%d cliff_a=%d cliff_b=%d shrubs_a=%d shrubs_b=%d canopy_a=%d canopy_b=%d canopy_c=%d"),
        RockOutcrops->GetInstanceCount(),
        UnderstoryClumps->GetInstanceCount(),
        CliffRockFacesA->GetInstanceCount(),
        CliffRockFacesB->GetInstanceCount(),
        ForestShrubsA->GetInstanceCount(),
        ForestShrubsB->GetInstanceCount(),
        CanopyTreesA->GetInstanceCount(),
        CanopyTreesB->GetInstanceCount(),
        CanopyTreesC->GetInstanceCount());
}

void AYarlungSceneryActor::ApplyMaterials()
{
    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));
    const auto ApplyTint = [TintMaterial](UHierarchicalInstancedStaticMeshComponent* Component, const FLinearColor& Color)
    {
        if (!Component || !TintMaterial)
        {
            return;
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

    // Wet Himalayan gorge rock sits around 0.08-0.13 albedo. The old 0.23-0.28
    // tints were ~3x the terrain mesh (~0.08), so under the daylight sun these
    // flat-tinted rock assets clipped to "styrofoam white" in the foreground
    // while the terrain behind them read correctly. Bring them into rock range.
    // RockOutcrops (boulder) and CliffRockFacesA/B (cliff) are Megascans assets:
    // keep their scanned PBR material instances, do NOT overwrite with flat tint.
    // Only the remaining proxy scatter (understory/shrubs) gets the tint.
    ApplyTint(UnderstoryClumps, FLinearColor(0.035f, 0.16f, 0.055f));
    ApplyTint(ForestShrubsA, FLinearColor(0.026f, 0.13f, 0.048f));
    ApplyTint(ForestShrubsB, FLinearColor(0.045f, 0.18f, 0.068f));
}
