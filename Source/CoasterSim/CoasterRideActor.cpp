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
    TrainBody->SetRelativeScale3D(FVector(1.35f, 0.78f, 0.32f));
    TrainBody->SetRelativeLocation(FVector(-45.0f, 0.0f, -78.0f));

    RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
    RideCamera->SetupAttachment(SceneRoot);
    RideCamera->SetRelativeLocation(FVector(-520.0f, 120.0f, 260.0f));
    RideCamera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
    RideCamera->SetFieldOfView(60.0f);
    RideCamera->bUsePawnControlRotation = false;
    RideCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    RideCamera->PostProcessSettings.MotionBlurAmount = 0.12f;
    RideCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
    RideCamera->PostProcessSettings.VignetteIntensity = 0.16f;
    RideCamera->PostProcessSettings.bOverride_ColorSaturation = true;
    RideCamera->PostProcessSettings.ColorSaturation = FVector4(0.94f, 1.0f, 0.98f, 1.0f);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetIntensity(0.38f);

    ValleyFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ValleyFog"));
    ValleyFog->SetupAttachment(SceneRoot);
    ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, RiverZCm + 70.0f));
    ValleyFog->SetFogDensity(0.0028f);
    ValleyFog->SetFogHeightFalloff(0.18f);
    ValleyFog->SetFogInscatteringColor(FLinearColor(0.52f, 0.62f, 0.60f));

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
    UpdateCinematicCamera(Location, Forward);
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

    const float RiverHalfWidth = 1800.0f;
    const float SegmentStep = 520.0f;

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

        FVector EnvForwardA(ForwardA.X, ForwardA.Y, 0.0f);
        if (EnvForwardA.IsNearlyZero())
        {
            EnvForwardA = FVector::ForwardVector;
        }
        EnvForwardA.Normalize();

        FVector EnvForwardB(ForwardB.X, ForwardB.Y, 0.0f);
        if (EnvForwardB.IsNearlyZero())
        {
            EnvForwardB = EnvForwardA;
        }
        EnvForwardB.Normalize();

        const FVector EnvRightA = FVector::CrossProduct(FVector::UpVector, EnvForwardA).GetSafeNormal();
        const FVector EnvRightB = FVector::CrossProduct(FVector::UpVector, EnvForwardB).GetSafeNormal();

        const float TrackRatio = Distance / TrackLengthCm;
        const float Bend = FMath::Sin(TrackRatio * UE_TWO_PI * 2.0f) * 90.0f;
        const FVector RiverA(LocationA.X, LocationA.Y + Bend, RiverZCm);
        const FVector RiverB(LocationB.X, LocationB.Y - Bend, RiverZCm);

        RiverSurface->AddInstance(MakeSegmentTransform(RiverA, RiverB, FVector(SegmentStep, RiverHalfWidth * 2.0f, 10.0f)));

        const float FoamSkew = (FMath::Fmod(Distance / SegmentStep, 2.0f) < 1.0f) ? 1.0f : -1.0f;
        const FVector FoamCenter = (RiverA + RiverB) * 0.5f + FVector(0.0f, 0.0f, 18.0f);
        const FVector FoamRight = EnvRightA;
        const FVector FoamForward = EnvForwardA;
        const FVector FoamStart = FoamCenter - FoamRight * (RiverHalfWidth * 0.78f) - FoamForward * (FoamSkew * 80.0f);
        const FVector FoamEnd = FoamCenter + FoamRight * (RiverHalfWidth * 0.78f) + FoamForward * (FoamSkew * 80.0f);
        Rapids->AddInstance(MakeSegmentTransform(FoamStart, FoamEnd, FVector(RiverHalfWidth * 1.05f, 4.0f, 2.0f)));

        const FVector MistCenter = FoamCenter + FVector(0.0f, 0.0f, 95.0f);
        MistBands->AddInstance(MakeSegmentTransform(
            MistCenter - FoamForward * 170.0f,
            MistCenter + FoamForward * 170.0f,
            FVector(260.0f, RiverHalfWidth * 0.34f, 5.0f)));

        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            for (int32 Layer = 0; Layer < 5; ++Layer)
            {
                const float LayerOffset = RiverHalfWidth + 5200.0f + Layer * 2400.0f;
                const float LayerWidth = 620.0f + Layer * 360.0f;
                const float LayerHeight = 220.0f + Layer * 260.0f;
                const float LayerZ = RiverZCm + 210.0f + Layer * 560.0f + 120.0f * FMath::Sin(TrackRatio * UE_TWO_PI * 2.5f + Layer);
                const FVector LayerA = FVector(RiverA.X, RiverA.Y, LayerZ) + EnvRightA * (Side * LayerOffset);
                const FVector LayerB = FVector(RiverB.X, RiverB.Y, LayerZ + 45.0f * FMath::Sin(TrackRatio * UE_TWO_PI + Layer)) + EnvRightB * (Side * (LayerOffset + 80.0f));
                CanyonWalls->AddInstance(MakeSegmentTransform(LayerA, LayerB, FVector(SegmentStep, LayerWidth, LayerHeight)));

                if (Layer >= 1)
                {
                    const FVector ForestA = LayerA + EnvRightA * (Side * 35.0f) + FVector(0.0f, 0.0f, LayerHeight * 0.65f);
                    const FVector ForestB = LayerB + EnvRightB * (Side * 35.0f) + FVector(0.0f, 0.0f, LayerHeight * 0.65f);
                    ForestPatches->AddInstance(MakeSegmentTransform(ForestA, ForestB, FVector(SegmentStep * 0.42f, LayerWidth * 0.10f, 12.0f)));
                }
            }

            const FVector BankA = RiverA + EnvRightA * (Side * (RiverHalfWidth + 140.0f)) + FVector(0.0f, 0.0f, 24.0f);
            const FVector BankB = RiverB + EnvRightB * (Side * (RiverHalfWidth + 190.0f)) + FVector(0.0f, 0.0f, 30.0f);
            RiverRocks->AddInstance(MakeSegmentTransform(BankA, BankB, FVector(SegmentStep, 320.0f, 34.0f)));
        }

        if (FMath::Fmod(Distance / SegmentStep, 3.0f) < 1.0f)
        {
            const FVector RockCenter = (RiverA + RiverB) * 0.5f + EnvRightA * (FMath::Sin(TrackRatio * UE_TWO_PI * 9.0f) * RiverHalfWidth * 0.5f) + FVector(0.0f, 0.0f, 42.0f);
            const FVector RockEnd = RockCenter + EnvForwardA * 120.0f + EnvRightA * 40.0f;
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
    UpdateCinematicCamera(NewLocation, NewForward);

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

void ACoasterRideActor::UpdateCinematicCamera(const FVector& TrainLocation, const FVector& TrainForward)
{
    FVector Forward = TrainForward.GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::ForwardVector;
    }

    FVector HorizontalForward(Forward.X, Forward.Y, 0.0f);
    if (HorizontalForward.IsNearlyZero())
    {
        HorizontalForward = FVector::ForwardVector;
    }
    HorizontalForward.Normalize();

    const FVector HorizontalRight = FVector::CrossProduct(FVector::UpVector, HorizontalForward).GetSafeNormal();
    const FVector CameraLocation = TrainLocation - HorizontalForward * 3200.0f + HorizontalRight * 7200.0f + FVector(0.0f, 0.0f, 3600.0f);
    const FVector LookTarget = TrainLocation + HorizontalForward * 1100.0f + FVector(0.0f, 0.0f, 260.0f);
    const FVector LookDirection = (LookTarget - CameraLocation).GetSafeNormal();

    RideCamera->SetRelativeLocation(CameraLocation);
    RideCamera->SetRelativeRotation(FRotationMatrix::MakeFromX(LookDirection).Rotator());
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
