#include "YarlungAtmosphere.h"

#include "YarlungRiverField.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
void SetConsoleVariableIfAvailable(const TCHAR* Name, int32 Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByCode);
    }
}

bool LoadGeneratedRiverAverageZCm(float& OutZCm)
{
    FYarlungRiverField RiverField;
    FString Error;
    if (!RiverField.LoadGeneratedCsv(&Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read generated Yarlung river field for fog anchor: %s"), *Error);
        return false;
    }

    double SumZ = 0.0;
    const TArray<FYarlungRiverRow>& Rows = RiverField.GetRows();
    for (const FYarlungRiverRow& Row : Rows)
    {
        SumZ += Row.PositionCm.Z;
    }
    OutZCm = static_cast<float>(SumZ / static_cast<double>(Rows.Num()));
    return true;
}

void ConfigureCloudMaterial(UVolumetricCloudComponent* Clouds, UObject* Owner)
{
    if (!Clouds)
    {
        return;
    }
    if (!Clouds->IsVisible())
    {
        UE_LOG(LogTemp, Display, TEXT("Yarlung volumetric clouds are disabled; no cloud material is loaded."));
        return;
    }

    UMaterialInterface* BaseMaterial = Clouds->GetMaterial();
    if (!BaseMaterial)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Yarlung volumetric clouds are visible but have no explicit material. Assign a cloud material or disable clouds."));
    }

    UMaterialInstanceDynamic* CloudMID = UMaterialInstanceDynamic::Create(BaseMaterial, Owner);
    if (!CloudMID)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Unable to create dynamic Yarlung cloud material instance from '%s'."), *BaseMaterial->GetPathName());
    }

    CloudMID->SetScalarParameterValue(TEXT("Cloud_GlobalCoverage"), 0.02f);
    CloudMID->SetScalarParameterValue(TEXT("Cloud_GlobalDensity"), 0.015f);
    CloudMID->SetScalarParameterValue(TEXT("Layout_CloudGlobalScale"), 18.0f);
    CloudMID->SetScalarParameterValue(TEXT("CloudTextureWeight"), 0.82f);
    CloudMID->SetScalarParameterValue(TEXT("Noise_Strength"), 0.56f);
    CloudMID->SetScalarParameterValue(TEXT("Noise_Bias"), -0.10f);
    CloudMID->SetVectorParameterValue(TEXT("Cloud_AlbedoColor"), FLinearColor(0.96f, 0.97f, 0.93f));

    Clouds->SetMaterial(CloudMID);
    UE_LOG(LogTemp, Display, TEXT("Configured Yarlung volumetric clouds: coverage=0.02 density=0.015 scale=18.0"));
}
}

namespace YarlungAtmosphere
{
void ConfigureComponents(
    USkyLightComponent* SkyLight,
    UDirectionalLightComponent* SunLight,
    USkyAtmosphereComponent* SkyAtmosphere,
    UVolumetricCloudComponent* VolumetricClouds,
    UExponentialHeightFogComponent* ValleyFog)
{
    if (SkyLight)
    {
        SkyLight->SetIntensity(0.56f);
        SkyLight->SetRealTimeCapture(true);
    }

    if (SunLight)
    {
        SunLight->SetRelativeRotation(FRotator(-55.0f, -18.0f, 0.0f));
        SunLight->SetIntensity(100000.0f);
        SunLight->SetUseTemperature(true);
        SunLight->SetTemperature(6500.0f);
        SunLight->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f));
        SunLight->SetAtmosphereSunLight(true);
        SunLight->SetAtmosphereSunLightIndex(0);
    }

    if (SkyAtmosphere)
    {
        SkyAtmosphere->SetVisibility(true, true);
    }

    if (VolumetricClouds)
    {
        VolumetricClouds->SetVisibility(false, true);
        VolumetricClouds->SetLayerBottomAltitude(4.80f);
        VolumetricClouds->SetLayerHeight(1.45f);
        VolumetricClouds->SetTracingStartMaxDistance(170.0f);
        VolumetricClouds->SetTracingMaxDistance(150.0f);
        VolumetricClouds->SetbUsePerSampleAtmosphericLightTransmittance(true);
        VolumetricClouds->SetSkyLightCloudBottomOcclusion(0.10f);
        VolumetricClouds->SetViewSampleCountScale(2.35f);
        VolumetricClouds->SetShadowViewSampleCountScale(0.80f);
        VolumetricClouds->SetGroundAlbedo(FColor(34, 54, 43));
    }

    if (ValleyFog)
    {
        ValleyFog->SetRelativeLocation(FVector::ZeroVector);
        ValleyFog->SetFogDensity(0.000040f);
        ValleyFog->SetFogHeightFalloff(0.10f);
        ValleyFog->SetFogMaxOpacity(0.24f);
        ValleyFog->SetStartDistance(18000.0f);
        ValleyFog->SetFogInscatteringColor(FLinearColor(0.60f, 0.72f, 0.86f));
        ValleyFog->SetVolumetricFog(true);
        ValleyFog->SetVolumetricFogScatteringDistribution(0.30f);
        ValleyFog->SetVolumetricFogExtinctionScale(0.014f);
        ValleyFog->SetVolumetricFogDistance(120000.0f);
    }
}

