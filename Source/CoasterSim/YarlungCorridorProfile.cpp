#include "YarlungCorridorProfile.h"

#include "YarlungTerrainProfile.h"

namespace YarlungCorridorProfile
{
using YarlungTerrain::Smooth01;

float CorridorTerrainHeightCm(const FVector2D& Center, float SignedOffsetCm, float TrackBaseHeight, float BaseHeight)
{
    const float AbsOffset = FMath::Abs(SignedOffsetCm);
    const float Side = SignedOffsetCm >= 0.0f ? 1.0f : -1.0f;
    const float Along = Center.X * 0.00024f + Center.Y * 0.00020f;
    const float Across = AbsOffset * 0.000026f;

    const float WallMask = Smooth01((AbsOffset - 58000.0f) / 105000.0f)
        * (1.0f - Smooth01((AbsOffset - 335000.0f) / 135000.0f));
    const float BackWallMask = Smooth01((AbsOffset - 142000.0f) / 115000.0f)
        * (1.0f - Smooth01((AbsOffset - 380000.0f) / 120000.0f));
    const float Buttress =
        0.68f * FMath::Sin(Along * 0.92f + Side * 1.1f)
        + 0.32f * FMath::Sin(Along * 1.48f - Across * 0.55f + Side * 2.2f);
    const float RavineSignal = 0.5f + 0.5f * FMath::Sin(Along * 1.18f + Across * 0.54f - Side * 0.7f);
    const float Ravine = FMath::Pow(FMath::Clamp(RavineSignal, 0.0f, 1.0f), 2.6f);
    const float TalusApron = YarlungTerrain::Smooth01((AbsOffset - 36000.0f) / 54000.0f)
        * (1.0f - YarlungTerrain::Smooth01((AbsOffset - 104000.0f) / 68000.0f));

    const float ValleyFloorCut = TalusApron * 5200.0f;
    const float WallLift = WallMask * (24000.0f + BackWallMask * 36000.0f);
    const float ButtressLift = WallMask * Buttress * 16000.0f;
    const float RavineCut = WallMask * Ravine * FMath::Lerp(6500.0f, 18000.0f, BackWallMask);
    const float TrackRelativeCap = FMath::Max(0.0f, TrackBaseHeight - BaseHeight + 128000.0f);
    const float MacroBreakup = FMath::Min(
        WallLift + ButtressLift - RavineCut - ValleyFloorCut,
        TrackRelativeCap);

    return BaseHeight + FMath::Clamp(MacroBreakup, -9000.0f, 86000.0f);
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
