#pragma once

#include "CoreMinimal.h"

namespace YarlungTerrain
{
constexpr float EncodedMinZCm = 260000.0f;
constexpr float EncodedMaxZCm = 730000.0f;
constexpr float RiverAnchorXCm = 95543.0f;
constexpr float RiverAnchorYCm = -142330.0f;

inline float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

inline float HeightValueToCm(uint16 Encoded)
{
    return FMath::Lerp(EncodedMinZCm, EncodedMaxZCm, static_cast<float>(Encoded) / 65535.0f);
}

inline float NormalizeEncodedHeightCm(float HeightCm)
{
    return FMath::Clamp((HeightCm - EncodedMinZCm) / (EncodedMaxZCm - EncodedMinZCm), 0.0f, 1.0f);
}

inline float RiverCenterY(float X)
{
    const float OffsetX = X - RiverAnchorXCm;
    return RiverAnchorYCm
        + 9000.0f * FMath::Sin(OffsetX * 0.00009f + 0.25f)
        + 4200.0f * FMath::Sin(OffsetX * 0.00021f - 0.6f);
}
}
