#include "YarlungSceneryActor.h"

#include "YarlungCorridorProfile.h"
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

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoulderMesh(TEXT("/Game/Generated/Models/Boulder01/boulder_01_1k.boulder_01_1k"));
    if (BoulderMesh.Succeeded())
    {
        RockOutcrops->SetStaticMesh(BoulderMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockFaceAMesh(TEXT("/Game/Generated/Models/RockFace01/rock_face_01_1k.rock_face_01_1k"));
    if (RockFaceAMesh.Succeeded())
    {
        CliffRockFacesA->SetStaticMesh(RockFaceAMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> RockFaceBMesh(TEXT("/Game/Generated/Models/RockFace02/rock_face_02_1k.rock_face_02_1k"));
    if (RockFaceBMesh.Succeeded())
    {
        CliffRockFacesB->SetStaticMesh(RockFaceBMesh.Object);
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
        if (Row.Section.Equals(TEXT("Outbound"), ESearchCase::IgnoreCase))
        {
            FYarlungSceneryTrackSample Sample;
            Sample.Position = Row.PositionCm;
            Sample.Section = Row.Section;
            OutSamples.Add(Sample);
        }
    }

    if (OutSamples.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few Outbound samples for Yarlung scenery scatter: %d"), OutSamples.Num());
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

    constexpr int32 RockCount = 0;
    constexpr int32 ClumpCount = 0;
    // The current rock_face proxy meshes read as low-poly curtain walls in the
    // hero POV. Keep them disabled until real authored/Nanite cliff assets land.
    constexpr int32 CliffFaceCount = 0;
    constexpr int32 ShrubCount = 0;
    constexpr float MinLateralCm = 2600.0f;
    constexpr float MaxLateralCm = 118000.0f;

    enum class EScatterKind
    {
        Boulder,
        CliffFace,
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
            const bool bShrub = Kind == EScatterKind::Shrub;
            const float AlongHash = Hash01(Index * 3.171f + Seed, bLargeRock ? 17.0f : (bCliffFace ? 23.0f : 29.0f));
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

            const float SideBias = bCliffFace ? 0.70f : 0.58f;
            const float Side = Hash01(Index * 5.991f + Seed, 3.0f) < SideBias ? 1.0f : -1.0f;
            const float DistanceHash = Hash01(Index * 1.731f + Seed, bLargeRock ? 41.0f : (bCliffFace ? 47.0f : 53.0f));
            const float DistanceT = FMath::Pow(DistanceHash, bLargeRock ? 0.82f : (bCliffFace ? 0.72f : 1.18f));
            const float LocalMinLateralCm = bCliffFace ? 5200.0f : (bShrub ? 3600.0f : MinLateralCm);
            const float LocalMaxLateralCm = bCliffFace ? 86000.0f : (bShrub ? 108000.0f : MaxLateralCm);
            const float LateralCm = FMath::Lerp(LocalMinLateralCm, LocalMaxLateralCm, DistanceT);
            const float AlongJitterCm = FMath::Lerp(-2400.0f, 2400.0f, Hash01(Index * 2.117f + Seed, 71.0f));
            const FVector Location2D = Center + Forward * AlongJitterCm + Right * Side * LateralCm;
            const float SignedOffsetCm = Side * LateralCm;

            const YarlungTerrain::FConfig& Bounds = YarlungTerrain::Config();
            if (Location2D.X < Bounds.MinXCm || Location2D.X > Bounds.MaxXCm || Location2D.Y < Bounds.MinYCm || Location2D.Y > Bounds.MaxYCm)
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
            const float MaxHeightCm = bShrub ? 455000.0f : (bCliffFace ? 520000.0f : 430000.0f);
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

            const float Yaw = bCliffFace
                ? FMath::RadiansToDegrees(FMath::Atan2((-Side * Right).Y, (-Side * Right).X)) + FMath::Lerp(-24.0f, 24.0f, Hash01(Index * 3.1f + Seed, 8.0f))
                : Hash01(Index * 4.121f + Seed, 97.0f) * 360.0f;
            const float ScaleBase = bLargeRock
                ? FMath::Lerp(2.4f, 9.5f, Hash01(Index * 6.617f, 13.0f))
                : (bCliffFace ? FMath::Lerp(18.0f, 56.0f, Hash01(Index * 6.617f + Seed, 13.0f)) : FMath::Lerp(6.5f, 22.0f, Hash01(Index * 6.617f + Seed, 13.0f)));
            const FVector Scale = bLargeRock
                ? FVector(ScaleBase * FMath::Lerp(0.7f, 1.8f, Hash01(Index * 9.0f, 1.0f)), ScaleBase, ScaleBase * FMath::Lerp(0.45f, 1.15f, Hash01(Index * 7.0f, 2.0f)))
                : (bCliffFace
                    ? FVector(ScaleBase * FMath::Lerp(0.8f, 1.9f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.45f, 1.2f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.65f, 1.45f, Hash01(Index * 5.0f + Seed, 3.0f)))
                    : FVector(ScaleBase * FMath::Lerp(0.8f, 1.55f, Hash01(Index * 9.0f + Seed, 1.0f)), ScaleBase * FMath::Lerp(0.75f, 1.3f, Hash01(Index * 7.0f + Seed, 2.0f)), ScaleBase * FMath::Lerp(0.75f, 1.8f, Hash01(Index * 5.0f + Seed, 3.0f))));
            const FVector Location(Location2D.X, Location2D.Y, Height + (bLargeRock ? 24.0f : (bCliffFace ? 18.0f : 6.0f)));
            Component->AddInstance(FTransform(SurfaceAlignedRotation(Normal, Yaw), Location, Scale));
        }
    };

    AddScatter(RockOutcrops, RockCount, EScatterKind::Boulder, 0.0f);
    AddScatter(UnderstoryClumps, ClumpCount, EScatterKind::Shrub, 3.0f);
    AddScatter(CliffRockFacesA, CliffFaceCount, EScatterKind::CliffFace, 17.0f);
    AddScatter(CliffRockFacesB, CliffFaceCount, EScatterKind::CliffFace, 31.0f);
    AddScatter(ForestShrubsA, ShrubCount, EScatterKind::Shrub, 43.0f);
    AddScatter(ForestShrubsB, ShrubCount, EScatterKind::Shrub, 59.0f);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung scatter instances: rocks=%d understory=%d cliff_a=%d cliff_b=%d shrubs_a=%d shrubs_b=%d"),
        RockOutcrops->GetInstanceCount(),
        UnderstoryClumps->GetInstanceCount(),
        CliffRockFacesA->GetInstanceCount(),
        CliffRockFacesB->GetInstanceCount(),
        ForestShrubsA->GetInstanceCount(),
        ForestShrubsB->GetInstanceCount());
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
    ApplyTint(RockOutcrops, FLinearColor(0.105f, 0.125f, 0.118f));
    ApplyTint(UnderstoryClumps, FLinearColor(0.035f, 0.16f, 0.055f));
    ApplyTint(CliffRockFacesA, FLinearColor(0.098f, 0.122f, 0.116f));
    ApplyTint(CliffRockFacesB, FLinearColor(0.086f, 0.108f, 0.102f));
    ApplyTint(ForestShrubsA, FLinearColor(0.026f, 0.13f, 0.048f));
    ApplyTint(ForestShrubsB, FLinearColor(0.045f, 0.18f, 0.068f));
}
