#include "YarlungTerrainSurface.h"

#include "YarlungCorridorProfile.h"
#include "YarlungRiverField.h"
#include "YarlungTerrainProfile.h"
#include "YarlungTerrainRelief.h"

namespace
{
float CubicBsplineInterp(float P0, float P1, float P2, float P3, float T)
{
    const float OneMinusT = 1.0f - T;
    const float W0 = OneMinusT * OneMinusT * OneMinusT / 6.0f;
    const float W1 = (3.0f * T * T * T - 6.0f * T * T + 4.0f) / 6.0f;
    const float W2 = (-3.0f * T * T * T + 3.0f * T * T + 3.0f * T + 1.0f) / 6.0f;
    const float W3 = T * T * T / 6.0f;
    return P0 * W0 + P1 * W1 + P2 * W2 + P3 * W3;
}

float HeightAtGrid(const TArray<uint16>& EncodedHeights, int32 XIndex, int32 YIndex)
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    const int32 ClampedX = FMath::Clamp(XIndex, 0, Size - 1);
    const int32 ClampedY = FMath::Clamp(YIndex, 0, Size - 1);
    return YarlungTerrain::HeightValueToCm(EncodedHeights[ClampedY * Size + ClampedX]);
}

float HeightAtSourceGrid(const TArray<uint16>& EncodedHeights, float SourceX, float SourceY)
{
    const int32 BaseX = FMath::FloorToInt(SourceX);
    const int32 BaseY = FMath::FloorToInt(SourceY);
    const float TX = SourceX - static_cast<float>(BaseX);
    const float TY = SourceY - static_cast<float>(BaseY);
    float Rows[4] = {};
    float MinNeighbor = TNumericLimits<float>::Max();
    float MaxNeighbor = -TNumericLimits<float>::Max();

    for (int32 Row = 0; Row < 4; ++Row)
    {
        float Samples[4] = {};
        for (int32 Column = 0; Column < 4; ++Column)
        {
            Samples[Column] = HeightAtGrid(EncodedHeights, BaseX + Column - 1, BaseY + Row - 1);
            MinNeighbor = FMath::Min(MinNeighbor, Samples[Column]);
            MaxNeighbor = FMath::Max(MaxNeighbor, Samples[Column]);
        }
        Rows[Row] = CubicBsplineInterp(Samples[0], Samples[1], Samples[2], Samples[3], TX);
    }

    return FMath::Clamp(
        CubicBsplineInterp(Rows[0], Rows[1], Rows[2], Rows[3], TY),
        MinNeighbor,
        MaxNeighbor);
}

float CarveRiverChannelCm(
    float HeightCm,
    const FVector2D& Position,
    const FYarlungRiverField& RiverField,
    YarlungTerrainSurface::FSurfaceStats* OutStats)
{
    const FYarlungRiverQuery River = RiverField.QueryNearest(Position);
    if (!River.bIsValid)
    {
        return HeightCm;
    }

    const float InnerBankCm = River.WaterHalfWidthCm + 4200.0f;
    const float OuterBankCm = River.WaterHalfWidthCm + 32000.0f;
    if (River.DistanceCm >= OuterBankCm)
    {
        return HeightCm;
    }

    const float BankT = YarlungTerrain::Smooth01((River.DistanceCm - InnerBankCm) / (OuterBankCm - InnerBankCm));
    const float CarveAlpha = 1.0f - BankT;

    // The bed profile is anchored to the *visible* water ribbon, not the wider
    // carved channel. Anchoring the bank rise to the carved (wider) half width
    // used to leave a flat shelf carved ~11.5m below the water out past the
    // ribbon edge, which made the opaque ribbon look like a slab floating a
    // notch above the surrounding ground.
    const float VisibleHalfWidthCm = FYarlungRiverField::VisibleRibbonHalfWidthCm(River.Row.HalfWidthCm);
    const float TargetHeightCm = YarlungTerrainSurface::RiverBedTargetHeightCm(
        River.DistanceCm,
        River.WaterSurfaceZCm,
        VisibleHalfWidthCm);
    const float CarvedHeightCm = FMath::Min(HeightCm, FMath::Lerp(HeightCm, TargetHeightCm, CarveAlpha));
    const float CarveCm = HeightCm - CarvedHeightCm;
    if (OutStats && CarveCm > 1.0f)
    {
        ++OutStats->RiverCarvedVertexCount;
        OutStats->MaxRiverCarveCm = FMath::Max(OutStats->MaxRiverCarveCm, CarveCm);
    }
    return CarvedHeightCm;
}
}

