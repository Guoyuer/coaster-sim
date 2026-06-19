#pragma once

#include "CoreMinimal.h"

namespace YarlungCoaster
{
constexpr float TrackClearanceOffsetCm = 1580.0f;
constexpr float TrackClearanceOuterRadiusCm = 3300.0f;
constexpr float TrackClearanceInnerRadiusCm = 1450.0f;

inline float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

inline const TArray<FVector>& DefaultTrackControlPoints()
{
    static const TArray<FVector> Points = {
        FVector(87043.0f, -147129.7f, 266910.5f),
        FVector(92343.0f, -148529.7f, 267500.0f),
        FVector(99743.0f, -146629.7f, 269200.0f),
        FVector(105343.0f, -141129.7f, 273405.1f),
        FVector(103143.0f, -135229.7f, 273898.5f),
        FVector(96443.0f, -133129.7f, 271391.0f),
        FVector(87943.0f, -135629.7f, 271904.3f),
        FVector(83743.0f, -141429.7f, 274685.5f)
    };
    return Points;
}

inline float DistanceToSegment2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B, float& OutT)
{
    const FVector2D AB = B - A;
    const float Denom = FMath::Max(AB.SizeSquared(), 1.0f);
    OutT = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / Denom, 0.0f, 1.0f);
    return (Point - (A + AB * OutT)).Size();
}

inline float DistanceToTrack2D(float X, float Y, float* OutTrackZ = nullptr)
{
    const TArray<FVector>& TrackPoints = DefaultTrackControlPoints();
    const FVector2D Point(X, Y);
    float BestDistance = TNumericLimits<float>::Max();
    float BestTrackZ = 0.0f;

    for (int32 Index = 0; Index < TrackPoints.Num(); ++Index)
    {
        const FVector& A3 = TrackPoints[Index];
        const FVector& B3 = TrackPoints[(Index + 1) % TrackPoints.Num()];
        float T = 0.0f;
        const float Distance = DistanceToSegment2D(Point, FVector2D(A3.X, A3.Y), FVector2D(B3.X, B3.Y), T);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestTrackZ = FMath::Lerp(A3.Z, B3.Z, T);
        }
    }

    if (OutTrackZ)
    {
        *OutTrackZ = BestTrackZ;
    }
    return BestDistance;
}

inline float ApplyTrackClearanceCut(float X, float Y, float Height)
{
    float TrackZ = Height;
    const float Distance = DistanceToTrack2D(X, Y, &TrackZ);
    if (Distance > TrackClearanceOuterRadiusCm)
    {
        return Height;
    }

    const float ClearanceTarget = TrackZ - TrackClearanceOffsetCm;
    const float BlendToOriginal = Smooth01(
        (Distance - TrackClearanceInnerRadiusCm) /
        (TrackClearanceOuterRadiusCm - TrackClearanceInnerRadiusCm));
    const float CutHeight = FMath::Min(Height, ClearanceTarget);
    return FMath::Lerp(CutHeight, Height, BlendToOriginal);
}
}
