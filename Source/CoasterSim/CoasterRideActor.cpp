#include "CoasterRideActor.h"

#include "YarlungCoasterProfile.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Landscape.h"
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

using YarlungCoaster::Smooth01;

float YarlungRiverCenterY(float X)
{
    return -1150.0f
        + 1050.0f * FMath::Sin(X * 0.00048f + 0.7f)
        + 420.0f * FMath::Sin(X * 0.00115f - 0.6f);
}

float YarlungTerrainHeight(float X, float Y)
{
    const float RiverY = YarlungRiverCenterY(X);
    const float Lateral = FMath::Abs(Y - RiverY);
    const float WideValley = Smooth01((Lateral - 1150.0f) / 9400.0f);
    const float OuterMountain = Smooth01((Lateral - 5800.0f) / 7600.0f);
    const float CliffBand = Smooth01((Lateral - 3100.0f) / 4200.0f);
    const float LongRidge = 155.0f * FMath::Sin(X * 0.00075f + Y * 0.00018f);
    const float FoldNoise = 82.0f * FMath::Sin(X * 0.0018f - Y * 0.00072f)
        + 46.0f * FMath::Sin(X * 0.0034f + Y * 0.0011f);
    const float Terraces = 58.0f * FMath::Sin((Lateral - 1200.0f) * 0.0018f + X * 0.00042f);

    float Height = RiverZCm - 22.0f
        + WideValley * 520.0f
        + CliffBand * 1420.0f
        + OuterMountain * 1900.0f
        + LongRidge + FoldNoise + Terraces;

    if (Lateral < 980.0f)
    {
        const float Channel = Smooth01(Lateral / 980.0f);
        Height = FMath::Lerp(RiverZCm - 46.0f, RiverZCm + 34.0f, Channel);
    }

    return YarlungCoaster::ApplyTrackClearanceCut(X, Y, Height);
}

float YarlungForestAmount(float X, float Y, float Height)
{
    const float Lateral = FMath::Abs(Y - YarlungRiverCenterY(X));
    const float ForestBand = Smooth01((Lateral - 3300.0f) / 1300.0f) * (1.0f - Smooth01((Lateral - 7800.0f) / 1500.0f));
    const float ForestHeight = 1.0f - Smooth01((Height - 1650.0f) / 550.0f);
    const float PatchNoise = 0.5f
        + 0.32f * FMath::Sin(X * 0.0021f + Y * 0.0013f)
        + 0.18f * FMath::Sin(X * 0.0053f - Y * 0.0031f);
    return FMath::Clamp(ForestBand * ForestHeight * Smooth01((PatchNoise - 0.28f) / 0.42f), 0.0f, 1.0f);
}

FLinearColor YarlungTerrainColor(float X, float Y, float Height)
{
    const float Lateral = FMath::Abs(Y - YarlungRiverCenterY(X));
    if (Height > 2860.0f)
    {
        const float SnowT = Smooth01((Height - 2860.0f) / 520.0f);
        return FLinearColor(
            FMath::Lerp(0.46f, 0.78f, SnowT),
            FMath::Lerp(0.47f, 0.82f, SnowT),
            FMath::Lerp(0.43f, 0.80f, SnowT),
            1.0f);
    }
    if (Lateral < 2350.0f)
    {
        return FLinearColor(0.27f, 0.28f, 0.23f, 1.0f);
    }

    const float ForestAmount = YarlungForestAmount(X, Y, Height);
    if (ForestAmount > 0.08f)
    {
        const FLinearColor RockBase(0.34f, 0.31f, 0.23f, 1.0f);
        const FLinearColor ForestBase(0.065f, 0.16f, 0.055f, 1.0f);
        return FMath::Lerp(RockBase, ForestBase, ForestAmount);
    }
    const float RockT = Smooth01((Height - 450.0f) / 2200.0f);
    return FLinearColor(
        FMath::Lerp(0.28f, 0.24f, RockT),
        FMath::Lerp(0.29f, 0.28f, RockT),
        FMath::Lerp(0.25f, 0.23f, RockT),
        1.0f);
}

