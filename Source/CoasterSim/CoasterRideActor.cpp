#include "CoasterRideActor.h"

#include "CoasterBanking.h"
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
#include "Engine/Scene.h"
#include "Engine/Engine.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

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
    RideCamera->SetRelativeLocation(FVector(-146.0f, 0.0f, 372.0f));
    // Both hero refs (02/03) look distinctly DOWN into the gorge at the river and
    // valley floor (~-15 to -25 deg), not across at the near hillside. The old
    // -4 deg framed flat pale mid-slope terrain that reads as greybox; pitching
    // down swaps it for the textured near canyon wall + valley depth.
    RideCamera->SetRelativeRotation(FRotator(-7.5f, 0.0f, 0.0f));
    RideCamera->SetFieldOfView(86.0f);
    RideCamera->bUsePawnControlRotation = false;
    RideCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    RideCamera->PostProcessSettings.MotionBlurAmount = 0.025f;
    RideCamera->PostProcessSettings.bOverride_VignetteIntensity = true;
    RideCamera->PostProcessSettings.VignetteIntensity = 0.18f;
    RideCamera->PostProcessSettings.bOverride_ColorSaturation = true;
    RideCamera->PostProcessSettings.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorContrast = true;
    RideCamera->PostProcessSettings.ColorContrast = FVector4(1.02f, 1.02f, 1.02f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorGamma = true;
    RideCamera->PostProcessSettings.ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_ColorGain = true;
    RideCamera->PostProcessSettings.ColorGain = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    RideCamera->PostProcessSettings.bOverride_WhiteTemp = true;
    RideCamera->PostProcessSettings.WhiteTemp = 6500.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    RideCamera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    RideCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    RideCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = true;
    RideCamera->PostProcessSettings.bOverride_CameraShutterSpeed = true;
    RideCamera->PostProcessSettings.CameraShutterSpeed = 500.0f;
    RideCamera->PostProcessSettings.bOverride_CameraISO = true;
    RideCamera->PostProcessSettings.CameraISO = 100.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
    // Reference-matched daylight: the canyon needs bright sky but dark forested
    // slopes. The old -0.25 bias still washed the mountain albedo into pale
    // mint; -1.05 keeps sunlit cloud/sky readable while restoring hillside mass.
    RideCamera->PostProcessSettings.AutoExposureBias = -1.05f;
    RideCamera->PostProcessSettings.bOverride_FilmSlope = true;
    RideCamera->PostProcessSettings.FilmSlope = 0.86f;
    RideCamera->PostProcessSettings.bOverride_FilmToe = true;
    RideCamera->PostProcessSettings.FilmToe = 0.22f;
    RideCamera->PostProcessSettings.bOverride_FilmShoulder = true;
    RideCamera->PostProcessSettings.FilmShoulder = 0.42f;
    RideCamera->PostProcessSettings.bOverride_FilmGrainIntensity = true;
    RideCamera->PostProcessSettings.FilmGrainIntensity = 0.035f;
    RideCamera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
    RideCamera->PostProcessSettings.SceneFringeIntensity = 0.05f;
    // A landscape coaster's hero subject is the km-distant canyon, not anything
    // 42m away. The old DoF focused at 4200cm actively blurred the vista — the
    // opposite of an epic cinematic wide shot. Keep the whole frame sharp.
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldEnabled = true;
    RideCamera->PostProcessSettings.DepthOfFieldEnabled = false;

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
    Ties = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ties"));
    Ties->SetupAttachment(SceneRoot);
    Supports = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Supports"));
    Supports->SetupAttachment(SceneRoot);
    CoasterTrackVisuals::ConfigureMeshes(LeftRail, RightRail, Ties, Supports);

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
        CoasterTrackVisuals::SetVisible(LeftRail, RightRail, Ties, Supports, false);
    }

    YarlungAtmosphere::ApplyCommandLineOverrides(RideCamera, SkyLight, SunLight, ValleyFog, VolumetricClouds);
    StartRideFromCommandLine(0.34f, 18.0f);
    ConfigureBatchScreenshotsFromCommandLine();
}

void ACoasterRideActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bBatchScreenshotsActive)
    {
        TickBatchScreenshots();
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
        const float ClampedRatio = FMath::Clamp(TrackRatio, 0.0f, 0.999f);
        const float StartDistanceCm = TrackLengthCm * ClampedRatio;
        const float AdvanceCm = FMath::Max(SpeedMps, NumericalStallFloorMps) * CmPerMeter * StartSeconds;
        float AdvancedRatio = FMath::Fmod((StartDistanceCm + AdvanceCm) / TrackLengthCm, 1.0f);
        if (AdvancedRatio < 0.0f)
        {
            AdvancedRatio += 1.0f;
        }
        StartRideAt(AdvancedRatio, SpeedMps);
        return;
    }

    StartRideAt(TrackRatio, SpeedMps);
}

bool ACoasterRideActor::ConfigureBatchScreenshotsFromCommandLine()
{
    const TCHAR* CommandLine = FCommandLine::Get();
    FString TimesValue;
    if (!FParse::Value(CommandLine, TEXT("YarlungBatchShotTimes="), TimesValue))
    {
        return false;
    }

    TimesValue.ReplaceInline(TEXT(","), TEXT("+"));
    TimesValue.ReplaceInline(TEXT(";"), TEXT("+"));
    TArray<FString> Tokens;
    TimesValue.ParseIntoArray(Tokens, TEXT("+"), true);
    for (FString& Token : Tokens)
    {
        Token.TrimStartAndEndInline();
        if (!Token.IsEmpty())
        {
            BatchScreenshotTimes.Add(FCString::Atof(*Token));
        }
    }

    if (BatchScreenshotTimes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("YarlungBatchShotTimes was provided but contained no usable times: %s"), *TimesValue);
        return false;
    }

    BatchScreenshotOutputDir = FPaths::ProjectSavedDir() / TEXT("OffscreenShots");
    FParse::Value(CommandLine, TEXT("YarlungBatchShotDir="), BatchScreenshotOutputDir);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotPrefix="), BatchScreenshotPrefix);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotResX="), BatchScreenshotResX);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotResY="), BatchScreenshotResY);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotSettleFrames="), BatchScreenshotSettleFrames);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotPostFrames="), BatchScreenshotPostFrames);

    BatchScreenshotResX = FMath::Max(BatchScreenshotResX, 320);
    BatchScreenshotResY = FMath::Max(BatchScreenshotResY, 180);
    BatchScreenshotSettleFrames = FMath::Clamp(BatchScreenshotSettleFrames, 1, 60);
    BatchScreenshotPostFrames = FMath::Clamp(BatchScreenshotPostFrames, 1, 120);
    FPaths::NormalizeDirectoryName(BatchScreenshotOutputDir);
    IFileManager::Get().MakeDirectory(*BatchScreenshotOutputDir, true);

    BatchScreenshotIndex = 0;
    BatchScreenshotStage = 0;
    BatchScreenshotFramesRemaining = 0;
    bBatchScreenshotsActive = true;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung batch screenshots enabled: count=%d prefix=%s output=%s res=%dx%d settle=%d post=%d"),
        BatchScreenshotTimes.Num(),
        *BatchScreenshotPrefix,
        *BatchScreenshotOutputDir,
        BatchScreenshotResX,
        BatchScreenshotResY,
        BatchScreenshotSettleFrames,
        BatchScreenshotPostFrames);
    return true;
}

