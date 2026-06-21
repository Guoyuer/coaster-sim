#include "YarlungCorridorProfile.h"

#include "YarlungTerrainProfile.h"

namespace YarlungCorridorProfile
{
using YarlungTerrain::Smooth01;

float AuthoredHeightCm(const FVector2D& Center, float SignedOffsetCm, float TrackBaseHeight, float BaseHeight)
{
    (void)TrackBaseHeight;
    const float AbsOffset = FMath::Abs(SignedOffsetCm);
    const float Side = SignedOffsetCm >= 0.0f ? 1.0f : -1.0f;
    const float Along = Center.X * 0.00032f + Center.Y * 0.00027f;
    const float Across = AbsOffset * 0.000044f;

    const float WallMask = Smooth01((AbsOffset - 36000.0f) / 52000.0f)
        * (1.0f - Smooth01((AbsOffset - 210000.0f) / 52000.0f));
    const float Buttress =
        0.58f * FMath::Sin(Along * 1.35f + Side * 1.1f)
        + 0.42f * FMath::Sin(Along * 2.15f - Across * 1.15f + Side * 2.2f);
    const float RavineSignal = 0.5f + 0.5f * FMath::Sin(Along * 2.75f + Across * 2.0f - Side * 0.7f);
    const float Ravine = FMath::Pow(FMath::Clamp(RavineSignal, 0.0f, 1.0f), 4.0f);
    const float WetGully = FMath::Pow(
        FMath::Clamp(0.5f + 0.5f * FMath::Sin(Along * 4.2f - Across * 3.1f + Side * 1.8f), 0.0f, 1.0f),
        8.0f);
    const float MacroBreakup = WallMask * (Buttress * 3200.0f - Ravine * 4200.0f - WetGully * 1800.0f);

    return BaseHeight + FMath::Clamp(MacroBreakup, -6500.0f, 6500.0f);
}

float NearTrackBlend(float AbsOffsetCm)
{
    (void)AbsOffsetCm;
    return 0.0f;
}

float RideEnvelopeHeightCm(float TrackBaseHeight)
{
    return TrackBaseHeight;
}
}
