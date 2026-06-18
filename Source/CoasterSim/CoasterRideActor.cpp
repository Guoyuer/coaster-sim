#include "CoasterRideActor.h"

#include "Camera/CameraComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float CmPerMeter = 100.0f;
constexpr float GravityCms2 = 980.665f;
constexpr float RiverZCm = 18.0f;

FTransform MakeSegmentTransform(const FVector& Start, const FVector& End, const FVector& ScaleCm)
{
    const FVector Mid = (Start + End) * 0.5f;
    const FVector Delta = End - Start;
    const float Length = FMath::Max(Delta.Length(), 1.0f);
    const FRotator Rotation = FRotationMatrix::MakeFromX(Delta.GetSafeNormal()).Rotator();
    return FTransform(Rotation, Mid, FVector(Length / 100.0f, ScaleCm.Y / 100.0f, ScaleCm.Z / 100.0f));
}

void AddDoubleSidedQuad(TArray<int32>& Triangles, int32 A, int32 B, int32 C, int32 D)
{
    Triangles.Append({ A, C, B, B, C, D, A, B, C, B, D, C });
}

float CanyonLayerNoise(int32 AlongIndex, int32 AcrossIndex, int32 Side)
{
    return 80.0f * FMath::Sin(AlongIndex * 0.61f + AcrossIndex * 1.73f + Side * 0.91f)
        + 45.0f * FMath::Sin(AlongIndex * 1.37f + AcrossIndex * 0.43f);
}
}

