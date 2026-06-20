#pragma once

#include "CoreMinimal.h"

namespace CoasterBanking
{
inline void ApplyBank(float BankRadians, const FVector& Forward, FVector& Right, FVector& Up)
{
    const FQuat BankQuat(Forward, BankRadians);
    Right = BankQuat.RotateVector(Right).GetSafeNormal();
    Up = BankQuat.RotateVector(Up).GetSafeNormal();
}
}