void BeginPlay(UVolumetricCloudComponent* VolumetricClouds, UObject* Owner)
{
    SetConsoleVariableIfAvailable(TEXT("r.VolumetricCloud"), 1);
    SetConsoleVariableIfAvailable(TEXT("r.VolumetricCloud.ShadowMap"), 1);
    SetConsoleVariableIfAvailable(TEXT("r.VolumetricFog"), 1);
    ConfigureCloudMaterial(VolumetricClouds, Owner);
}

bool AnchorFogToGeneratedRiver(UExponentialHeightFogComponent* ValleyFog)
{
    if (!ValleyFog)
    {
        return false;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("YarlungHideFog")))
    {
        ValleyFog->SetVisibility(false, true);
        UE_LOG(LogTemp, Display, TEXT("Yarlung valley fog disabled by -YarlungHideFog."));
        return true;
    }

    float RiverSurfaceZCm = 0.0f;
    if (!LoadGeneratedRiverAverageZCm(RiverSurfaceZCm))
    {
        UE_LOG(LogTemp, Fatal, TEXT("Generated Yarlung river CSV is required for valley fog."));
        return false;
    }

    ValleyFog->SetVisibility(true, true);
    ValleyFog->SetRelativeLocation(FVector(0.0f, 0.0f, RiverSurfaceZCm + 70.0f));
    UE_LOG(LogTemp, Display, TEXT("Anchored valley fog to generated Yarlung river average Z: %.1fcm"), RiverSurfaceZCm);
    return true;
}

void ApplyCommandLineOverrides(
    UCameraComponent* RideCamera,
    USkyLightComponent* SkyLight,
    UDirectionalLightComponent* SunLight,
    UExponentialHeightFogComponent* ValleyFog,
    UVolumetricCloudComponent* VolumetricClouds)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    if (FParse::Param(CommandLine, TEXT("YarlungHideFog")) && ValleyFog)
    {
        ValleyFog->SetVisibility(false, true);
    }
    if (FParse::Param(CommandLine, TEXT("YarlungHideClouds")) && VolumetricClouds)
    {
        VolumetricClouds->SetVisibility(false, true);
    }

    float DiagExposureBias = 0.0f;
    if (RideCamera && FParse::Value(CommandLine, TEXT("YarlungExposureBias="), DiagExposureBias))
    {
        RideCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
        RideCamera->PostProcessSettings.AutoExposureBias = DiagExposureBias;
        UE_LOG(LogTemp, Display, TEXT("[diag] YarlungExposureBias=%.2f"), DiagExposureBias);
    }

    float DiagSkyLightIntensity = 0.0f;
    if (SkyLight && FParse::Value(CommandLine, TEXT("YarlungSkyLightIntensity="), DiagSkyLightIntensity))
    {
        SkyLight->SetIntensity(DiagSkyLightIntensity);
        UE_LOG(LogTemp, Display, TEXT("[diag] YarlungSkyLightIntensity=%.2f"), DiagSkyLightIntensity);
    }

    float DiagSunIntensity = 0.0f;
    if (SunLight && FParse::Value(CommandLine, TEXT("YarlungSunIntensity="), DiagSunIntensity))
    {
        SunLight->SetIntensity(DiagSunIntensity);
        UE_LOG(LogTemp, Display, TEXT("[diag] YarlungSunIntensity=%.2f"), DiagSunIntensity);
    }

    if (SkyLight)
    {
        SkyLight->RecaptureSky();
    }
}
}
