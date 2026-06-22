#include "CoasterRideActor.h"

#include "CoasterBanking.h"
#include "CoasterRideCamera.h"
#include "CoasterTrackVisuals.h"
#include "CoasterTrackComponent.h"
#include "YarlungAtmosphere.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
constexpr float CmPerMeter = 100.0f;
constexpr float GravityCms2 = 980.665f;

}

ACoasterRideActor::ACoasterRideActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    TrackSpline = CreateDefaultSubobject<UCoasterTrackComponent>(TEXT("TrackSpline"));
    TrackSpline->SetupAttachment(SceneRoot);
    TrackSpline->SetClosedLoop(true);

    TrainRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TrainRoot"));
    TrainRoot->SetupAttachment(SceneRoot);

    RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
    RideCamera->SetupAttachment(TrainRoot);
    CoasterRideCamera::Configure(RideCamera);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(SceneRoot);

    VolumetricClouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricClouds"));
    VolumetricClouds->SetupAttachment(SceneRoot);

    ValleyFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ValleyFog"));
    ValleyFog->SetupAttachment(SceneRoot);
    YarlungAtmosphere::ConfigureComponents(SkyLight, SunLight, SkyAtmosphere, VolumetricClouds, ValleyFog);

    LeftRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftRail"));
    LeftRail->SetupAttachment(SceneRoot);
    RightRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightRail"));
    RightRail->SetupAttachment(SceneRoot);
    CenterSpine = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CenterSpine"));
    CenterSpine->SetupAttachment(SceneRoot);
    LeftGuardRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftGuardRail"));
    LeftGuardRail->SetupAttachment(SceneRoot);
    RightGuardRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightGuardRail"));
    RightGuardRail->SetupAttachment(SceneRoot);
    Ties = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ties"));
    Ties->SetupAttachment(SceneRoot);
    TrackBraces = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrackBraces"));
    TrackBraces->SetupAttachment(SceneRoot);
    Supports = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Supports"));
    Supports->SetupAttachment(SceneRoot);
    CoasterTrackVisuals::ConfigureMeshes(LeftRail, RightRail, CenterSpine, LeftGuardRail, RightGuardRail, Ties, TrackBraces, Supports);

    CurrentSpeedCms = 450.0f;
}

void ACoasterRideActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildSpline();
    RebuildVisuals();
}

void ACoasterRideActor::BeginPlay()
{
    Super::BeginPlay();
    YarlungAtmosphere::BeginPlay(VolumetricClouds, this);

    RebuildSpline();
    RebuildVisuals();

    const TCHAR* CommandLine = FCommandLine::Get();
    if (FParse::Param(CommandLine, TEXT("YarlungHideRide")))
    {
        CoasterTrackVisuals::SetVisible(LeftRail, RightRail, CenterSpine, LeftGuardRail, RightGuardRail, Ties, TrackBraces, Supports, false);
    }

    YarlungAtmosphere::ApplyCommandLineOverrides(RideCamera, SkyLight, SunLight, ValleyFog, VolumetricClouds);
    StartRideFromCommandLine(0.34f, 18.0f);
    BatchCapture.ConfigureFromCommandLine();
}

void ACoasterRideActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (BatchCapture.Tick([this](float StartSeconds) { PositionRideForCommandLineSeconds(StartSeconds); }))
    {
        return;
    }
    AdvanceRide(DeltaSeconds);
}

