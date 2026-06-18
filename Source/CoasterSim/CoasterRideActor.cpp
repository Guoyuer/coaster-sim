#include "CoasterRideActor.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float CmPerMeter = 100.0f;
constexpr float GravityCms2 = 980.665f;

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
    TrainBody->SetRelativeScale3D(FVector(1.8f, 0.9f, 0.45f));
    TrainBody->SetRelativeLocation(FVector(0.0f, 0.0f, -35.0f));

    RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
    RideCamera->SetupAttachment(TrainRoot);
    RideCamera->SetRelativeLocation(FVector(90.0f, 0.0f, 55.0f));
    RideCamera->SetFieldOfView(86.0f);
    RideCamera->bUsePawnControlRotation = false;

    LeftRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftRail"));
    LeftRail->SetupAttachment(SceneRoot);
    RightRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightRail"));
    RightRail->SetupAttachment(SceneRoot);
    Ties = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ties"));
    Ties->SetupAttachment(SceneRoot);
    Supports = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Supports"));
    Supports->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        TrainBody->SetStaticMesh(CubeMesh.Object);
        LeftRail->SetStaticMesh(CubeMesh.Object);
        RightRail->SetStaticMesh(CubeMesh.Object);
        Ties->SetStaticMesh(CubeMesh.Object);
        Supports->SetStaticMesh(CubeMesh.Object);
    }

    CurrentSpeedCms = 450.0f;
}

void ACoasterRideActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureDefaultTrack();
    RebuildSpline();
    RebuildVisuals();
}

void ACoasterRideActor::BeginPlay()
{
    Super::BeginPlay();
    EnsureDefaultTrack();
    RebuildSpline();
    CurrentDistanceCm = 0.0f;
    CurrentSpeedCms = 250.0f;
    LastVelocityCms = FVector::ZeroVector;
}

void ACoasterRideActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AdvanceRide(DeltaSeconds);
}

void ACoasterRideActor::EnsureDefaultTrack()
{
    if (!ControlPoints.IsEmpty())
    {
        return;
    }

    ControlPoints = {
        FVector(0.0f, 0.0f, 350.0f),
        FVector(900.0f, 0.0f, 1150.0f),
        FVector(1700.0f, 250.0f, 3100.0f),
        FVector(2500.0f, 500.0f, 4550.0f),
        FVector(3600.0f, 300.0f, 1250.0f),
        FVector(4300.0f, -1050.0f, 420.0f),
        FVector(3000.0f, -2300.0f, 1500.0f),
        FVector(1100.0f, -2450.0f, 850.0f),
        FVector(-900.0f, -1450.0f, 1850.0f),
        FVector(-1550.0f, 600.0f, 780.0f),
        FVector(-600.0f, 1250.0f, 420.0f)
    };
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

        LeftRail->AddInstance(MakeSegmentTransform(LocationA - RightA * RailHalfGauge, LocationB - RightB * RailHalfGauge, FVector(SegmentStep, 16.0f, 16.0f)));
        RightRail->AddInstance(MakeSegmentTransform(LocationA + RightA * RailHalfGauge, LocationB + RightB * RailHalfGauge, FVector(SegmentStep, 16.0f, 16.0f)));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += TieStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector TieStart = Location - Right * (RailHalfGauge + 45.0f);
        const FVector TieEnd = Location + Right * (RailHalfGauge + 45.0f);
        Ties->AddInstance(MakeSegmentTransform(TieStart, TieEnd, FVector(RailGaugeCm + 90.0f, 24.0f, 18.0f)));
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
            const FVector Foot = FVector(Location.X, Location.Y, 0.0f);
            Supports->AddInstance(MakeSegmentTransform(Foot, Location - Up * 80.0f, FVector(Location.Z, 18.0f, 18.0f)));
        }
    }
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
        DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 1.2f, -120.0f, 360.0f);
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

    const FMatrix FrameMatrix(
        FPlane(OutForward, 0.0f),
        FPlane(OutRight, 0.0f),
        FPlane(OutUp, 0.0f),
        FPlane(0.0f, 0.0f, 0.0f, 1.0f));
    OutRotation = FQuat(FrameMatrix).Rotator();
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