ACoasterRideActor::ACoasterRideActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    TrackSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrackSpline"));
    TrackSpline->SetupAttachment(SceneRoot);
    TrackSpline->SetClosedLoop(true);

    TrainRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TrainRoot"));
    TrainRoot->SetupAttachment(SceneRoot);

    TrainBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrainBody"));
    TrainBody->SetupAttachment(TrainRoot);
    TrainBody->SetRelativeScale3D(FVector(1.70f, 0.74f, 0.24f));
    TrainBody->SetRelativeLocation(FVector(124.0f, 0.0f, -72.0f));

    RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
    RideCamera->SetupAttachment(TrainRoot);
    RideCamera->SetRelativeLocation(FVector(-24.0f, 0.0f, 205.0f));
    RideCamera->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
    RideCamera->SetFieldOfView(92.0f);
    RideCamera->bUsePawnControlRotation = false;
    RideCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    RideCamera->PostProcessSettings.MotionBlurAmount = 0.12f;
    RideCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
    RideCamera->PostProcessSettings.VignetteIntensity = 0.16f;
    RideCamera->PostProcessSettings.bOverride_ColorSaturation = true;
    RideCamera->PostProcessSettings.ColorSaturation = FVector4(1.05f, 1.08f, 1.06f, 1.0f);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetIntensity(0.38f);

    ValleyFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ValleyFog"));
    ValleyFog->SetupAttachment(SceneRoot);
    ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, RiverZCm + 70.0f));
    ValleyFog->SetFogDensity(0.00065f);
    ValleyFog->SetFogHeightFalloff(0.32f);
    ValleyFog->SetFogInscatteringColor(FLinearColor(0.40f, 0.48f, 0.50f));

    LeftRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftRail"));
    LeftRail->SetupAttachment(SceneRoot);
    RightRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightRail"));
    RightRail->SetupAttachment(SceneRoot);
    Ties = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ties"));
    Ties->SetupAttachment(SceneRoot);
    Supports = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Supports"));
    Supports->SetupAttachment(SceneRoot);
    CanyonWalls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CanyonWalls"));
    CanyonWalls->SetupAttachment(SceneRoot);
    RiverSurface = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RiverSurface"));
    RiverSurface->SetupAttachment(SceneRoot);
    Rapids = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rapids"));
    Rapids->SetupAttachment(SceneRoot);
    RiverRocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RiverRocks"));
    RiverRocks->SetupAttachment(SceneRoot);
    ForestPatches = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ForestPatches"));
    ForestPatches->SetupAttachment(SceneRoot);
    MistBands = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MistBands"));
    MistBands->SetupAttachment(SceneRoot);
    CanyonTerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CanyonTerrainMesh"));
    CanyonTerrainMesh->SetupAttachment(SceneRoot);
    CanyonTerrainMesh->bUseAsyncCooking = true;
    RiverRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RiverRibbonMesh"));
    RiverRibbonMesh->SetupAttachment(SceneRoot);
    RiverRibbonMesh->bUseAsyncCooking = true;
    FoamRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FoamRibbonMesh"));
    FoamRibbonMesh->SetupAttachment(SceneRoot);
    FoamRibbonMesh->bUseAsyncCooking = true;

    CanyonWalls->SetCastShadow(false);
    RiverSurface->SetCastShadow(false);
    Rapids->SetCastShadow(false);
    RiverRocks->SetCastShadow(false);
    ForestPatches->SetCastShadow(false);
    MistBands->SetCastShadow(false);
    CanyonTerrainMesh->SetCastShadow(false);
    RiverRibbonMesh->SetCastShadow(false);
    FoamRibbonMesh->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        TrainBody->SetStaticMesh(CubeMesh.Object);
        LeftRail->SetStaticMesh(CubeMesh.Object);
        RightRail->SetStaticMesh(CubeMesh.Object);
        Ties->SetStaticMesh(CubeMesh.Object);
        Supports->SetStaticMesh(CubeMesh.Object);
        CanyonWalls->SetStaticMesh(CubeMesh.Object);
        RiverSurface->SetStaticMesh(CubeMesh.Object);
        Rapids->SetStaticMesh(CubeMesh.Object);
        RiverRocks->SetStaticMesh(CubeMesh.Object);
        ForestPatches->SetStaticMesh(CubeMesh.Object);
        MistBands->SetStaticMesh(CubeMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        TrainBody->SetMaterial(0, BasicMaterial.Object);
        LeftRail->SetMaterial(0, BasicMaterial.Object);
        RightRail->SetMaterial(0, BasicMaterial.Object);
        Ties->SetMaterial(0, BasicMaterial.Object);
        Supports->SetMaterial(0, BasicMaterial.Object);
        CanyonWalls->SetMaterial(0, BasicMaterial.Object);
        RiverSurface->SetMaterial(0, BasicMaterial.Object);
        Rapids->SetMaterial(0, BasicMaterial.Object);
        RiverRocks->SetMaterial(0, BasicMaterial.Object);
        ForestPatches->SetMaterial(0, BasicMaterial.Object);
        MistBands->SetMaterial(0, BasicMaterial.Object);
        CanyonTerrainMesh->SetMaterial(0, BasicMaterial.Object);
        RiverRibbonMesh->SetMaterial(0, BasicMaterial.Object);
        FoamRibbonMesh->SetMaterial(0, BasicMaterial.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterial(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
    if (VertexColorMaterial.Succeeded())
    {
        CanyonTerrainMesh->SetMaterial(0, VertexColorMaterial.Object);
        RiverRibbonMesh->SetMaterial(0, VertexColorMaterial.Object);
        FoamRibbonMesh->SetMaterial(0, VertexColorMaterial.Object);
    }

    CurrentSpeedCms = 450.0f;
}

void ACoasterRideActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureDefaultTrack();
    RebuildSpline();
    ApplyVisualMaterials();
    RebuildEnvironment();
    RebuildVisuals();
}

void ACoasterRideActor::BeginPlay()
{
    Super::BeginPlay();
    EnsureDefaultTrack();
    RebuildSpline();
    StartRideAt(0.34f, 18.0f);
}

void ACoasterRideActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AdvanceRide(DeltaSeconds);
}