void ACoasterRideActor::StartRideAt(float TrackRatio, float SpeedMps)
{
    RebuildSpline();

    const float ClampedRatio = FMath::Clamp(TrackRatio, 0.0f, 0.999f);
    CurrentDistanceCm = TrackLengthCm * ClampedRatio;
    CurrentSpeedCms = FMath::Max(SpeedMps, NumericalStallFloorMps) * CmPerMeter;
    LastVelocityCms = FVector::ZeroVector;

    FVector Location;
    FVector Forward;
    FVector Right;
    FVector Up;
    FRotator Rotation;
    SampleFrame(CurrentDistanceCm, Location, Rotation, Forward, Right, Up);
    TrainRoot->SetRelativeLocationAndRotation(Location, Rotation);
    UpdateFirstPersonCamera();

    Telemetry.SpeedMps = CurrentSpeedCms / CmPerMeter;
    Telemetry.HeightMeters = Location.Z / CmPerMeter;
    Telemetry.TrackDistanceMeters = CurrentDistanceCm / CmPerMeter;
    Telemetry.VerticalG = 1.0f;
    Telemetry.LateralG = 0.0f;
    Telemetry.LongitudinalG = 0.0f;
    Telemetry.SectionName = UCoasterTrackComponent::SectionName(TrackSpline->GetSectionAtDistance(CurrentDistanceCm));
}

void ACoasterRideActor::StartRideFromCommandLine(float DefaultTrackRatio, float DefaultSpeedMps)
{
    float TrackRatio = DefaultTrackRatio;
    float SpeedMps = DefaultSpeedMps;
    float StartSeconds = 0.0f;
    const TCHAR* CommandLine = FCommandLine::Get();
    FParse::Value(CommandLine, TEXT("CoasterStartRatio="), TrackRatio);
    FParse::Value(CommandLine, TEXT("CoasterStartSpeed="), SpeedMps);

    if (FParse::Value(CommandLine, TEXT("CoasterStartSeconds="), StartSeconds))
    {
        RebuildSpline();
        StartRideAt(ComputeAdvancedTrackRatio(TrackRatio, SpeedMps, StartSeconds), SpeedMps);
        return;
    }

    StartRideAt(TrackRatio, SpeedMps);
}

float ACoasterRideActor::ComputeAdvancedTrackRatio(float TrackRatio, float SpeedMps, float StartSeconds) const
{
    const float ClampedRatio = FMath::Clamp(TrackRatio, 0.0f, 0.999f);
    const float StartDistanceCm = TrackLengthCm * ClampedRatio;
    const float AdvanceCm = FMath::Max(SpeedMps, NumericalStallFloorMps) * CmPerMeter * StartSeconds;
    float AdvancedRatio = FMath::Fmod((StartDistanceCm + AdvanceCm) / TrackLengthCm, 1.0f);
    if (AdvancedRatio < 0.0f)
    {
        AdvancedRatio += 1.0f;
    }
    return AdvancedRatio;
}

void ACoasterRideActor::PositionRideForCommandLineSeconds(float StartSeconds)
{
    float TrackRatio = 0.34f;
    float SpeedMps = 18.0f;
    const TCHAR* CommandLine = FCommandLine::Get();
    FParse::Value(CommandLine, TEXT("CoasterStartRatio="), TrackRatio);
    FParse::Value(CommandLine, TEXT("CoasterStartSpeed="), SpeedMps);

    const float AdvancedRatio = ComputeAdvancedTrackRatio(TrackRatio, SpeedMps, StartSeconds);
    StartRideAt(AdvancedRatio, SpeedMps);
}

