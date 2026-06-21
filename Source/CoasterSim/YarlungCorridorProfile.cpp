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
    const float Along = Center.X * 0.00024f + Center.Y * 0.00020f;
    const float Across = AbsOffset * 0.000026f;

    const float WallMask = Smooth01((AbsOffset - 36000.0f) / 52000.0f)
        * (1.0f - Smooth01((AbsOffset - 210000.0f) / 52000.0f));
    const float Buttress =
        0.68f * FMath::Sin(Along * 0.92f + Side * 1.1f)
        + 0.32f * FMath::Sin(Along * 1.48f - Across * 0.55f + Side * 2.2f);
    const float RavineSignal = 0.5f + 0.5f * FMath::Sin(Along * 1.38f + Across * 0.72f - Side * 0.7f);
    const float Ravine = FMath::Pow(FMath::Clamp(RavineSignal, 0.0f, 1.0f), 5.5f);
    const float TalusApron = YarlungTerrain::Smooth01((AbsOffset - 42000.0f) / 46000.0f)
        * (1.0f - YarlungTerrain::Smooth01((AbsOffset - 126000.0f) / 52000.0f));
    const float MacroBreakup = WallMask * (Buttress * 2100.0f - Ravine * 2100.0f - TalusApron * 900.0f);

    return BaseHeight + FMath::Clamp(MacroBreakup, -4200.0f, 4200.0f);
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