namespace YarlungTerrainSurface
{
float RiverBedTargetHeightCm(
    float DistanceCm,
    float WaterSurfaceZCm,
    float VisibleHalfWidthCm)
{
    constexpr float ChannelBedDepthCm = 1150.0f;  // deepest point under the channel center
    constexpr float ShorelineSubmergeCm = 60.0f;  // bed barely under water at the visible ribbon edge

    const float ChannelT = YarlungTerrain::Smooth01(DistanceCm / FMath::Max(VisibleHalfWidthCm, 1.0f));
    const float SubmergeCm = FMath::Lerp(ChannelBedDepthCm, ShorelineSubmergeCm, ChannelT);

    const float ShoreOffsetCm = FMath::Max(0.0f, DistanceCm - VisibleHalfWidthCm);
    const float WetShelfRiseCm = YarlungTerrain::Smooth01(ShoreOffsetCm / 16000.0f) * 2400.0f;
    const float InnerBankRiseCm = YarlungTerrain::Smooth01(ShoreOffsetCm / 42000.0f) * 9500.0f;
    const float OuterBankRiseCm = YarlungTerrain::Smooth01(ShoreOffsetCm / 98000.0f) * 22000.0f;
    const float BankRiseCm = WetShelfRiseCm + InnerBankRiseCm + OuterBankRiseCm;

    return WaterSurfaceZCm - SubmergeCm + BankRiseCm;
}

float SourceHeightCm(const TArray<uint16>& EncodedHeights, float X, float Y)
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const float U = FMath::Clamp((X - Tc.MinXCm) / (Tc.MaxXCm - Tc.MinXCm), 0.0f, 1.0f);
    const float V = FMath::Clamp((Y - Tc.MinYCm) / (Tc.MaxYCm - Tc.MinYCm), 0.0f, 1.0f);
    return HeightAtSourceGrid(
        EncodedHeights,
        U * static_cast<float>(Tc.GridSize - 1),
        V * static_cast<float>(Tc.GridSize - 1));
}

FVector SourceNormal(const TArray<uint16>& EncodedHeights, float X, float Y)
{
    constexpr float SampleSpacingCm = 900.0f;
    const float Left = SourceHeightCm(EncodedHeights, X - SampleSpacingCm, Y);
    const float Right = SourceHeightCm(EncodedHeights, X + SampleSpacingCm, Y);
    const float Down = SourceHeightCm(EncodedHeights, X, Y - SampleSpacingCm);
    const float Up = SourceHeightCm(EncodedHeights, X, Y + SampleSpacingCm);
    return FVector(Left - Right, Down - Up, SampleSpacingCm * 4.0f).GetSafeNormal();
}

