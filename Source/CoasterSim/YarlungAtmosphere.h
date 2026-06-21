#pragma once

#include "CoreMinimal.h"

class UCameraComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;

namespace YarlungAtmosphere
{
void ConfigureComponents(
    USkyLightComponent* SkyLight,
    UDirectionalLightComponent* SunLight,
    USkyAtmosphereComponent* SkyAtmosphere,
    UVolumetricCloudComponent* VolumetricClouds,
    UExponentialHeightFogComponent* ValleyFog);

void BeginPlay(UVolumetricCloudComponent* VolumetricClouds, UObject* Owner);

bool AnchorFogToGeneratedRiver(UExponentialHeightFogComponent* ValleyFog);

void ApplyCommandLineOverrides(
    UCameraComponent* RideCamera,
    USkyLightComponent* SkyLight,
    UDirectionalLightComponent* SunLight,
    UExponentialHeightFogComponent* ValleyFog,
    UVolumetricCloudComponent* VolumetricClouds);
}
