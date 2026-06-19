#pragma once

#include "CoreMinimal.h"

namespace CoasterBanking
{
inline float LegacyBankRadians(float TrackRatio)
{
    return FMath::DegreesToRadians(28.0f * FMath::Sin(TrackRatio * UE_TWO_PI * 3.0f));
}

inline void ApplyBank(float BankRadians, const FVector& Forward, FVector& Right, FVector& Up)
{
    const FQuat BankQuat(Forward, BankRadians);
    Right = BankQuat.RotateVector(Right).GetSafeNormal();
    Up = BankQuat.RotateVector(Up).GetSafeNormal();
}
}