void ACoasterRideActor::TickBatchScreenshots()
{
    if (BatchScreenshotIndex >= BatchScreenshotTimes.Num())
    {
        bBatchScreenshotsActive = false;
        FPlatformMisc::RequestExit(false);
        return;
    }

    if (BatchScreenshotStage == 0)
    {
        PositionRideForCommandLineSeconds(BatchScreenshotTimes[BatchScreenshotIndex]);
        BatchScreenshotFramesRemaining = BatchScreenshotSettleFrames;
        BatchScreenshotStage = 1;
        return;
    }

    if (BatchScreenshotStage == 1)
    {
        --BatchScreenshotFramesRemaining;
        if (BatchScreenshotFramesRemaining > 0)
        {
            return;
        }
        RequestCurrentBatchScreenshot();
        BatchScreenshotFramesRemaining = BatchScreenshotPostFrames;
        BatchScreenshotStage = 2;
        return;
    }

    --BatchScreenshotFramesRemaining;
    if (BatchScreenshotFramesRemaining > 0)
    {
        return;
    }

    ++BatchScreenshotIndex;
    BatchScreenshotStage = 0;
}

void ACoasterRideActor::PositionRideForCommandLineSeconds(float StartSeconds)
{
    float TrackRatio = 0.34f;
    float SpeedMps = 18.0f;
    const TCHAR* CommandLine = FCommandLine::Get();
    FParse::Value(CommandLine, TEXT("CoasterStartRatio="), TrackRatio);
    FParse::Value(CommandLine, TEXT("CoasterStartSpeed="), SpeedMps);

    const float ClampedRatio = FMath::Clamp(TrackRatio, 0.0f, 0.999f);
    const float StartDistanceCm = TrackLengthCm * ClampedRatio;
    const float AdvanceCm = FMath::Max(SpeedMps, NumericalStallFloorMps) * CmPerMeter * StartSeconds;
    float AdvancedRatio = FMath::Fmod((StartDistanceCm + AdvanceCm) / TrackLengthCm, 1.0f);
    if (AdvancedRatio < 0.0f)
    {
        AdvancedRatio += 1.0f;
    }
    StartRideAt(AdvancedRatio, SpeedMps);
}

void ACoasterRideActor::RequestCurrentBatchScreenshot()
{
    if (BatchScreenshotIndex >= BatchScreenshotTimes.Num())
    {
        return;
    }

    const int32 TimeLabel = FMath::RoundToInt(BatchScreenshotTimes[BatchScreenshotIndex]);
    const FString Filename = BatchScreenshotOutputDir / FString::Printf(TEXT("%s-t%d.png"), *BatchScreenshotPrefix, TimeLabel);
    FString StandardFilename = Filename;
    FPaths::MakeStandardFilename(StandardFilename);
    UE_LOG(LogTemp, Display, TEXT("Yarlung batch screenshot %d/%d at t=%.2fs -> %s"), BatchScreenshotIndex + 1, BatchScreenshotTimes.Num(), BatchScreenshotTimes[BatchScreenshotIndex], *StandardFilename);
    FScreenshotRequest::RequestScreenshot(StandardFilename, false, false);
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
    CoasterTrackVisuals::ApplyMaterials(LeftRail, RightRail, Ties, Supports);
    CoasterTrackVisuals::Rebuild(
        LeftRail,
        RightRail,
        Ties,
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
    RideCamera->SetRelativeLocation(FVector(-146.0f, 0.0f, 372.0f));
    // Keep in sync with the constructor: this runs every frame and governs the
    // running-shot framing. Look down into the gorge to match hero refs 02/03.
    RideCamera->SetRelativeRotation(FRotator(-7.5f, 0.0f, 0.0f));
}

void ACoasterRideActor::SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const
{
    const float WrappedDistance = FMath::Fmod(FMath::Max(DistanceCm, 0.0f), TrackLengthCm);
    TrackSpline->SampleBaseFrame(WrappedDistance, OutLocation, OutRotation, OutForward, OutRight, OutUp);
    CoasterBanking::ApplyBank(TrackSpline->GetGeneratedBankRadiansAtDistance(WrappedDistance), OutForward, OutRight, OutUp);

    OutRotation = FRotationMatrix::MakeFromXZ(OutForward, OutUp).Rotator();
}
