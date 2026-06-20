#include "YarlungCorridorProfile.h"

namespace YarlungCorridorProfile
{
float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

float AuthoredHeightCm(const FVector2D& Center, float SignedOffsetCm, float TrackBaseHeight, float BaseHeight)
{
    const float AbsOffset = FMath::Abs(SignedOffsetCm);
    const float Side = SignedOffsetCm >= 0.0f ? 1.0f : -1.0f;
    const float Along = Center.X * 0.00032f + Center.Y * 0.00027f;
    const float Across = AbsOffset * 0.000044f;

    const float Talus = Smooth01((AbsOffset - 24000.0f) / 52000.0f);
    const float Wall = Smooth01((AbsOffset - 62000.0f) / 50000.0f);
    const float Skyline = Smooth01((AbsOffset - 104000.0f) / 24000.0f);
    const float ProfileRise = -6200.0f + Talus * 10400.0f + Wall * 18800.0f + Skyline * 7200.0f;

    const float WallMask = Smooth01((AbsOffset - 30000.0f) / 34000.0f)
        * (1.0f - Smooth01((AbsOffset - 112000.0f) / 18000.0f));
    const float Buttress =
        0.58f * FMath::Sin(Along * 1.35f + Side * 1.1f)
        + 0.42f * FMath::Sin(Along * 2.15f - Across * 1.15f + Side * 2.2f);
    const float RavineSignal = 0.5f + 0.5f * FMath::Sin(Along * 2.75f + Across * 2.0f - Side * 0.7f);
    const float Ravine = FMath::Pow(FMath::Clamp(RavineSignal, 0.0f, 1.0f), 4.0f);
    const float WetGully = FMath::Pow(
        FMath::Clamp(0.5f + 0.5f * FMath::Sin(Along * 4.2f - Across * 3.1f + Side * 1.8f), 0.0f, 1.0f),
        8.0f);
    const float MacroBreakup = WallMask * (Buttress * 3600.0f - Ravine * 6200.0f - WetGully * 2600.0f);

    const float AuthoredHeight = TrackBaseHeight + ProfileRise + MacroBreakup;
    const float DemBlend = 0.08f + 0.08f * Skyline;
    return FMath::Lerp(AuthoredHeight, BaseHeight, DemBlend);
}

float NearTrackBlend(float AbsOffsetCm)
{
    return 1.0f - Smooth01((AbsOffsetCm - 18000.0f) / 16000.0f);
}

float RideEnvelopeHeightCm(float TrackBaseHeight)
{
    return TrackBaseHeight - 9500.0f;
}
}