bool FindNearestTrackProfileFrame(
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FVector2D& Position,
    FVector2D& OutCenter,
    float& OutSignedOffsetCm)
{
    if (TrackPoints.Num() < 2)
    {
        return false;
    }

    float BestSquared = TNumericLimits<float>::Max();
    FVector2D BestCenter = TrackPoints[0].Position;
    FVector2D BestTangent = FVector2D(1.0f, 0.0f);

    for (int32 Index = 0; Index < TrackPoints.Num(); ++Index)
    {
        const FVector2D A = TrackPoints[Index].Position;
        const FVector2D B = TrackPoints[(Index + 1) % TrackPoints.Num()].Position;
        const FVector2D AB = B - A;
        const float LengthSquared = AB.SizeSquared();
        if (LengthSquared <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float T = FMath::Clamp(FVector2D::DotProduct(Position - A, AB) / LengthSquared, 0.0f, 1.0f);
        const FVector2D Closest = A + AB * T;
        const float Squared = FVector2D::DistSquared(Position, Closest);
        if (Squared < BestSquared)
        {
            BestSquared = Squared;
            BestCenter = Closest;
            BestTangent = AB.GetSafeNormal();
        }
    }

    const FVector2D Relative = Position - BestCenter;
    OutCenter = BestCenter;
    OutSignedOffsetCm = BestTangent.X * Relative.Y - BestTangent.Y * Relative.X;
    return BestSquared < TNumericLimits<float>::Max();
}

float SurfaceZCm(
    const TArray<uint16>& EncodedHeights,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    const FVector2D& Position,
    FSurfaceStats* OutStats)
{
    const float BaseHeight = SourceHeightCm(EncodedHeights, Position.X, Position.Y);
    const FVector BaseNormal = SourceNormal(EncodedHeights, Position.X, Position.Y);
    const float TrackDistance = YarlungViewCorridor::DistanceToTrackCm(TrackPoints, Position);
    const float ViewCorridorMask = YarlungViewCorridor::ComputeMask(TrackPoints, Position);

    FVector2D ProfileCenter = Position;
    float SignedOffsetCm = 0.0f;
    const bool bHasProfileFrame = FindNearestTrackProfileFrame(TrackPoints, Position, ProfileCenter, SignedOffsetCm);
    const float TrackBaseHeight = bHasProfileFrame
        ? SourceHeightCm(EncodedHeights, ProfileCenter.X, ProfileCenter.Y)
        : BaseHeight;
    const float CorridorProfileHeight = bHasProfileFrame
        ? YarlungCorridorProfile::CorridorTerrainHeightCm(ProfileCenter, SignedOffsetCm, TrackBaseHeight, BaseHeight)
        : BaseHeight;
    const float ProfileBlend = FMath::Clamp(ViewCorridorMask, 0.0f, 1.0f)
        * YarlungTerrain::Smooth01((TrackDistance - 30000.0f) / 52000.0f);
    const float RelaxedProfileHeight = BaseHeight + (CorridorProfileHeight - BaseHeight) * 0.48f;
    const float RiverDistance = RiverField.DistanceCm(Position);
    const float DisplacementCm = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
        Position,
        RelaxedProfileHeight,
        BaseNormal,
        TrackDistance,
        RiverDistance,
        ViewCorridorMask);
    if (OutStats && FMath::Abs(DisplacementCm) > 1.0f)
    {
        ++OutStats->DisplacedVertexCount;
        OutStats->MaxAbsDisplacementCm = FMath::Max(OutStats->MaxAbsDisplacementCm, FMath::Abs(DisplacementCm));
    }

    const float HeightBeforeRiverCarve = FMath::Lerp(BaseHeight, RelaxedProfileHeight, ProfileBlend) + DisplacementCm;
    return CarveRiverChannelCm(HeightBeforeRiverCarve, Position, RiverField, OutStats) + 25.0f;
}

FVector SurfaceNormal(
    const TArray<uint16>& EncodedHeights,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    const FVector2D& Position)
{
    constexpr float SampleSpacingCm = 900.0f;
    const float Left = SurfaceZCm(EncodedHeights, TrackPoints, RiverField, FVector2D(Position.X - SampleSpacingCm, Position.Y));
    const float Right = SurfaceZCm(EncodedHeights, TrackPoints, RiverField, FVector2D(Position.X + SampleSpacingCm, Position.Y));
    const float Down = SurfaceZCm(EncodedHeights, TrackPoints, RiverField, FVector2D(Position.X, Position.Y - SampleSpacingCm));
    const float Up = SurfaceZCm(EncodedHeights, TrackPoints, RiverField, FVector2D(Position.X, Position.Y + SampleSpacingCm));
    return FVector(Left - Right, Down - Up, SampleSpacingCm * 4.0f).GetSafeNormal();
}
}
