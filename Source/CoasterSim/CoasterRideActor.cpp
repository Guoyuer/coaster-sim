#include "CoasterRideActor.h"

#include "CoasterBanking.h"
#include "CoasterTrackComponent.h"
#include "YarlungCoasterProfile.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float CmPerMeter = 100.0f;
constexpr float GravityCms2 = 980.665f;
constexpr float RiverZCm = 265200.0f;
constexpr float FallbackGeneratedRiverSurfaceZCm = 267655.0f;
constexpr float YarlungRiverAnchorXCm = 95543.0f;
constexpr float YarlungRiverAnchorYCm = -142330.0f;

FTransform MakeSegmentTransform(const FVector& Start, const FVector& End, const FVector& ScaleCm)
{
    const FVector Mid = (Start + End) * 0.5f;
    const FVector Delta = End - Start;
    const float Length = FMath::Max(Delta.Length(), 1.0f);
    const FRotator Rotation = FRotationMatrix::MakeFromX(Delta.GetSafeNormal()).Rotator();
    return FTransform(Rotation, Mid, FVector(Length / 100.0f, ScaleCm.Y / 100.0f, ScaleCm.Z / 100.0f));
}

using YarlungCoaster::Smooth01;

float Hash01(float A, float B)
{
    return FMath::Frac(FMath::Sin(A * 12.9898f + B * 78.233f) * 43758.5453f);
}

bool LoadGeneratedRiverAverageZCm(float& OutZCm)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("Generated/YarlungLandscape/YarlungRiver.csv");
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("Unable to read Yarlung river CSV for fog anchor: %s"), *Path);
        return false;
    }

    double SumZ = 0.0;
    int32 Count = 0;
    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        TArray<FString> Columns;
        Lines[LineIndex].ParseIntoArray(Columns, TEXT(","), true);
        if (Columns.Num() >= 4)
        {
            SumZ += FCString::Atod(*Columns[3]);
            ++Count;
        }
    }

    if (Count == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Yarlung river CSV has no usable fog-anchor samples: %s"), *Path);
        return false;
    }

    OutZCm = static_cast<float>(SumZ / static_cast<double>(Count));
    return true;
}

float YarlungRiverCenterY(float X)
{
    const float OffsetX = X - YarlungRiverAnchorXCm;
    return YarlungRiverAnchorYCm
        + 9000.0f * FMath::Sin(OffsetX * 0.00009f + 0.25f)
        + 4200.0f * FMath::Sin(OffsetX * 0.00021f - 0.6f);
}