void ACoasterRideActor::EnsureDefaultTrack()
{
    const bool bLooksLikeLegacyDefault =
        ControlPoints.Num() == 11 &&
        ControlPoints[0].Equals(FVector(0.0f, 0.0f, 350.0f), 2.0f) &&
        ControlPoints[1].Equals(FVector(900.0f, 0.0f, 1150.0f), 2.0f);

    if (!ControlPoints.IsEmpty() && !bLooksLikeLegacyDefault)
    {
        return;
    }

    ControlPoints = {
        FVector(0.0f, 0.0f, 420.0f),
        FVector(1800.0f, 0.0f, 560.0f),
        FVector(4200.0f, 280.0f, 1800.0f),
        FVector(6800.0f, 620.0f, 3550.0f),
        FVector(9300.0f, 260.0f, 1250.0f),
        FVector(10400.0f, -1800.0f, 520.0f),
        FVector(7700.0f, -3650.0f, 1500.0f),
        FVector(4100.0f, -3900.0f, 900.0f),
        FVector(600.0f, -2500.0f, 1850.0f),
        FVector(-1800.0f, 300.0f, 780.0f),
        FVector(-850.0f, 1500.0f, 460.0f)
    };
}

void ACoasterRideActor::StartRideAt(float TrackRatio, float SpeedMps)
{
    EnsureDefaultTrack();
    RebuildSpline();

    const float ClampedRatio = FMath::Clamp(TrackRatio, 0.0f, 0.999f);
    CurrentDistanceCm = TrackLengthCm * ClampedRatio;
    CurrentSpeedCms = FMath::Max(SpeedMps, 1.8f) * CmPerMeter;
    LastVelocityCms = FVector::ZeroVector;

    FVector Location;
    FVector Forward;
    FVector Right;
    FVector Up;
    FRotator Rotation;
    SampleFrame(CurrentDistanceCm, Location, Rotation, Forward, Right, Up);
    TrainRoot->SetRelativeLocationAndRotation(Location, Rotation);
    UpdateFirstPersonCamera();
}

void ACoasterRideActor::RebuildSpline()
{
    TrackSpline->ClearSplinePoints(false);

    for (int32 Index = 0; Index < ControlPoints.Num(); ++Index)
    {
        TrackSpline->AddSplinePoint(ControlPoints[Index], ESplineCoordinateSpace::Local, false);
        TrackSpline->SetSplinePointType(Index, ESplinePointType::Curve, false);
    }

    TrackSpline->SetClosedLoop(true, false);
    TrackSpline->UpdateSpline();
    TrackLengthCm = FMath::Max(TrackSpline->GetSplineLength(), 1.0f);
}

void ACoasterRideActor::RebuildVisuals()
{
    LeftRail->ClearInstances();
    RightRail->ClearInstances();
    Ties->ClearInstances();
    Supports->ClearInstances();

    const float RailHalfGauge = RailGaugeCm * 0.5f;
    const float SegmentStep = 180.0f;
    const float TieStep = 360.0f;
    const float SupportStep = 720.0f;

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += SegmentStep)
    {
        FVector LocationA;
        FVector ForwardA;
        FVector RightA;
        FVector UpA;
        FRotator RotationA;
        SampleFrame(Distance, LocationA, RotationA, ForwardA, RightA, UpA);

        FVector LocationB;
        FVector ForwardB;
        FVector RightB;
        FVector UpB;
        FRotator RotationB;
        SampleFrame(FMath::Fmod(Distance + SegmentStep, TrackLengthCm), LocationB, RotationB, ForwardB, RightB, UpB);

        const FVector RailDropA = UpA * 18.0f;
        const FVector RailDropB = UpB * 18.0f;
        LeftRail->AddInstance(MakeSegmentTransform(LocationA - RightA * RailHalfGauge - RailDropA, LocationB - RightB * RailHalfGauge - RailDropB, FVector(SegmentStep, 9.0f, 9.0f)));
        RightRail->AddInstance(MakeSegmentTransform(LocationA + RightA * RailHalfGauge - RailDropA, LocationB + RightB * RailHalfGauge - RailDropB, FVector(SegmentStep, 9.0f, 9.0f)));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += TieStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector TieCenter = Location - Up * 42.0f;
        const FVector TieStart = TieCenter - Right * (RailHalfGauge + 28.0f);
        const FVector TieEnd = TieCenter + Right * (RailHalfGauge + 28.0f);
        Ties->AddInstance(MakeSegmentTransform(TieStart, TieEnd, FVector(RailGaugeCm + 56.0f, 14.0f, 9.0f)));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += SupportStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        if (Location.Z > 250.0f)
        {
            const FVector YokeCenter = Location - Up * 62.0f;
            const FVector YokeLeft = YokeCenter - Right * (RailHalfGauge + 34.0f);
            const FVector YokeRight = YokeCenter + Right * (RailHalfGauge + 34.0f);
            const FVector LeftFoot = FVector(YokeLeft.X, YokeLeft.Y, RiverZCm - 35.0f);
            const FVector RightFoot = FVector(YokeRight.X, YokeRight.Y, RiverZCm - 35.0f);

            Supports->AddInstance(MakeSegmentTransform(YokeLeft, YokeRight, FVector(RailGaugeCm + 68.0f, 10.0f, 10.0f)));
            Supports->AddInstance(MakeSegmentTransform(LeftFoot, YokeLeft, FVector(YokeLeft.Z, 9.0f, 9.0f)));
            Supports->AddInstance(MakeSegmentTransform(RightFoot, YokeRight, FVector(YokeRight.Z, 9.0f, 9.0f)));

            if (YokeCenter.Z > 850.0f)
            {
                Supports->AddInstance(MakeSegmentTransform(LeftFoot, YokeRight, FVector(YokeRight.Z, 6.0f, 6.0f)));
                Supports->AddInstance(MakeSegmentTransform(RightFoot, YokeLeft, FVector(YokeLeft.Z, 6.0f, 6.0f)));
            }
        }
    }
}

