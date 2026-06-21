#include "CoasterRideActor.h"

#include "YarlungRiverCsv.h"

#include "CoasterBanking.h"
#include "CoasterTrackComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Scene.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
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

bool LoadGeneratedRiverAverageZCm(float& OutZCm)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("Generated/YarlungLandscape/YarlungRiver.csv");
    TArray<FYarlungRiverRow> Rows;
    FString Error;
    if (!YarlungRiverCsv::Load(Path, Rows, &Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung river CSV for fog anchor: %s"), *Error);
        return false;
    }

    if (Rows.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung river CSV has no usable fog-anchor samples: %s"), *Path);
        return false;
    }

    double SumZ = 0.0;
    for (const FYarlungRiverRow& Row : Rows)
    {
        SumZ += Row.PositionCm.Z;
    }
    OutZCm = static_cast<float>(SumZ / static_cast<double>(Rows.Num()));
    return true;
}

void SetConsoleVariableIfAvailable(const TCHAR* Name, int32 Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByCode);
    }
}

void ConfigureYarlungCloudMaterial(UVolumetricCloudComponent* Clouds, UObject* Owner)
{
    if (!Clouds)
    {
        return;
    }

    UMaterialInterface* BaseMaterial = Clouds->GetMaterial();
    if (!BaseMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("Yarlung volumetric clouds have no material; sky will render cloudless."));
        return;
    }

    UMaterialInstanceDynamic* CloudMID = UMaterialInstanceDynamic::Create(BaseMaterial, Owner);
    if (!CloudMID)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unable to create dynamic Yarlung cloud material instance."));
        return;
    }

    CloudMID->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), 0.24f);
    CloudMID->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), 0.065f);
    CloudMID->SetScalarParameterValue(TEXT("Layout_CloudGlobalScale"), 18.0f);
    CloudMID->SetScalarParameterValue(TEXT("CloudTextureWeight"), 0.82f);
    CloudMID->SetScalarParameterValue(TEXT("Noise_Strength"), 0.56f);
    CloudMID->SetScalarParameterValue(TEXT("Noise_Bias"), -0.10f);
    CloudMID->SetVectorParameterValue(TEXT("Cloud_AlbedoColor"), FLinearColor(0.96f, 0.97f, 0.93f));

    Clouds->SetMaterial(CloudMID);
    UE_LOG(LogTemp, Display, TEXT("Configured Yarlung volumetric clouds: coverage=0.24 density=0.065 scale=18.0"));
}

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

    TrainBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrainBody"));
    TrainBody->SetupAttachment(TrainRoot);
    TrainBody->SetRelativeScale3D(FVector(1.22f, 0.60f, 0.14f));
    TrainBody->SetRelativeLocation(FVector(155.0f, 0.0f, -126.0f));

    RideCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RideCamera"));
    RideCamera->SetupAttachment(TrainRoot);
    RideCamera->SetRelativeLocation(FVector(-104.0f, 0.0f, 286.0f));
    RideCamera->SetRelativeRotation(FRotator(-4.0f, 0.0f, 0.0f));
    RideCamera->SetFieldOfView(78.0f);
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
    // Pivot cut #2: the previous +0.95 bias left the 120000-lux daylight scene
    // ~1 stop hot, blowing dark rock/terrain toward white and washing out the
    // new atmospheric perspective. Calibrate down toward physical daylight.
    RideCamera->PostProcessSettings.AutoExposureBias = -0.25f;
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
    // Real-time-capture skylight: intensity is a multiplier on the captured
    // physical sky. 3.0 over-filled shadows, flattening the scene. ~1.1 keeps
    // ambient physically plausible and restores cinematic shadow contrast.
    SkyLight->SetIntensity(1.1f);
    SkyLight->SetRealTimeCapture(true);

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);
    SunLight->SetRelativeRotation(FRotator(-55.0f, -18.0f, 0.0f));
    SunLight->SetIntensity(120000.0f);
    SunLight->SetUseTemperature(true);
    SunLight->SetTemperature(6500.0f);
    SunLight->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f));
    SunLight->SetAtmosphereSunLight(true);
    SunLight->SetAtmosphereSunLightIndex(0);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(SceneRoot);

    VolumetricClouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricClouds"));
    VolumetricClouds->SetupAttachment(SceneRoot);
    VolumetricClouds->SetLayerBottomAltitude(3.90f);
    VolumetricClouds->SetLayerHeight(2.00f);
    VolumetricClouds->SetTracingStartMaxDistance(170.0f);
    VolumetricClouds->SetTracingMaxDistance(150.0f);
    VolumetricClouds->SetbUsePerSampleAtmosphericLightTransmittance(true);
    VolumetricClouds->SetSkyLightCloudBottomOcclusion(0.10f);
    VolumetricClouds->SetViewSampleCountScale(2.35f);
    VolumetricClouds->SetShadowViewSampleCountScale(0.80f);
    VolumetricClouds->SetGroundAlbedo(FColor(34, 54, 43));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CloudMaterial(
        TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));
    if (CloudMaterial.Succeeded())
    {
        VolumetricClouds->SetMaterial(CloudMaterial.Object);
    }

    ValleyFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ValleyFog"));
    ValleyFog->SetupAttachment(SceneRoot);
    ValleyFog->SetRelativeLocation(FVector::ZeroVector);
    ValleyFog->SetFogDensity(0.000040f);
    ValleyFog->SetFogHeightFalloff(0.16f);
    ValleyFog->SetFogMaxOpacity(0.18f);
    ValleyFog->SetStartDistance(6200.0f);
    ValleyFog->SetFogInscatteringColor(FLinearColor(0.58f, 0.70f, 0.82f));
    ValleyFog->SetVolumetricFog(true);
    ValleyFog->SetVolumetricFogScatteringDistribution(0.38f);
    ValleyFog->SetVolumetricFogExtinctionScale(0.032f);
    ValleyFog->SetVolumetricFogDistance(65000.0f);

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

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
    if (BasicMaterial.Succeeded())
    {
        TrainBody->SetMaterial(0, BasicMaterial.Object);
        LeftRail->SetMaterial(0, BasicMaterial.Object);
        RightRail->SetMaterial(0, BasicMaterial.Object);
        Ties->SetMaterial(0, BasicMaterial.Object);
        Supports->SetMaterial(0, BasicMaterial.Object);
    }

    CurrentSpeedCms = 450.0f;
}

void ACoasterRideActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildSpline();
    ApplyVisualMaterials();
    RebuildVisuals();
}

void ACoasterRideActor::BeginPlay()
{
    Super::BeginPlay();
    SetConsoleVariableIfAvailable(TEXT("r.VolumetricCloud"), 1);
    SetConsoleVariableIfAvailable(TEXT("r.VolumetricCloud.ShadowMap"), 1);
    SetConsoleVariableIfAvailable(TEXT("r.VolumetricFog"), 1);
    ConfigureYarlungCloudMaterial(VolumetricClouds, this);

    RebuildSpline();
    ApplyVisualMaterials();
    RebuildVisuals();

    const TCHAR* CommandLine = FCommandLine::Get();
    if (FParse::Param(CommandLine, TEXT("YarlungHideRide")))
    {
        TrainBody->SetVisibility(false, true);
        LeftRail->SetVisibility(false, true);
        RightRail->SetVisibility(false, true);
        Ties->SetVisibility(false, true);
        Supports->SetVisibility(false, true);
    }
    if (FParse::Param(CommandLine, TEXT("YarlungHideFog")))
    {
        ValleyFog->SetVisibility(false, true);
    }
    if (FParse::Param(CommandLine, TEXT("YarlungHideClouds")))
    {
        VolumetricClouds->SetVisibility(false, true);
    }
    // --- Pivot cut #2 diagnostics: localize the "styrofoam white" terrain ---
    // Terrain vertex albedo is already dark humid green-grey (~0.08), so the
    // white must come from the lighting/exposure chain. These command-line
    // knobs isolate which lever clips it, without re-baking anything.
    float DiagExposureBias = 0.0f;
    if (FParse::Value(CommandLine, TEXT("YarlungExposureBias="), DiagExposureBias))
    {
        RideCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
        RideCamera->PostProcessSettings.AutoExposureBias = DiagExposureBias;
        UE_LOG(LogTemp, Display, TEXT("[diag] YarlungExposureBias=%.2f"), DiagExposureBias);
    }
    float DiagSkyLightIntensity = 0.0f;
    if (FParse::Value(CommandLine, TEXT("YarlungSkyLightIntensity="), DiagSkyLightIntensity))
    {
        SkyLight->SetIntensity(DiagSkyLightIntensity);
        UE_LOG(LogTemp, Display, TEXT("[diag] YarlungSkyLightIntensity=%.2f"), DiagSkyLightIntensity);
    }
    float DiagSunIntensity = 0.0f;
    if (FParse::Value(CommandLine, TEXT("YarlungSunIntensity="), DiagSunIntensity))
    {
        SunLight->SetIntensity(DiagSunIntensity);
        UE_LOG(LogTemp, Display, TEXT("[diag] YarlungSunIntensity=%.2f"), DiagSunIntensity);
    }

    SkyLight->RecaptureSky();
    StartRideFromCommandLine(0.34f, 18.0f);
}

void ACoasterRideActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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

    float RiverSurfaceZCm = 0.0f;
    if (LoadGeneratedRiverAverageZCm(RiverSurfaceZCm))
    {
        ValleyFog->SetVisibility(true, true);
        ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, RiverSurfaceZCm + 70.0f));
        UE_LOG(LogTemp, Display, TEXT("Anchored valley fog to generated Yarlung river average Z: %.1fcm"), RiverSurfaceZCm);
        return;
    }

    ValleyFog->SetVisibility(false, true);
    UE_LOG(LogTemp, Error, TEXT("Generated Yarlung river CSV is required for valley fog; fog disabled."));
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
    const float SupportStep = 14400.0f;

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
            // Sink the feet well below the heightfield reference Z so they always
            // meet the visible Nanite terrain (which carries up to ~49 m of sub-30 m
            // rock displacement not present in the track CSV's terrain_z) instead of
            // floating above it. The buried length is occluded by the terrain surface
            // from the on-rails camera, so there is no visual cost.
            const float TerrainRefZ = TrackSpline->GetGeneratedTerrainZAtDistance(Distance);
            const float TerrainFootZ = TerrainRefZ - 6000.0f;
            const FVector LeftFoot = FVector(YokeLeft.X, YokeLeft.Y, TerrainFootZ);
            const FVector RightFoot = FVector(YokeRight.X, YokeRight.Y, TerrainFootZ);

            // The track is a high canyon viaduct (avg ~157 m, up to ~331 m above the
            // valley floor). Drop a few thick monumental piers, not thousands of
            // hair-thin threads: girth scales with the *visible* height so a 150 m
            // pier still reads as structure instead of a vertical streak.
            // (MakeSegmentTransform derives length from the endpoints, so ScaleCm.X is unused.)
            const float PierHeight = FMath::Max(YokeCenter.Z - TerrainRefZ, 1.0f);
            const float LegThickness = FMath::Clamp(PierHeight * 0.014f, 150.0f, 470.0f);

            Supports->AddInstance(MakeSegmentTransform(YokeLeft, YokeRight, FVector(0.0f, LegThickness * 1.25f, LegThickness)));
            Supports->AddInstance(MakeSegmentTransform(LeftFoot, YokeLeft, FVector(0.0f, LegThickness, LegThickness)));
            Supports->AddInstance(MakeSegmentTransform(RightFoot, YokeRight, FVector(0.0f, LegThickness, LegThickness)));

            // Horizontal cross-ties up the legs read as an engineered trestle ladder
            // and break up the otherwise blank tall pier face. Place them along the
            // visible span only (surface -> yoke), not the buried portion.
            const FVector LeftSurface = FVector(YokeLeft.X, YokeLeft.Y, TerrainRefZ);
            const FVector RightSurface = FVector(YokeRight.X, YokeRight.Y, TerrainRefZ);
            const int32 BraceCount = FMath::Clamp(FMath::FloorToInt(PierHeight / 9000.0f), 0, 2);
            for (int32 BraceIndex = 1; BraceIndex <= BraceCount; ++BraceIndex)
            {
                const float BraceT = static_cast<float>(BraceIndex) / static_cast<float>(BraceCount + 1);
                const FVector BraceLeft = FMath::Lerp(LeftSurface, YokeLeft, BraceT);
                const FVector BraceRight = FMath::Lerp(RightSurface, YokeRight, BraceT);
                Supports->AddInstance(MakeSegmentTransform(BraceLeft, BraceRight, FVector(0.0f, LegThickness * 0.5f, LegThickness * 0.5f)));
            }
        }
    }
}

void ACoasterRideActor::ApplyVisualMaterials()
{
    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));

    auto TintComponent = [TintMaterial](UMeshComponent* Component, const FLinearColor& Color, float Metallic, float Roughness)
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
        Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
        Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        Material->SetScalarParameterValue(TEXT("Specular"), 0.5f);
    };

    // Steel coaster: rails are bright polished steel, the structure is painted/
    // galvanized metal, ties read as weathered metal-composite. Rendering these
    // as flat non-metallic matte (old Roughness 0.88, Metallic 0) was the single
    // biggest reason the track itself never read as a real coaster.
    TintComponent(TrainBody, FLinearColor(0.16f, 0.015f, 0.012f), 0.55f, 0.32f); // deep red car shell
    TintComponent(LeftRail, FLinearColor(0.52f, 0.54f, 0.57f), 1.0f, 0.24f);     // polished running rail
    TintComponent(RightRail, FLinearColor(0.52f, 0.54f, 0.57f), 1.0f, 0.24f);
    TintComponent(Ties, FLinearColor(0.20f, 0.21f, 0.23f), 0.85f, 0.55f);        // galvanized cross-ties
    TintComponent(Supports, FLinearColor(0.26f, 0.30f, 0.34f), 0.9f, 0.45f);     // painted steel columns
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
    RideCamera->SetRelativeLocation(FVector(-104.0f, 0.0f, 286.0f));
    RideCamera->SetRelativeRotation(FRotator(-4.0f, 0.0f, 0.0f));
}

void ACoasterRideActor::SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const
{
    const float WrappedDistance = FMath::Fmod(FMath::Max(DistanceCm, 0.0f), TrackLengthCm);
    TrackSpline->SampleBaseFrame(WrappedDistance, OutLocation, OutRotation, OutForward, OutRight, OutUp);
    CoasterBanking::ApplyBank(TrackSpline->GetGeneratedBankRadiansAtDistance(WrappedDistance), OutForward, OutRight, OutUp);

    OutRotation = FRotationMatrix::MakeFromXZ(OutForward, OutUp).Rotator();
}
