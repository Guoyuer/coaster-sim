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
    RideCamera->SetRelativeLocation(FVector(-104.0f, 0.0f, 286.0f));
    RideCamera->SetRelativeRotation(FRotator(-4.0f, 0.0f, 0.0f));
    RideCamera->SetFieldOfView(92.0f);
    RideCamera->bUsePawnControlRotation = false;
    RideCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    RideCamera->PostProcessSettings.MotionBlurAmount = 0.065f;
    RideCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
    RideCamera->PostProcessSettings.VignetteIntensity = 0.18f;
    RideCamera->PostProcessSettings.bOverride_ColorSaturation = true;
    RideCamera->PostProcessSettings.ColorSaturation = FVector4(0.74f, 0.86f, 0.84f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorContrast = true;
    RideCamera->PostProcessSettings.ColorContrast = FVector4(1.08f, 1.07f, 1.04f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorGamma = true;
    RideCamera->PostProcessSettings.ColorGamma = FVector4(0.96f, 0.98f, 1.02f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorGain = true;
    RideCamera->PostProcessSettings.ColorGain = FVector4(1.03f, 1.08f, 1.10f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_WhiteTemp = true;
    RideCamera->PostProcessSettings.WhiteTemp = 6300.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    RideCamera->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    RideCamera->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
    RideCamera->PostProcessSettings.AutoExposureBias = 0.22f;
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
    SkyLight->SetIntensity(2.2f);

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);
    SunLight->SetRelativeRotation(FRotator(-46.0f, -24.0f, 0.0f));
    SunLight->SetIntensity(15.0f);
    SunLight->SetTemperature(6100.0f);
    SunLight->SetLightColor(FLinearColor(1.0f, 1.0f, 0.96f));
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
    RiverSurface = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RiverSurface"));
    RiverSurface->SetupAttachment(SceneRoot);
    Rapids = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rapids"));
    Rapids->SetupAttachment(SceneRoot);
    MistBands = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MistBands"));
    MistBands->SetupAttachment(SceneRoot);
    RiverRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RiverRibbonMesh"));
    RiverRibbonMesh->SetupAttachment(SceneRoot);
    RiverRibbonMesh->bUseAsyncCooking = true;
    FoamRibbonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FoamRibbonMesh"));
    FoamRibbonMesh->SetupAttachment(SceneRoot);
    FoamRibbonMesh->bUseAsyncCooking = true;
    DistantRidgeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("DistantRidgeMesh"));
    DistantRidgeMesh->SetupAttachment(SceneRoot);
    DistantRidgeMesh->bUseAsyncCooking = true;
    SkyDomeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SkyDomeMesh"));
    SkyDomeMesh->SetupAttachment(SceneRoot);
    SkyDomeMesh->bUseAsyncCooking = true;
    CloudLayerMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CloudLayerMesh"));
    CloudLayerMesh->SetupAttachment(SceneRoot);
    CloudLayerMesh->bUseAsyncCooking = true;

    RiverSurface->SetCastShadow(false);
    Rapids->SetCastShadow(false);
    MistBands->SetCastShadow(false);
    RiverRibbonMesh->SetCastShadow(false);
    FoamRibbonMesh->SetCastShadow(false);
    DistantRidgeMesh->SetCastShadow(false);
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
        RiverSurface->SetStaticMesh(CubeMesh.Object);
        Rapids->SetStaticMesh(CubeMesh.Object);
        MistBands->SetStaticMesh(CubeMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
    if (BasicMaterial.Succeeded())
    {
        TrainBody->SetMaterial(0, BasicMaterial.Object);
        LeftRail->SetMaterial(0, BasicMaterial.Object);
        RightRail->SetMaterial(0, BasicMaterial.Object);
        Ties->SetMaterial(0, BasicMaterial.Object);
        Supports->SetMaterial(0, BasicMaterial.Object);
        RiverSurface->SetMaterial(0, BasicMaterial.Object);
        Rapids->SetMaterial(0, BasicMaterial.Object);
        MistBands->SetMaterial(0, BasicMaterial.Object);
        RiverRibbonMesh->SetMaterial(0, BasicMaterial.Object);
        FoamRibbonMesh->SetMaterial(0, BasicMaterial.Object);
        DistantRidgeMesh->SetMaterial(0, BasicMaterial.Object);
        SkyDomeMesh->SetMaterial(0, BasicMaterial.Object);
        CloudLayerMesh->SetMaterial(0, BasicMaterial.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterial(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
    if (VertexColorMaterial.Succeeded())
    {
        DistantRidgeMesh->SetMaterial(0, VertexColorMaterial.Object);
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
    ApplyVisualMaterials();
    RebuildEnvironment();
    RebuildVisuals();
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
    ClearEnvironmentVisuals();
    BuildSkyDome();
    BuildCloudLayer();
    BuildDistantRidges();
    BuildRiverEffects();
}

void ACoasterRideActor::ClearEnvironmentVisuals()
{
    RiverSurface->ClearInstances();
    Rapids->ClearInstances();
    MistBands->ClearInstances();
    RiverRibbonMesh->ClearAllMeshSections();
    FoamRibbonMesh->ClearAllMeshSections();
    DistantRidgeMesh->ClearAllMeshSections();
    SkyDomeMesh->ClearAllMeshSections();
    CloudLayerMesh->ClearAllMeshSections();
}

void ACoasterRideActor::BuildSkyDome()
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

void ACoasterRideActor::BuildCloudLayer()
{
    TArray<FVector> CloudVertices;
    TArray<int32> CloudTriangles;
    TArray<FVector> CloudNormals;
    TArray<FVector2D> CloudUVs;
    TArray<FLinearColor> CloudColors;
    TArray<FProcMeshTangent> CloudTangents;
    constexpr int32 CloudPatchCount = 12;
    constexpr int32 CloudSegments = 18;
    for (int32 Patch = 0; Patch < CloudPatchCount; ++Patch)
    {
        const float Row = static_cast<float>(Patch / 6);
        const float Column = static_cast<float>(Patch % 6);
        const float X = FMath::Lerp(-43000.0f, 46000.0f, Column / 5.0f) + 3200.0f * FMath::Sin(Patch * 1.7f);
        const float Y = FMath::Lerp(-24500.0f, 21000.0f, Row / 2.0f) + 2600.0f * FMath::Cos(Patch * 0.9f);
        const float Z = 21400.0f + 1700.0f * FMath::Sin(Patch * 0.47f);
        const float RadiusX = 1750.0f + 860.0f * Hash01(Patch * 1.3f, 2.1f);
        const float RadiusY = 420.0f + 300.0f * Hash01(3.4f, Patch * 2.2f);
        const float Yaw = Hash01(Patch * 4.1f, Patch * 0.6f) * UE_TWO_PI;
        const FVector Center(X, Y, Z);
        const FVector AxisX(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0f);
        const FVector AxisY(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0f);
        const int32 BaseIndex = CloudVertices.Num();

        CloudVertices.Add(Center);
        CloudNormals.Add(FVector::DownVector);
        CloudUVs.Add(FVector2D(0.5f, 0.5f));
        CloudColors.Add(FLinearColor(0.78f, 0.82f, 0.84f, 1.0f));
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
            const float Bright = 0.70f + 0.09f * Hash01(Segment * 0.7f, Patch * 1.9f);
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

void ACoasterRideActor::BuildDistantRidges()
{
    TArray<FVector> RidgeVertices;
    TArray<int32> RidgeTriangles;
    TArray<FVector> RidgeNormals;
    TArray<FVector2D> RidgeUVs;
    TArray<FLinearColor> RidgeColors;
    TArray<FProcMeshTangent> RidgeTangents;
    constexpr int32 RidgeSamples = 42;
    const TArray<float> RidgeYValues = { -23200.0f, 21400.0f };

    for (int32 RidgeIndex = 0; RidgeIndex < RidgeYValues.Num(); ++RidgeIndex)
    {
        const float RidgeY = RidgeYValues[RidgeIndex];
        const int32 BaseIndex = RidgeVertices.Num();
        for (int32 Sample = 0; Sample < RidgeSamples; ++Sample)
        {
            const float T = static_cast<float>(Sample) / static_cast<float>(RidgeSamples - 1);
            const float X = FMath::Lerp(-11200.0f, 18400.0f, T);
            const float RidgeNoise = 0.45f * FMath::Sin(T * UE_TWO_PI * 2.4f + RidgeIndex)
                + 0.32f * FMath::Sin(T * UE_TWO_PI * 5.7f + 0.6f);
            const float BaseZ = 420.0f + 260.0f * FMath::Sin(T * UE_TWO_PI * 1.2f + RidgeIndex);
            const float MidZ = 2440.0f + 360.0f * RidgeIndex + RidgeNoise * 520.0f;
            const float PeakZ = 4860.0f + RidgeNoise * 880.0f + 540.0f * Hash01(Sample * 1.7f, RidgeIndex);

            RidgeVertices.Append({
                FVector(X, RidgeY, BaseZ),
                FVector(X, RidgeY, MidZ),
                FVector(X, RidgeY, PeakZ)
            });
            RidgeNormals.Append({ FVector::ForwardVector, FVector::ForwardVector, FVector::ForwardVector });
            RidgeUVs.Append({ FVector2D(T, 0.0f), FVector2D(T, 0.55f), FVector2D(T, 1.0f) });
            RidgeColors.Append({
                FLinearColor(0.10f, 0.13f, 0.13f, 1.0f),
                FLinearColor(0.20f, 0.24f, 0.24f, 1.0f),
                FLinearColor(0.38f, 0.45f, 0.46f, 1.0f)
            });
            RidgeTangents.Append({
                FProcMeshTangent(FVector::ForwardVector, false),
                FProcMeshTangent(FVector::ForwardVector, false),
                FProcMeshTangent(FVector::ForwardVector, false)
            });
        }

        for (int32 Sample = 0; Sample < RidgeSamples - 1; ++Sample)
        {
            const int32 A = BaseIndex + Sample * 3;
            const int32 B = BaseIndex + (Sample + 1) * 3;
            const int32 C = BaseIndex + Sample * 3 + 1;
            const int32 D = BaseIndex + (Sample + 1) * 3 + 1;
            const int32 E = BaseIndex + Sample * 3 + 2;
            const int32 F = BaseIndex + (Sample + 1) * 3 + 2;
            AddDoubleSidedQuad(RidgeTriangles, A, B, C, D);
            AddDoubleSidedQuad(RidgeTriangles, C, D, E, F);
        }
    }

    DistantRidgeMesh->CreateMeshSection_LinearColor(0, RidgeVertices, RidgeTriangles, RidgeNormals, RidgeUVs, RidgeColors, RidgeTangents, false);
}

void ACoasterRideActor::BuildRiverEffects()
{
    const float RiverHalfWidth = 2050.0f;
    const float SegmentStep = 520.0f;
    const int32 SampleCount = FMath::Max(FMath::CeilToInt(TrackLengthCm / SegmentStep), 16);

    TArray<FEnvironmentRiverSample> Samples;
    BuildRiverSamples(Samples, SampleCount);
    BuildRiverRibbon(Samples, RiverHalfWidth);
    BuildFoamRibbon(Samples, RiverHalfWidth);
    BuildRapids(Samples, RiverHalfWidth);
}

void ACoasterRideActor::BuildRiverSamples(TArray<FEnvironmentRiverSample>& OutSamples, int32 SampleCount) const
{
    OutSamples.Reset();
    OutSamples.Reserve(SampleCount + 1);

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
        FEnvironmentRiverSample& Sample = OutSamples.AddDefaulted_GetRef();
        Sample.Center = FVector(LocationA.X, LocationA.Y + Bend, RiverZCm);
        Sample.Forward = EnvForwardA;
        Sample.Right = EnvRightA;
        Sample.Ratio = TrackRatio;
    }
}

void ACoasterRideActor::BuildRiverRibbon(const TArray<FEnvironmentRiverSample>& Samples, float RiverHalfWidth)
{
    TArray<FVector> RiverVertices;
    TArray<int32> RiverTriangles;
    TArray<FVector> RiverNormals;
    TArray<FVector2D> RiverUVs;
    TArray<FLinearColor> RiverColors;
    TArray<FProcMeshTangent> RiverTangents;
    const TArray<float> RiverAcross = { -1.0f, -0.52f, 0.0f, 0.52f, 1.0f };
    for (int32 Along = 0; Along < Samples.Num(); ++Along)
    {
        const FEnvironmentRiverSample& Sample = Samples[Along];
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
}

void ACoasterRideActor::BuildFoamRibbon(const TArray<FEnvironmentRiverSample>& Samples, float RiverHalfWidth)
{
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
            const FEnvironmentRiverSample& Sample = Samples[Along];
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
}

void ACoasterRideActor::BuildRapids(const TArray<FEnvironmentRiverSample>& Samples, float RiverHalfWidth)
{
    for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
    {
        const FEnvironmentRiverSample& SampleA = Samples[Along];
        const FEnvironmentRiverSample& SampleB = Samples[Along + 1];
        if (Along % 4 == 0)
        {
            const FVector FoamStart = (SampleA.Center + SampleB.Center) * 0.5f - SampleA.Right * (RiverHalfWidth * 0.56f) + FVector(0.0f, 0.0f, 18.0f);
            const FVector FoamEnd = (SampleA.Center + SampleB.Center) * 0.5f + SampleA.Right * (RiverHalfWidth * 0.56f) + FVector(0.0f, 0.0f, 18.0f);
            Rapids->AddInstance(MakeSegmentTransform(FoamStart, FoamEnd, FVector(RiverHalfWidth * 0.54f, 4.0f, 2.0f)));
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
    TintComponent(RiverSurface, FLinearColor(0.035f, 0.24f, 0.27f));
    TintComponent(Rapids, FLinearColor(0.44f, 0.56f, 0.50f));
    TintComponent(MistBands, FLinearColor(0.50f, 0.56f, 0.52f));
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
    RideCamera->SetRelativeLocation(FVector(-104.0f, 0.0f, 286.0f));
    RideCamera->SetRelativeRotation(FRotator(-4.0f, 0.0f, 0.0f));
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