void ACoasterRideActor::RebuildEnvironment()
{
    CanyonWalls->ClearInstances();
    RiverSurface->ClearInstances();
    Rapids->ClearInstances();
    RiverRocks->ClearInstances();
    ForestPatches->ClearInstances();
    MistBands->ClearInstances();
    CanyonTerrainMesh->ClearAllMeshSections();
    RiverRibbonMesh->ClearAllMeshSections();
    FoamRibbonMesh->ClearAllMeshSections();

    const float RiverHalfWidth = 2800.0f;
    const float SegmentStep = 520.0f;
    const int32 SampleCount = FMath::Max(FMath::CeilToInt(TrackLengthCm / SegmentStep), 16);

    struct FRiverSample
    {
        FVector Center = FVector::ZeroVector;
        FVector Forward = FVector::ForwardVector;
        FVector Right = FVector::RightVector;
        float Ratio = 0.0f;
    };

    TArray<FRiverSample> Samples;
    Samples.Reserve(SampleCount + 1);

    for (int32 Index = 0; Index <= SampleCount; ++Index)
    {
        const float TrackRatio = static_cast<float>(Index) / static_cast<float>(SampleCount);
        const float Distance = TrackLengthCm * TrackRatio;
        FVector LocationA;
        FVector ForwardA;
        FVector RightA;
        FVector UpA;
        FRotator RotationA;
        SampleFrame(Distance, LocationA, RotationA, ForwardA, RightA, UpA);

        FVector EnvForwardA(ForwardA.X, ForwardA.Y, 0.0f);
        if (EnvForwardA.IsNearlyZero())
        {
            EnvForwardA = FVector::ForwardVector;
        }
        EnvForwardA.Normalize();

        const FVector EnvRightA = FVector::CrossProduct(FVector::UpVector, EnvForwardA).GetSafeNormal();
        const float Bend = FMath::Sin(TrackRatio * UE_TWO_PI * 2.0f) * 90.0f;
        FRiverSample& Sample = Samples.AddDefaulted_GetRef();
        Sample.Center = FVector(LocationA.X, LocationA.Y + Bend, RiverZCm);
        Sample.Forward = EnvForwardA;
        Sample.Right = EnvRightA;
        Sample.Ratio = TrackRatio;
    }

    TArray<FVector> TerrainVertices;
    TArray<int32> TerrainTriangles;
    TArray<FVector> TerrainNormals;
    TArray<FVector2D> TerrainUVs;
    TArray<FLinearColor> TerrainColors;
    TArray<FProcMeshTangent> TerrainTangents;
    const TArray<float> TerrainOffsets = {
        RiverHalfWidth + 11000.0f,
        RiverHalfWidth + 15500.0f,
        RiverHalfWidth + 21000.0f,
        RiverHalfWidth + 28000.0f,
        RiverHalfWidth + 36000.0f,
        RiverHalfWidth + 46000.0f
    };
    const TArray<float> TerrainHeights = { 120.0f, 280.0f, 620.0f, 1100.0f, 1780.0f, 2600.0f };

    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        const int32 SideBase = TerrainVertices.Num();
        for (int32 Along = 0; Along < Samples.Num(); ++Along)
        {
            const FRiverSample& Sample = Samples[Along];
            for (int32 Across = 0; Across < TerrainOffsets.Num(); ++Across)
            {
                const float WindCarve = 90.0f * FMath::Sin(Sample.Ratio * UE_TWO_PI * 3.0f + Across * 0.7f);
                const float Height = TerrainHeights[Across] + CanyonLayerNoise(Along, Across, Side);
                TerrainVertices.Add(Sample.Center + Sample.Right * (Side * (TerrainOffsets[Across] + WindCarve)) + FVector(0.0f, 0.0f, Height));
                TerrainNormals.Add(FVector::UpVector);
                TerrainUVs.Add(FVector2D(Sample.Ratio * 12.0f, Across * 0.75f));
                const float ColorT = static_cast<float>(Across) / static_cast<float>(TerrainOffsets.Num() - 1);
                TerrainColors.Add(FLinearColor(
                    FMath::Lerp(0.24f, 0.12f, ColorT),
                    FMath::Lerp(0.22f, 0.14f, ColorT),
                    FMath::Lerp(0.18f, 0.13f, ColorT),
                    1.0f));
                TerrainTangents.Add(FProcMeshTangent(Sample.Forward, false));
            }
        }

        for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
        {
            for (int32 Across = 0; Across < TerrainOffsets.Num() - 1; ++Across)
            {
                const int32 A = SideBase + Along * TerrainOffsets.Num() + Across;
                const int32 B = SideBase + (Along + 1) * TerrainOffsets.Num() + Across;
                const int32 C = SideBase + Along * TerrainOffsets.Num() + Across + 1;
                const int32 D = SideBase + (Along + 1) * TerrainOffsets.Num() + Across + 1;
                AddDoubleSidedQuad(TerrainTriangles, A, B, C, D);
            }
        }
    }

    CanyonTerrainMesh->CreateMeshSection_LinearColor(0, TerrainVertices, TerrainTriangles, TerrainNormals, TerrainUVs, TerrainColors, TerrainTangents, false);

    TArray<FVector> RiverVertices;
    TArray<int32> RiverTriangles;
    TArray<FVector> RiverNormals;
    TArray<FVector2D> RiverUVs;
    TArray<FLinearColor> RiverColors;
    TArray<FProcMeshTangent> RiverTangents;
    const TArray<float> RiverAcross = { -1.0f, -0.52f, 0.0f, 0.52f, 1.0f };
    for (int32 Along = 0; Along < Samples.Num(); ++Along)
    {
        const FRiverSample& Sample = Samples[Along];
        for (int32 Across = 0; Across < RiverAcross.Num(); ++Across)
        {
            const float Wave = 4.0f * FMath::Sin(Along * 0.85f + Across * 1.7f);
            RiverVertices.Add(Sample.Center + Sample.Right * (RiverAcross[Across] * RiverHalfWidth) + FVector(0.0f, 0.0f, Wave));
            RiverNormals.Add(FVector::UpVector);
            RiverUVs.Add(FVector2D(Sample.Ratio * 18.0f, RiverAcross[Across] * 0.5f + 0.5f));
            const float CenterWeight = 1.0f - FMath::Abs(RiverAcross[Across]);
            RiverColors.Add(FLinearColor(
                0.0f,
                0.40f + CenterWeight * 0.14f,
                0.52f + CenterWeight * 0.22f,
                1.0f));
            RiverTangents.Add(FProcMeshTangent(Sample.Forward, false));
        }
    }

    for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
    {
        for (int32 Across = 0; Across < RiverAcross.Num() - 1; ++Across)
        {
            const int32 A = Along * RiverAcross.Num() + Across;
            const int32 B = (Along + 1) * RiverAcross.Num() + Across;
            const int32 C = Along * RiverAcross.Num() + Across + 1;
            const int32 D = (Along + 1) * RiverAcross.Num() + Across + 1;
            AddDoubleSidedQuad(RiverTriangles, A, B, C, D);
        }
    }

    RiverRibbonMesh->CreateMeshSection_LinearColor(0, RiverVertices, RiverTriangles, RiverNormals, RiverUVs, RiverColors, RiverTangents, false);

    TArray<FVector> FoamVertices;
    TArray<int32> FoamTriangles;
    TArray<FVector> FoamNormals;
    TArray<FVector2D> FoamUVs;
    TArray<FLinearColor> FoamColors;
    TArray<FProcMeshTangent> FoamTangents;
    const TArray<float> FoamLanes = { -0.58f, -0.18f, 0.25f, 0.62f };
    for (int32 Lane = 0; Lane < FoamLanes.Num(); ++Lane)
    {
        for (int32 Along = 0; Along < Samples.Num(); ++Along)
        {
            const FRiverSample& Sample = Samples[Along];
            const float Lateral = (FoamLanes[Lane] * RiverHalfWidth) + 120.0f * FMath::Sin(Along * 0.9f + Lane);
            const float Width = 46.0f + 22.0f * FMath::Sin(Along * 0.51f + Lane * 1.4f);
            const FVector FoamCenter = Sample.Center + Sample.Right * Lateral + FVector(0.0f, 0.0f, 16.0f);
            FoamVertices.Add(FoamCenter - Sample.Right * Width);
            FoamVertices.Add(FoamCenter + Sample.Right * Width);
            FoamNormals.Add(FVector::UpVector);
            FoamNormals.Add(FVector::UpVector);
            FoamUVs.Add(FVector2D(Sample.Ratio * 24.0f, 0.0f));
            FoamUVs.Add(FVector2D(Sample.Ratio * 24.0f, 1.0f));
            FoamColors.Add(FLinearColor(0.76f, 0.93f, 0.90f, 1.0f));
            FoamColors.Add(FLinearColor(0.88f, 0.98f, 0.95f, 1.0f));
            FoamTangents.Add(FProcMeshTangent(Sample.Forward, false));
            FoamTangents.Add(FProcMeshTangent(Sample.Forward, false));
        }

        const int32 LaneBase = Lane * Samples.Num() * 2;
        for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
        {
            const int32 A = LaneBase + Along * 2;
            const int32 B = LaneBase + (Along + 1) * 2;
            const int32 C = LaneBase + Along * 2 + 1;
            const int32 D = LaneBase + (Along + 1) * 2 + 1;
            AddDoubleSidedQuad(FoamTriangles, A, B, C, D);
        }
    }

    FoamRibbonMesh->CreateMeshSection_LinearColor(0, FoamVertices, FoamTriangles, FoamNormals, FoamUVs, FoamColors, FoamTangents, false);

    for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
    {
        const FRiverSample& SampleA = Samples[Along];
        const FRiverSample& SampleB = Samples[Along + 1];
        RiverSurface->AddInstance(MakeSegmentTransform(SampleA.Center, SampleB.Center, FVector(SegmentStep, RiverHalfWidth * 4.2f, 6.0f)));

        if (Along % 2 == 0)
        {
            const FVector FoamStart = (SampleA.Center + SampleB.Center) * 0.5f - SampleA.Right * (RiverHalfWidth * 0.56f) + FVector(0.0f, 0.0f, 18.0f);
            const FVector FoamEnd = (SampleA.Center + SampleB.Center) * 0.5f + SampleA.Right * (RiverHalfWidth * 0.56f) + FVector(0.0f, 0.0f, 18.0f);
            Rapids->AddInstance(MakeSegmentTransform(FoamStart, FoamEnd, FVector(RiverHalfWidth * 0.82f, 5.0f, 3.0f)));
        }

        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const FVector BankA = SampleA.Center + SampleA.Right * (Side * (RiverHalfWidth + 140.0f)) + FVector(0.0f, 0.0f, 24.0f);
            const FVector BankB = SampleB.Center + SampleB.Right * (Side * (RiverHalfWidth + 190.0f)) + FVector(0.0f, 0.0f, 30.0f);
            RiverRocks->AddInstance(MakeSegmentTransform(BankA, BankB, FVector(SegmentStep, 320.0f, 34.0f)));

        }

        if (Along % 3 == 0)
        {
            const FVector RockCenter = (SampleA.Center + SampleB.Center) * 0.5f + SampleA.Right * (FMath::Sin(SampleA.Ratio * UE_TWO_PI * 9.0f) * RiverHalfWidth * 0.5f) + FVector(0.0f, 0.0f, 42.0f);
            const FVector RockEnd = RockCenter + SampleA.Forward * 120.0f + SampleA.Right * 40.0f;
            RiverRocks->AddInstance(MakeSegmentTransform(RockCenter, RockEnd, FVector(140.0f, 70.0f, 44.0f)));
        }
    }
}

