#pragma once

#include "CoreMinimal.h"

namespace YarlungCorridorProfile
{
float Smooth01(float Value);
float AuthoredHeightCm(const FVector2D& Center, float SignedOffsetCm, float TrackBaseHeight, float BaseHeight);
float NearTrackBlend(float AbsOffsetCm);
float RideEnvelopeHeightCm(float TrackBaseHeight);
}
