#pragma once

#include "CoreMinimal.h"

namespace YarlungDeterministicNoise
{
inline float Value01(float X, float Y)
{
    return FMath::Frac(FMath::Sin(X * 0.00173f + Y * 0.00291f) * 43758.5453f);
}

inline float Signed(float X, float Y)
{
    return Value01(X, Y) * 2.0f - 1.0f;
}
}