void ACoasterRideActor::ApplyVisualMaterials()
{
    auto TintComponent = [](UMeshComponent* Component, const FLinearColor& Color)
    {
        if (!Component)
        {
            return;
        }

        UMaterialInstanceDynamic* Material = Component->CreateAndSetMaterialInstanceDynamic(0);
        if (!Material)
        {
            return;
        }

        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    };

    TintComponent(TrainBody, FLinearColor(0.85f, 0.08f, 0.04f));
    TintComponent(LeftRail, FLinearColor(0.55f, 0.57f, 0.56f));
    TintComponent(RightRail, FLinearColor(0.55f, 0.57f, 0.56f));
    TintComponent(Ties, FLinearColor(0.22f, 0.18f, 0.13f));
    TintComponent(Supports, FLinearColor(0.38f, 0.42f, 0.44f));
    TintComponent(CanyonWalls, FLinearColor(0.27f, 0.25f, 0.21f));
    TintComponent(RiverSurface, FLinearColor(0.01f, 0.36f, 0.42f));
    TintComponent(Rapids, FLinearColor(0.78f, 0.92f, 0.88f));
    TintComponent(RiverRocks, FLinearColor(0.42f, 0.39f, 0.34f));
    TintComponent(ForestPatches, FLinearColor(0.02f, 0.11f, 0.04f));
    TintComponent(MistBands, FLinearColor(0.78f, 0.88f, 0.84f));
}