float Hash01(float A, float B)
{
    return FMath::Frac(FMath::Sin(A * 12.9898f + B * 78.233f) * 43758.5453f);
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
    TrainBody->SetRelativeScale3D(FVector(1.22f, 0.60f, 0.14f));
    TrainBody->SetRelativeLocation(FVector(155.0f, 0.0f, -126.0f));

    RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
    RideCamera->SetupAttachment(TrainRoot);
    RideCamera->SetRelativeLocation(FVector(-64.0f, 0.0f, 248.0f));
    RideCamera->SetRelativeRotation(FRotator(-9.0f, 0.0f, 0.0f));
    RideCamera->SetFieldOfView(82.0f);
    RideCamera->bUsePawnControlRotation = false;
    RideCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    RideCamera->PostProcessSettings.MotionBlurAmount = 0.16f;
    RideCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
    RideCamera->PostProcessSettings.VignetteIntensity = 0.18f;
    RideCamera->PostProcessSettings.bOverride_ColorSaturation = true;
    RideCamera->PostProcessSettings.ColorSaturation = FVector4(0.82f, 0.90f, 0.86f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorContrast = true;
    RideCamera->PostProcessSettings.ColorContrast = FVector4(1.08f, 1.07f, 1.04f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorGamma = true;
    RideCamera->PostProcessSettings.ColorGamma = FVector4(0.96f, 0.98f, 1.02f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorGain = true;
    RideCamera->PostProcessSettings.ColorGain = FVector4(0.94f, 0.98f, 1.04f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_WhiteTemp = true;
    RideCamera->PostProcessSettings.WhiteTemp = 6300.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    RideCamera->PostProcessSettings.AutoExposureMinBrightness = 0.72f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    RideCamera->PostProcessSettings.AutoExposureMaxBrightness = 0.72f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
    RideCamera->PostProcessSettings.AutoExposureBias = -0.18f;
    RideCamera->PostProcessSettings.bOverride_FilmSlope = true;
    RideCamera->PostProcessSettings.FilmSlope = 0.78f;
    RideCamera->PostProcessSettings.bOverride_FilmToe = true;
    RideCamera->PostProcessSettings.FilmToe = 0.36f;
    RideCamera->PostProcessSettings.bOverride_FilmShoulder = true;
    RideCamera->PostProcessSettings.FilmShoulder = 0.34f;
    RideCamera->PostProcessSettings.bOverride_FilmGrainIntensity = true;
    RideCamera->PostProcessSettings.FilmGrainIntensity = 0.035f;
    RideCamera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
    RideCamera->PostProcessSettings.SceneFringeIntensity = 0.05f;
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldEnabled = true;
    RideCamera->PostProcessSettings.DepthOfFieldEnabled = true;
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
    RideCamera->PostProcessSettings.DepthOfFieldFstop = 7.5f;
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    RideCamera->PostProcessSettings.DepthOfFieldFocalDistance = 4200.0f;

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetIntensity(1.05f);

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);
    SunLight->SetRelativeRotation(FRotator(-46.0f, -24.0f, 0.0f));
    SunLight->SetIntensity(12.5f);
    SunLight->SetTemperature(6100.0f);
    SunLight->SetLightColor(FLinearColor(1.0f, 0.96f, 0.88f));
    SunLight->SetAtmosphereSunLight(true);
    SunLight->SetAtmosphereSunLightIndex(0);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(SceneRoot);
    SkyAtmosphere->SetRayleighScatteringScale(1.45f);
    SkyAtmosphere->SetMieScatteringScale(0.012f);
    SkyAtmosphere->SetAerialPerspectiveStartDepth(3400.0f);

    ValleyFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ValleyFog"));
    ValleyFog->SetupAttachment(SceneRoot);
    ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, RiverZCm + 70.0f));
    ValleyFog->SetFogDensity(0.00022f);
    ValleyFog->SetFogHeightFalloff(0.34f);
    ValleyFog->SetFogMaxOpacity(0.24f);
    ValleyFog->SetStartDistance(1800.0f);
    ValleyFog->SetFogInscatteringColor(FLinearColor(0.58f, 0.68f, 0.76f));
    ValleyFog->SetVolumetricFog(true);
    ValleyFog->SetVolumetricFogScatteringDistribution(0.28f);
    ValleyFog->SetVolumetricFogExtinctionScale(0.18f);
    ValleyFog->SetVolumetricFogDistance(16000.0f);

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
    SandBars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SandBars"));
    SandBars->SetupAttachment(SceneRoot);
    SnowCaps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SnowCaps"));
    SnowCaps->SetupAttachment(SceneRoot);
    CanyonTerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CanyonTerrainMesh"));
    CanyonTerrainMesh->SetupAttachment(SceneRoot);
    CanyonTerrainMesh->bUseAsyncCooking = true;
    RiverRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RiverRibbonMesh"));
    RiverRibbonMesh->SetupAttachment(SceneRoot);
    RiverRibbonMesh->bUseAsyncCooking = true;
    FoamRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FoamRibbonMesh"));
    FoamRibbonMesh->SetupAttachment(SceneRoot);
    FoamRibbonMesh->bUseAsyncCooking = true;
    SkyDomeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SkyDomeMesh"));
    SkyDomeMesh->SetupAttachment(SceneRoot);
    SkyDomeMesh->bUseAsyncCooking = true;
    CloudLayerMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CloudLayerMesh"));
    CloudLayerMesh->SetupAttachment(SceneRoot);
    CloudLayerMesh->bUseAsyncCooking = true;

    CanyonWalls->SetCastShadow(true);
    RiverSurface->SetCastShadow(false);
    Rapids->SetCastShadow(false);
    RiverRocks->SetCastShadow(true);
    ForestPatches->SetCastShadow(true);
    MistBands->SetCastShadow(false);
    SandBars->SetCastShadow(true);
    SnowCaps->SetCastShadow(true);
    CanyonTerrainMesh->SetCastShadow(false);
    RiverRibbonMesh->SetCastShadow(false);
    FoamRibbonMesh->SetCastShadow(false);
    SkyDomeMesh->SetCastShadow(false);
    CloudLayerMesh->SetCastShadow(false);

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
        SandBars->SetStaticMesh(CubeMesh.Object);
        SnowCaps->SetStaticMesh(CubeMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        CanyonWalls->SetStaticMesh(SphereMesh.Object);
        RiverRocks->SetStaticMesh(SphereMesh.Object);
        SnowCaps->SetStaticMesh(SphereMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (ConeMesh.Succeeded())
    {
        ForestPatches->SetStaticMesh(ConeMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
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
        SandBars->SetMaterial(0, BasicMaterial.Object);
        SnowCaps->SetMaterial(0, BasicMaterial.Object);
        CanyonTerrainMesh->SetMaterial(0, BasicMaterial.Object);
        RiverRibbonMesh->SetMaterial(0, BasicMaterial.Object);
        FoamRibbonMesh->SetMaterial(0, BasicMaterial.Object);
        SkyDomeMesh->SetMaterial(0, BasicMaterial.Object);
        CloudLayerMesh->SetMaterial(0, BasicMaterial.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterial(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
    if (VertexColorMaterial.Succeeded())
    {
        CanyonTerrainMesh->SetMaterial(0, VertexColorMaterial.Object);
        SkyDomeMesh->SetMaterial(0, VertexColorMaterial.Object);
        CloudLayerMesh->SetMaterial(0, VertexColorMaterial.Object);
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

    ControlPoints = YarlungCoaster::DefaultTrackControlPoints();
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
    SandBars->ClearInstances();
    SnowCaps->ClearInstances();
    CanyonTerrainMesh->ClearAllMeshSections();
    RiverRibbonMesh->ClearAllMeshSections();
    FoamRibbonMesh->ClearAllMeshSections();
    SkyDomeMesh->ClearAllMeshSections();
    CloudLayerMesh->ClearAllMeshSections();

    {
        TArray<FVector> SkyVertices;
        TArray<int32> SkyTriangles;
        TArray<FVector> SkyNormals;
        TArray<FVector2D> SkyUVs;
        TArray<FLinearColor> SkyColors;
        TArray<FProcMeshTangent> SkyTangents;
        constexpr int32 RingCount = 9;
        constexpr int32 SegmentCount = 56;
        constexpr float SkyRadius = 92000.0f;
        SkyVertices.Reserve(RingCount * SegmentCount);

        for (int32 Ring = 0; Ring < RingCount; ++Ring)
        {
            const float V = static_cast<float>(Ring) / static_cast<float>(RingCount - 1);
            const float Theta = V * UE_HALF_PI;
            const float RadiusAtRing = FMath::Cos(Theta) * SkyRadius;
            const float Z = FMath::Sin(Theta) * SkyRadius - 2200.0f;
            const FLinearColor HorizonColor(0.54f, 0.69f, 0.88f, 1.0f);
            const FLinearColor ZenithColor(0.08f, 0.30f, 0.70f, 1.0f);
            const FLinearColor RingColor = FMath::Lerp(HorizonColor, ZenithColor, Smooth01(V));

            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const float U = static_cast<float>(Segment) / static_cast<float>(SegmentCount);
                const float Angle = U * UE_TWO_PI;
                SkyVertices.Add(FVector(FMath::Cos(Angle) * RadiusAtRing, FMath::Sin(Angle) * RadiusAtRing, Z));
                SkyNormals.Add(-SkyVertices.Last().GetSafeNormal());
                SkyUVs.Add(FVector2D(U, V));
                SkyColors.Add(RingColor);
                SkyTangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
            }
        }

        for (int32 Ring = 0; Ring < RingCount - 1; ++Ring)
        {
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const int32 NextSegment = (Segment + 1) % SegmentCount;
                const int32 A = Ring * SegmentCount + Segment;
                const int32 B = (Ring + 1) * SegmentCount + Segment;
                const int32 C = Ring * SegmentCount + NextSegment;
                const int32 D = (Ring + 1) * SegmentCount + NextSegment;
                AddDoubleSidedQuad(SkyTriangles, A, B, C, D);
            }
        }

        SkyDomeMesh->CreateMeshSection_LinearColor(0, SkyVertices, SkyTriangles, SkyNormals, SkyUVs, SkyColors, SkyTangents, false);
    }

    {
        TArray<FVector> CloudVertices;
        TArray<int32> CloudTriangles;
        TArray<FVector> CloudNormals;
        TArray<FVector2D> CloudUVs;
        TArray<FLinearColor> CloudColors;
        TArray<FProcMeshTangent> CloudTangents;
        constexpr int32 CloudPatchCount = 18;
        constexpr int32 CloudSegments = 18;
        for (int32 Patch = 0; Patch < CloudPatchCount; ++Patch)
        {
            const float Row = static_cast<float>(Patch / 6);
            const float Column = static_cast<float>(Patch % 6);
            const float X = FMath::Lerp(-43000.0f, 46000.0f, Column / 5.0f) + 3200.0f * FMath::Sin(Patch * 1.7f);
            const float Y = FMath::Lerp(-24500.0f, 21000.0f, Row / 2.0f) + 2600.0f * FMath::Cos(Patch * 0.9f);
            const float Z = 18200.0f + 1500.0f * FMath::Sin(Patch * 0.47f);
            const float RadiusX = 2450.0f + 1150.0f * Hash01(Patch * 1.3f, 2.1f);
            const float RadiusY = 680.0f + 440.0f * Hash01(3.4f, Patch * 2.2f);
            const float Yaw = Hash01(Patch * 4.1f, Patch * 0.6f) * UE_TWO_PI;
            const FVector Center(X, Y, Z);
            const FVector AxisX(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0f);
            const FVector AxisY(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0f);
            const int32 BaseIndex = CloudVertices.Num();

            CloudVertices.Add(Center);
            CloudNormals.Add(FVector::DownVector);
            CloudUVs.Add(FVector2D(0.5f, 0.5f));
            CloudColors.Add(FLinearColor(0.82f, 0.86f, 0.88f, 1.0f));
            CloudTangents.Add(FProcMeshTangent(AxisX, false));

            for (int32 Segment = 0; Segment < CloudSegments; ++Segment)
            {
                const float Angle = (static_cast<float>(Segment) / static_cast<float>(CloudSegments)) * UE_TWO_PI;
                const float EdgeJitter = 0.84f + 0.20f * Hash01(Patch * 11.0f, Segment * 5.0f);
                const FVector Edge = Center
                    + AxisX * (FMath::Cos(Angle) * RadiusX * EdgeJitter)
                    + AxisY * (FMath::Sin(Angle) * RadiusY * EdgeJitter);
                CloudVertices.Add(Edge);
                CloudNormals.Add(FVector::DownVector);
                CloudUVs.Add(FVector2D(FMath::Cos(Angle) * 0.5f + 0.5f, FMath::Sin(Angle) * 0.5f + 0.5f));
                const float Bright = 0.76f + 0.10f * Hash01(Segment * 0.7f, Patch * 1.9f);
                CloudColors.Add(FLinearColor(Bright, Bright + 0.04f, Bright + 0.06f, 1.0f));
                CloudTangents.Add(FProcMeshTangent(AxisX, false));
            }

            for (int32 Segment = 0; Segment < CloudSegments; ++Segment)
            {
                const int32 Next = (Segment + 1) % CloudSegments;
                CloudTriangles.Append({ BaseIndex, BaseIndex + 1 + Segment, BaseIndex + 1 + Next });
                CloudTriangles.Append({ BaseIndex, BaseIndex + 1 + Next, BaseIndex + 1 + Segment });
            }
        }

        CloudLayerMesh->CreateMeshSection_LinearColor(0, CloudVertices, CloudTriangles, CloudNormals, CloudUVs, CloudColors, CloudTangents, false);
    }

    const float RiverHalfWidth = 2050.0f;
    const float SegmentStep = 520.0f;
    const int32 SampleCount = FMath::Max(FMath::CeilToInt(TrackLengthCm / SegmentStep), 16);
    const bool bHasImportedLandscape = HasImportedLandscape();

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
    const int32 TerrainGridX = 156;
    const int32 TerrainGridY = 128;
    const float MinX = -4200.0f;
    const float MaxX = 12200.0f;
    const float MinY = -10500.0f;
    const float MaxY = 8200.0f;
    TerrainVertices.Reserve(TerrainGridX * TerrainGridY);
    TerrainNormals.Reserve(TerrainGridX * TerrainGridY);
    TerrainUVs.Reserve(TerrainGridX * TerrainGridY);
    TerrainColors.Reserve(TerrainGridX * TerrainGridY);
    TerrainTangents.Reserve(TerrainGridX * TerrainGridY);

    for (int32 YIndex = 0; YIndex < TerrainGridY; ++YIndex)
    {
        const float V = static_cast<float>(YIndex) / static_cast<float>(TerrainGridY - 1);
        const float Y = FMath::Lerp(MinY, MaxY, V);
        for (int32 XIndex = 0; XIndex < TerrainGridX; ++XIndex)
        {
            const float U = static_cast<float>(XIndex) / static_cast<float>(TerrainGridX - 1);
            const float X = FMath::Lerp(MinX, MaxX, U);
            const float Height = YarlungTerrainHeight(X, Y);
            TerrainVertices.Add(FVector(X, Y, Height));
            TerrainNormals.Add(FVector::UpVector);
            TerrainUVs.Add(FVector2D(U * 9.0f, V * 9.0f));
            TerrainColors.Add(YarlungTerrainColor(X, Y, Height));
            TerrainTangents.Add(FProcMeshTangent(FVector::ForwardVector, false));

            const float ForestAmount = YarlungForestAmount(X, Y, Height);
            const float Lateral = FMath::Abs(Y - YarlungRiverCenterY(X));
            const float DetailNoise = Hash01(static_cast<float>(XIndex), static_cast<float>(YIndex));
            const float TrackDistance = YarlungCoaster::DistanceToTrack2D(X, Y);
            if (ForestAmount > 0.32f && TrackDistance > 2350.0f && (XIndex % 5) == 0 && (YIndex % 4) == 0 && DetailNoise > 0.22f)
            {
                const float Yaw = FMath::Fmod(FMath::Abs(X * 0.37f + Y * 0.19f), 360.0f);
                const float ScaleJitter = 0.62f + 0.34f * Hash01(X * 0.013f, Y * 0.009f);
                ForestPatches->AddInstance(FTransform(
                    FRotator(0.0f, Yaw, 0.0f),
                    FVector(X, Y, Height + 58.0f),
                    FVector(0.58f * ScaleJitter, 0.58f * ScaleJitter, 1.80f * ScaleJitter)));
            }

            if (Lateral > 1200.0f && Lateral < 3150.0f && TrackDistance > 1800.0f && (XIndex % 8) == 0 && (YIndex % 5) == 0)
            {
                const float SandScale = 0.55f + 0.35f * DetailNoise;
                SandBars->AddInstance(FTransform(
                    FRotator(0.0f, FMath::Fmod(X * 0.08f + Y * 0.03f, 360.0f), 0.0f),
                    FVector(X, Y, Height + 4.0f),
                    FVector(2.7f * SandScale, 0.36f * SandScale, 0.035f)));
            }

            if (Height > 460.0f && Lateral > 2200.0f && TrackDistance > 2200.0f && (XIndex % 8) == 0 && (YIndex % 7) == 0 && DetailNoise > 0.18f)
            {
                const float RockScale = 0.62f + 0.56f * Hash01(Y * 0.011f, X * 0.017f);
                RiverRocks->AddInstance(FTransform(
                    FRotator(0.0f, FMath::Fmod(X + Y, 360.0f), 0.0f),
                    FVector(X, Y, Height + 34.0f),
                    FVector(RockScale * 1.9f, RockScale * 1.15f, RockScale * 0.62f)));
            }

            if (Height > 650.0f && Lateral > 3900.0f && TrackDistance > 4100.0f && (XIndex % 13) == 0 && (YIndex % 10) == 0 && DetailNoise > 0.48f)
            {
                const float SlabScale = 0.86f + DetailNoise * 0.92f;
                CanyonWalls->AddInstance(FTransform(
                    FRotator(-6.0f + 12.0f * DetailNoise, FMath::Fmod(X * 0.05f - Y * 0.02f, 360.0f), 0.0f),
                    FVector(X, Y, Height + 54.0f),
                    FVector(SlabScale * 1.62f, SlabScale * 0.48f, SlabScale * 0.24f)));
            }

            if (Height > 2700.0f && TrackDistance > 2200.0f && (XIndex % 11) == 0 && (YIndex % 9) == 0)
            {
                const float SnowScale = 1.2f + DetailNoise * 1.8f;
                SnowCaps->AddInstance(FTransform(
                    FRotator(0.0f, FMath::Fmod(X * 0.11f + Y * 0.07f, 360.0f), 0.0f),
                    FVector(X, Y, Height + 12.0f),
                    FVector(SnowScale * 2.2f, SnowScale * 1.15f, SnowScale * 0.06f)));
            }
        }
    }

    for (int32 YIndex = 0; YIndex < TerrainGridY; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < TerrainGridX; ++XIndex)
        {
            const int32 Center = YIndex * TerrainGridX + XIndex;
            const int32 Left = YIndex * TerrainGridX + FMath::Max(XIndex - 1, 0);
            const int32 Right = YIndex * TerrainGridX + FMath::Min(XIndex + 1, TerrainGridX - 1);
            const int32 Down = FMath::Max(YIndex - 1, 0) * TerrainGridX + XIndex;
            const int32 Up = FMath::Min(YIndex + 1, TerrainGridY - 1) * TerrainGridX + XIndex;
            const FVector Dx = TerrainVertices[Right] - TerrainVertices[Left];
            const FVector Dy = TerrainVertices[Up] - TerrainVertices[Down];
            TerrainNormals[Center] = FVector::CrossProduct(Dx, Dy).GetSafeNormal();
        }
    }

    for (int32 YIndex = 0; YIndex < TerrainGridY - 1; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < TerrainGridX - 1; ++XIndex)
        {
            const int32 A = YIndex * TerrainGridX + XIndex;
            const int32 B = (YIndex + 1) * TerrainGridX + XIndex;
            const int32 C = YIndex * TerrainGridX + XIndex + 1;
            const int32 D = (YIndex + 1) * TerrainGridX + XIndex + 1;
            AddDoubleSidedQuad(TerrainTriangles, A, B, C, D);
        }
    }

    if (!bHasImportedLandscape)
    {
        CanyonTerrainMesh->CreateMeshSection_LinearColor(0, TerrainVertices, TerrainTriangles, TerrainNormals, TerrainUVs, TerrainColors, TerrainTangents, false);
    }

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
                0.035f + CenterWeight * 0.015f,
                0.21f + CenterWeight * 0.12f,
                0.24f + CenterWeight * 0.16f,
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
            FoamColors.Add(FLinearColor(0.38f, 0.48f, 0.44f, 1.0f));
            FoamColors.Add(FLinearColor(0.56f, 0.66f, 0.60f, 1.0f));
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
        if (Along % 4 == 0)
        {
            const FVector FoamStart = (SampleA.Center + SampleB.Center) * 0.5f - SampleA.Right * (RiverHalfWidth * 0.56f) + FVector(0.0f, 0.0f, 18.0f);
            const FVector FoamEnd = (SampleA.Center + SampleB.Center) * 0.5f + SampleA.Right * (RiverHalfWidth * 0.56f) + FVector(0.0f, 0.0f, 18.0f);
            Rapids->AddInstance(MakeSegmentTransform(FoamStart, FoamEnd, FVector(RiverHalfWidth * 0.54f, 4.0f, 2.0f)));
        }

        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const FVector BankA = SampleA.Center + SampleA.Right * (Side * (RiverHalfWidth + 220.0f)) + FVector(0.0f, 0.0f, 18.0f);
            const FVector BankB = SampleB.Center + SampleB.Right * (Side * (RiverHalfWidth + 280.0f)) + FVector(0.0f, 0.0f, 24.0f);
            if (Along % 4 == 0)
            {
                RiverRocks->AddInstance(MakeSegmentTransform(BankA + FVector(0.0f, 0.0f, 8.0f), BankB + FVector(0.0f, 0.0f, 8.0f), FVector(SegmentStep, 120.0f, 14.0f)));
            }
        }

        if (Along % 5 == 0)
        {
            const FVector RockCenter = (SampleA.Center + SampleB.Center) * 0.5f + SampleA.Right * (FMath::Sin(SampleA.Ratio * UE_TWO_PI * 9.0f) * RiverHalfWidth * 0.5f) + FVector(0.0f, 0.0f, 42.0f);
            const FVector RockEnd = RockCenter + SampleA.Forward * 120.0f + SampleA.Right * 40.0f;
            RiverRocks->AddInstance(MakeSegmentTransform(RockCenter, RockEnd, FVector(110.0f, 48.0f, 30.0f)));
        }
    }
}

void ACoasterRideActor::ApplyVisualMaterials()
{
    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));

    auto TintComponent = [TintMaterial](UMeshComponent* Component, const FLinearColor& Color)
    {
        if (!Component)
        {
            return;
        }

        if (TintMaterial)
        {
            Component->SetMaterial(0, TintMaterial);
        }

        UMaterialInstanceDynamic* Material = Component->CreateAndSetMaterialInstanceDynamic(0);
        if (!Material)
        {
            return;
        }

        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
        Material->SetScalarParameterValue(TEXT("Roughness"), 0.88f);
        Material->SetScalarParameterValue(TEXT("Specular"), 0.10f);
    };

    TintComponent(TrainBody, FLinearColor(0.12f, 0.10f, 0.085f));
    TintComponent(LeftRail, FLinearColor(0.55f, 0.57f, 0.56f));
    TintComponent(RightRail, FLinearColor(0.55f, 0.57f, 0.56f));
    TintComponent(Ties, FLinearColor(0.22f, 0.18f, 0.13f));
    TintComponent(Supports, FLinearColor(0.38f, 0.42f, 0.44f));
    TintComponent(CanyonWalls, FLinearColor(0.31f, 0.28f, 0.22f));
    TintComponent(RiverSurface, FLinearColor(0.035f, 0.24f, 0.27f));
    TintComponent(Rapids, FLinearColor(0.44f, 0.56f, 0.50f));
    TintComponent(RiverRocks, FLinearColor(0.33f, 0.32f, 0.29f));
    TintComponent(ForestPatches, FLinearColor(0.035f, 0.12f, 0.045f));
    TintComponent(MistBands, FLinearColor(0.50f, 0.56f, 0.52f));
    TintComponent(SandBars, FLinearColor(0.46f, 0.40f, 0.29f));
    TintComponent(SnowCaps, FLinearColor(0.74f, 0.80f, 0.78f));
    TintComponent(RiverRibbonMesh, FLinearColor(0.035f, 0.24f, 0.27f));
    TintComponent(FoamRibbonMesh, FLinearColor(0.44f, 0.56f, 0.50f));
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
    RideCamera->SetRelativeLocation(FVector(-64.0f, 0.0f, 248.0f));
    RideCamera->SetRelativeRotation(FRotator(-9.0f, 0.0f, 0.0f));
}

bool ACoasterRideActor::HasImportedLandscape() const
{
    if (!bUseImportedLandscapeWhenPresent || !GetWorld())
    {
        return false;
    }

    for (TActorIterator<ALandscape> It(GetWorld()); It; ++It)
    {
        return true;
    }

    return false;
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