float YarlungLandscapeHeight(float X, float Y)
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
    RideCamera->SetFieldOfView(92.0f);
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
    RideCamera->PostProcessSettings.AutoExposureBias = 1.2f;
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
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldEnabled = true;
    RideCamera->PostProcessSettings.DepthOfFieldEnabled = true;
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
    RideCamera->PostProcessSettings.DepthOfFieldFstop = 11.0f;
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    RideCamera->PostProcessSettings.DepthOfFieldFocalDistance = 4200.0f;

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetIntensity(3.0f);
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

    ValleyFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ValleyFog"));
    ValleyFog->SetupAttachment(SceneRoot);
    ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, FallbackGeneratedRiverSurfaceZCm + 70.0f));
    ValleyFog->SetFogDensity(0.000020f);
    ValleyFog->SetFogHeightFalloff(0.40f);
    ValleyFog->SetFogMaxOpacity(0.035f);
    ValleyFog->SetStartDistance(12000.0f);
    ValleyFog->SetFogInscatteringColor(FLinearColor(0.78f, 0.88f, 1.0f));
    ValleyFog->SetVolumetricFog(false);
    ValleyFog->SetVolumetricFogScatteringDistribution(0.28f);
    ValleyFog->SetVolumetricFogExtinctionScale(0.02f);
    ValleyFog->SetVolumetricFogDistance(22000.0f);

    LeftRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LeftRail"));
    LeftRail->SetupAttachment(SceneRoot);
    RightRail = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RightRail"));
    RightRail->SetupAttachment(SceneRoot);
    Ties = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ties"));
    Ties->SetupAttachment(SceneRoot);
    Supports = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Supports"));
    Supports->SetupAttachment(SceneRoot);
    BoulderOutcrops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoulderOutcrops"));
    BoulderOutcrops->SetupAttachment(SceneRoot);
    BoulderOutcrops->SetCastShadow(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        TrainBody->SetStaticMesh(CubeMesh.Object);
        LeftRail->SetStaticMesh(CubeMesh.Object);
        RightRail->SetStaticMesh(CubeMesh.Object);
        Ties->SetStaticMesh(CubeMesh.Object);
        Supports->SetStaticMesh(CubeMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> BoulderMesh(TEXT("/Game/Generated/Models/Boulder01/boulder_01_1k.boulder_01_1k"));
    if (BoulderMesh.Succeeded())
    {
        BoulderOutcrops->SetStaticMesh(BoulderMesh.Object);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
    if (BasicMaterial.Succeeded())
    {
        TrainBody->SetMaterial(0, BasicMaterial.Object);
        LeftRail->SetMaterial(0, BasicMaterial.Object);
        RightRail->SetMaterial(0, BasicMaterial.Object);
        Ties->SetMaterial(0, BasicMaterial.Object);
        Supports->SetMaterial(0, BasicMaterial.Object);
        BoulderOutcrops->SetMaterial(0, BasicMaterial.Object);
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
    SkyLight->RecaptureSky();
    StartRideFromCommandLine(0.34f, 18.0f);
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
        EnsureDefaultTrack();
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

    float RiverSurfaceZCm = FallbackGeneratedRiverSurfaceZCm;
    if (LoadGeneratedRiverAverageZCm(RiverSurfaceZCm))
    {
        UE_LOG(LogTemp, Display, TEXT("Anchored valley fog to generated Yarlung river average Z: %.1fcm"), RiverSurfaceZCm);
    }
    ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, RiverSurfaceZCm + 70.0f));
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
            const float TerrainFootZ = TrackSpline->GetGeneratedTerrainZAtDistance(Distance) - 35.0f;
            const FVector LeftFoot = FVector(YokeLeft.X, YokeLeft.Y, TerrainFootZ);
            const FVector RightFoot = FVector(YokeRight.X, YokeRight.Y, TerrainFootZ);

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
    BuildBoulderOutcrops();
}

void ACoasterRideActor::ClearEnvironmentVisuals()
{
    BoulderOutcrops->ClearInstances();
}

void ACoasterRideActor::BuildBoulderOutcrops()
{
    if (!BoulderOutcrops || !BoulderOutcrops->GetStaticMesh())
    {
        return;
    }

    constexpr int32 BoulderCount = 118;
    constexpr float MinX = -3200.0f;
    constexpr float MaxX = 11600.0f;

    for (int32 Index = 0; Index < BoulderCount; ++Index)
    {
        const float T = static_cast<float>(Index) / static_cast<float>(BoulderCount - 1);
        const float X = FMath::Lerp(MinX, MaxX, T) + (Hash01(Index * 2.9f, 5.1f) - 0.5f) * 520.0f;
        const float Side = (Index % 2 == 0) ? -1.0f : 1.0f;
        const float Lateral = Side * FMath::Lerp(780.0f, 2650.0f, Hash01(Index * 4.7f, 1.4f));
        const float Y = YarlungRiverCenterY(X) + Lateral;
        const float Height = YarlungLandscapeHeight(X, Y);
        if (Height < 120.0f || Height > 2550.0f)
        {
            continue;
        }

        const float Yaw = Hash01(Index * 7.2f, 8.4f) * 360.0f;
        const float Pitch = FMath::Lerp(-8.0f, 8.0f, Hash01(Index * 3.5f, 2.2f));
        const float Roll = FMath::Lerp(-6.0f, 6.0f, Hash01(Index * 5.8f, 9.6f));
        const float Scale = FMath::Lerp(0.42f, 1.08f, Hash01(Index * 1.8f, 3.1f));
        const FVector Location(X, Y, Height + 18.0f);
        BoulderOutcrops->AddInstance(FTransform(FRotator(Pitch, Yaw, Roll), Location, FVector(Scale)));
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

    TintComponent(TrainBody, FLinearColor(0.09f, 0.10f, 0.11f));
    TintComponent(LeftRail, FLinearColor(0.22f, 0.25f, 0.27f));
    TintComponent(RightRail, FLinearColor(0.22f, 0.25f, 0.27f));
    TintComponent(Ties, FLinearColor(0.13f, 0.12f, 0.105f));
    TintComponent(Supports, FLinearColor(0.20f, 0.24f, 0.27f));
    TintComponent(BoulderOutcrops, FLinearColor(0.20f, 0.23f, 0.20f));
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

    const FName SectionName = GetSectionName(CurrentDistanceCm);
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
    else if (SectionName == TEXT("Turnaround"))
    {
        const float Target = PoweredTurnaroundTargetSpeedMps * CmPerMeter;
        if (CurrentSpeedCms < Target)
        {
            DriveAccel = FMath::Clamp(
                (Target - CurrentSpeedCms) * 1.4f,
                0.0f,
                PoweredDriveMaxAccelMps2 * CmPerMeter);
        }
        else
        {
            BrakeAccel = FMath::Min(
                (CurrentSpeedCms - Target) * 1.1f,
                PoweredBrakeMaxAccelMps2 * CmPerMeter);
        }
    }
    else
    {
        const float Target = PoweredCruiseTargetSpeedMps * CmPerMeter;
        DriveAccel = FMath::Clamp(
            (Target - CurrentSpeedCms) * 1.1f,
            0.0f,
            PoweredDriveMaxAccelMps2 * CmPerMeter);
    }

    const float NetAccel = DriveAccel + GravityAccel - DragAccel - RollingAccel - BrakeAccel;
    CurrentSpeedCms = FMath::Clamp(CurrentSpeedCms + NetAccel * DeltaSeconds, NumericalStallFloorMps * CmPerMeter, 5600.0f);
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
    TrackSpline->SampleBaseFrame(WrappedDistance, OutLocation, OutRotation, OutForward, OutRight, OutUp);
    CoasterBanking::ApplyBank(TrackSpline->GetGeneratedBankRadiansAtDistance(WrappedDistance), OutForward, OutRight, OutUp);

    OutRotation = FRotationMatrix::MakeFromXZ(OutForward, OutUp).Rotator();
}

FName ACoasterRideActor::GetSectionName(float DistanceCm) const
{
    return TrackSpline->GetSectionNameAtDistance(DistanceCm);
}