void ACoasterRideActor::AdvanceRide(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f || TrackLengthCm <= 1.0f)
    {
        return;
    }

    FVector Location;
    FVector Forward;
    FVector Right;
    FVector Up;
    FRotator Rotation;
    SampleFrame(CurrentDistanceCm, Location, Rotation, Forward, Right, Up);

    const float TrackRatio = CurrentDistanceCm / TrackLengthCm;
    const FName SectionName = GetSectionName(TrackRatio);
    const float GravityAccel = FVector::DotProduct(FVector(0.0f, 0.0f, -GravityCms2), Forward);
    const float DragAccel = 0.000015f * CurrentSpeedCms * CurrentSpeedCms;
    const float RollingAccel = 18.0f;

    float DriveAccel = 0.0f;
    float BrakeAccel = 0.0f;

    if (SectionName == TEXT("Lift"))
    {
        const float Target = LiftTargetSpeedMps * CmPerMeter;
        DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 2.0f, 0.0f, 980.0f);
    }
    else if (SectionName == TEXT("Launch"))
    {
        const float Target = LaunchTargetSpeedMps * CmPerMeter;
        DriveAccel = CurrentSpeedCms < Target ? 720.0f : 0.0f;
    }
    else if (SectionName == TEXT("Brake"))
    {
        const float Target = BrakeTargetSpeedMps * CmPerMeter;
        BrakeAccel = CurrentSpeedCms > Target ? FMath::Min((CurrentSpeedCms - Target) * 1.1f, 850.0f) : 0.0f;
    }
    else if (SectionName == TEXT("Station"))
    {
        const float Target = 4.0f * CmPerMeter;
        DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 1.0f, -200.0f, 180.0f);
    }

    const float NetAccel = DriveAccel + GravityAccel - DragAccel - RollingAccel - BrakeAccel;
    CurrentSpeedCms = FMath::Clamp(CurrentSpeedCms + NetAccel * DeltaSeconds, 180.0f, 5600.0f);
    CurrentDistanceCm = FMath::Fmod(CurrentDistanceCm + CurrentSpeedCms * DeltaSeconds, TrackLengthCm);

    FVector NewLocation;
    FVector NewForward;
    FVector NewRight;
    FVector NewUp;
    FRotator NewRotation;
    SampleFrame(CurrentDistanceCm, NewLocation, NewRotation, NewForward, NewRight, NewUp);
    TrainRoot->SetRelativeLocationAndRotation(NewLocation, NewRotation);
    UpdateFirstPersonCamera();

    const FVector VelocityCms = NewForward * CurrentSpeedCms;
    const FVector AccelWorldCms2 = (VelocityCms - LastVelocityCms) / DeltaSeconds;
    LastVelocityCms = VelocityCms;

    const FVector SeatForceCms2 = AccelWorldCms2 - FVector(0.0f, 0.0f, -GravityCms2);
    Telemetry.SpeedMps = CurrentSpeedCms / CmPerMeter;
    Telemetry.HeightMeters = NewLocation.Z / CmPerMeter;
    Telemetry.TrackDistanceMeters = CurrentDistanceCm / CmPerMeter;
    Telemetry.VerticalG = FVector::DotProduct(SeatForceCms2, NewUp) / GravityCms2;
    Telemetry.LateralG = FVector::DotProduct(SeatForceCms2, NewRight) / GravityCms2;
    Telemetry.LongitudinalG = FVector::DotProduct(SeatForceCms2, NewForward) / GravityCms2;
    Telemetry.SectionName = SectionName;
}

