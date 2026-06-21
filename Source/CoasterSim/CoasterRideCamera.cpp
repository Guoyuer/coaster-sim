#include "CoasterRideCamera.h"

#include "Camera/CameraComponent.h"
#include "Engine/Scene.h"

namespace
{
const FVector CameraOffsetCm(-146.0f, 0.0f, 372.0f);
const FRotator CameraLookRotation(-14.0f, 0.0f, 0.0f);
}

namespace CoasterRideCamera
{
void Configure(UCameraComponent* RideCamera)
{
    if (!RideCamera)
    {
        return;
    }

    ApplyRigTransform(RideCamera);
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
    RideCamera->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
    RideCamera->PostProcessSettings.DepthOfFieldFstop = 11.0f;
    RideCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
    RideCamera->PostProcessSettings.AutoExposureBias = -1.45f;
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
    RideCamera->PostProcessSettings.DepthOfFieldEnabled = false;
}

void ApplyRigTransform(UCameraComponent* RideCamera)
{
    if (!RideCamera)
    {
        return;
    }

    RideCamera->SetRelativeLocation(CameraOffsetCm);
    RideCamera->SetRelativeRotation(CameraLookRotation);
}
}