void ACoasterRideActor::RebuildSpline()
{
    const FString GeneratedTrackPath = FPaths::ProjectContentDir() / TEXT("Generated/YarlungLandscape/YarlungTrack.csv");
    if (!TrackSpline->LoadGeneratedTrack(GeneratedTrackPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to rebuild coaster spline because generated long-track CSV failed to load."));
        TrackLengthCm = 1.0f;
        return;
    }

    TrackLengthCm = TrackSpline->GetTrackLengthCm();
    YarlungAtmosphere::AnchorFogToGeneratedRiver(ValleyFog);
}

void ACoasterRideActor::RebuildVisuals()
{
    CoasterTrackVisuals::ApplyMaterials(LeftRail, RightRail, CenterSpine, LeftGuardRail, RightGuardRail, Ties, TrackBraces, Supports);
    CoasterTrackVisuals::Rebuild(
        LeftRail,
        RightRail,
        CenterSpine,
        LeftGuardRail,
        RightGuardRail,
        Ties,
        TrackBraces,
        Supports,
        TrackSpline,
        TrackLengthCm,
        RailGaugeCm,
        [this](float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp)
        {
            SampleFrame(DistanceCm, OutLocation, OutRotation, OutForward, OutRight, OutUp);
        });
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

    const ECoasterSection Section = TrackSpline->GetSectionAtDistance(CurrentDistanceCm);
    const float GravityAccel = FVector::DotProduct(FVector(0.0f, 0.0f, -GravityCms2), Forward);
    const float DragAccel = AeroDragCoefficient * CurrentSpeedCms * CurrentSpeedCms;
    const float RollingAccel = RollingResistanceCms2;

    float DriveAccel = 0.0f;
    float BrakeAccel = 0.0f;

    switch (Section)
    {
    case ECoasterSection::Lift:
    {
        const float Target = LiftTargetSpeedMps * CmPerMeter;
        DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 2.0f, 0.0f, 980.0f);
        break;
    }
    case ECoasterSection::Launch:
    {
        const float Target = LaunchTargetSpeedMps * CmPerMeter;
        DriveAccel = CurrentSpeedCms < Target ? 720.0f : 0.0f;
        break;
    }
    case ECoasterSection::Brake:
    {
        const float Target = BrakeTargetSpeedMps * CmPerMeter;
        BrakeAccel = CurrentSpeedCms > Target ? FMath::Min((CurrentSpeedCms - Target) * 1.1f, 850.0f) : 0.0f;
        break;
    }
    case ECoasterSection::Station:
    {
        const float Target = 4.0f * CmPerMeter;
        DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 1.0f, -200.0f, 180.0f);
        break;
    }
    case ECoasterSection::Turnaround:
    {
        const float Target = PoweredTurnaroundTargetSpeedMps * CmPerMeter;
        if (CurrentSpeedCms < Target)
        {
            DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 1.4f, 0.0f, PoweredDriveMaxAccelMps2 * CmPerMeter);
        }
        else
        {
            BrakeAccel = FMath::Min((CurrentSpeedCms - Target) * 1.1f, PoweredBrakeMaxAccelMps2 * CmPerMeter);
        }
        break;
    }
    default: // Outbound / Return / Coast → powered cruise
    {
        const float Target = PoweredCruiseTargetSpeedMps * CmPerMeter;
        DriveAccel = FMath::Clamp((Target - CurrentSpeedCms) * 1.1f, 0.0f, PoweredDriveMaxAccelMps2 * CmPerMeter);
        break;
    }
    }

    const float NetAccel = DriveAccel + GravityAccel - DragAccel - RollingAccel - BrakeAccel;
    CurrentSpeedCms = FMath::Clamp(CurrentSpeedCms + NetAccel * DeltaSeconds, NumericalStallFloorMps * CmPerMeter, MaxSpeedMps * CmPerMeter);
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
    Telemetry.SectionName = UCoasterTrackComponent::SectionName(Section);
}

void ACoasterRideActor::UpdateFirstPersonCamera()
{
    CoasterRideCamera::ApplyRigTransform(RideCamera);
}

void ACoasterRideActor::SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const
{
    const float WrappedDistance = FMath::Fmod(FMath::Max(DistanceCm, 0.0f), TrackLengthCm);
    TrackSpline->SampleBaseFrame(WrappedDistance, OutLocation, OutRotation, OutForward, OutRight, OutUp);
    CoasterBanking::ApplyBank(TrackSpline->GetGeneratedBankRadiansAtDistance(WrappedDistance), OutForward, OutRight, OutUp);

    OutRotation = FRotationMatrix::MakeFromXZ(OutForward, OutUp).Rotator();
}
