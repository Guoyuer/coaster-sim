#include "YarlungViewCorridor.h"

#include "YarlungTerrainProfile.h"

namespace YarlungViewCorridor
{
float DistanceToTrackCm(const TArray<FTrackPoint>& TrackPoints, const FVector2D& Position)
{
    if (TrackPoints.Num() < 2)
    {
        return TNumericLimits<float>::Max();
    }

    float BestSquared = TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < TrackPoints.Num(); ++Index)
    {
        const FVector2D A = TrackPoints[Index].Position;
        const FVector2D B = TrackPoints[(Index + 1) % TrackPoints.Num()].Position;
        const FVector2D AB = B - A;
        const float LengthSquared = AB.SizeSquared();
        const float T = LengthSquared > KINDA_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(Position - A, AB) / LengthSquared, 0.0f, 1.0f)
            : 0.0f;
        const FVector2D Closest = A + AB * T;
        BestSquared = FMath::Min(BestSquared, FVector2D::DistSquared(Position, Closest));
    }

    return FMath::Sqrt(BestSquared);
}

float ComputeMask(const TArray<FTrackPoint>& TrackPoints, const FVector2D& Position, const FViewCorridorConfig& Config)
{
    if (TrackPoints.Num() < 3)
    {
        return 0.0f;
    }

    float BestMask = 0.0f;
    for (int32 Index = 0; Index < TrackPoints.Num(); ++Index)
    {
        const FVector2D Previous = TrackPoints[(Index + TrackPoints.Num() - 1) % TrackPoints.Num()].Position;
        const FVector2D Current = TrackPoints[Index].Position;
        const FVector2D Next = TrackPoints[(Index + 1) % TrackPoints.Num()].Position;
        const FVector2D Tangent = (Next - Previous).GetSafeNormal();
        if (Tangent.IsNearlyZero())
        {
            continue;
        }

        const FVector2D Relative = Position - Current;
        const float ForwardCm = FVector2D::DotProduct(Relative, Tangent);
        if (ForwardCm < -Config.BackwardFadeCm || ForwardCm > Config.FarStartCm + Config.FarFadeCm)
        {
            continue;
        }

        const float LateralCm = FMath::Abs(Tangent.X * Relative.Y - Tangent.Y * Relative.X);
        const float SideLimitCm = FMath::Clamp(
            Config.SideBaseCm + FMath::Max(ForwardCm, 0.0f) * Config.TanHalfFov,
            Config.SideBaseCm,
            Config.MaxSideCm);
        const float ForwardMask = YarlungTerrain::Smooth01((ForwardCm + Config.BackwardFadeCm) / Config.BackwardFadeCm)
            * (1.0f - YarlungTerrain::Smooth01((ForwardCm - Config.FarStartCm) / Config.FarFadeCm));
        const float SideMask = 1.0f - YarlungTerrain::Smooth01((LateralCm - SideLimitCm) / Config.SideFadeCm);
        BestMask = FMath::Max(BestMask, ForwardMask * SideMask);
        if (BestMask >= 0.999f)
        {
            return 1.0f;
        }
    }

    return FMath::Clamp(BestMask, 0.0f, 1.0f);
}
}