void ACoasterRideActor::UpdateFirstPersonCamera()
{
    RideCamera->SetRelativeLocation(FVector(-24.0f, 0.0f, 205.0f));
    RideCamera->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
}

void ACoasterRideActor::SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const
{
    const float WrappedDistance = FMath::Fmod(FMath::Max(DistanceCm, 0.0f), TrackLengthCm);
    OutLocation = TrackSpline->GetLocationAtDistanceAlongSpline(WrappedDistance, ESplineCoordinateSpace::Local);
    OutForward = TrackSpline->GetDirectionAtDistanceAlongSpline(WrappedDistance, ESplineCoordinateSpace::Local).GetSafeNormal();

    OutRight = FVector::CrossProduct(FVector::UpVector, OutForward).GetSafeNormal();
    if (OutRight.IsNearlyZero())
    {
        OutRight = FVector::RightVector;
    }

    OutUp = FVector::CrossProduct(OutForward, OutRight).GetSafeNormal();

    const float TrackRatio = WrappedDistance / TrackLengthCm;
    const float BankRadians = FMath::DegreesToRadians(28.0f * FMath::Sin(TrackRatio * UE_TWO_PI * 3.0f));
    const FQuat BankQuat(OutForward, BankRadians);
    OutRight = BankQuat.RotateVector(OutRight).GetSafeNormal();
    OutUp = BankQuat.RotateVector(OutUp).GetSafeNormal();

    OutRotation = FRotationMatrix::MakeFromXZ(OutForward, OutUp).Rotator();
}

FName ACoasterRideActor::GetSectionName(float TrackRatio) const
{
    if (TrackRatio < 0.04f)
    {
        return TEXT("Station");
    }
    if (TrackRatio < 0.24f)
    {
        return TEXT("Lift");
    }
    if (TrackRatio > 0.56f && TrackRatio < 0.62f)
    {
        return TEXT("Launch");
    }
    if (TrackRatio > 0.88f)
    {
        return TEXT("Brake");
    }
    return TEXT("Coast");
}
