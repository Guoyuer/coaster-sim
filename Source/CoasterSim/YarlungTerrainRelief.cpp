#include "YarlungTerrainRelief.h"

namespace YarlungTerrainRelief
{
namespace
{
float LatticeHash(int32 X, int32 Y)
{
    uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Y) * 668265263u;
    H = (H ^ (H >> 13)) * 1274126177u;
    H = H ^ (H >> 16);
    return static_cast<float>(H & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float CoherentNoise2D(float X, float Y)
{
    const float Fx = FMath::FloorToFloat(X);
    const float Fy = FMath::FloorToFloat(Y);
    const int32 Ix = static_cast<int32>(Fx);
    const int32 Iy = static_cast<int32>(Fy);
    const float Tx = YarlungTerrain::Smooth01(X - Fx);
    const float Ty = YarlungTerrain::Smooth01(Y - Fy);
    const float A = LatticeHash(Ix, Iy);
    const float B = LatticeHash(Ix + 1, Iy);
    const float C = LatticeHash(Ix, Iy + 1);
    const float D = LatticeHash(Ix + 1, Iy + 1);
    return FMath::Lerp(FMath::Lerp(A, B, Tx), FMath::Lerp(C, D, Tx), Ty);
}

float Fbm2D(float X, float Y, int32 Octaves, float Lacunarity, float Gain)
{
    float Sum = 0.0f;
    float Amplitude = 0.5f;
    float Frequency = 1.0f;
    for (int32 Octave = 0; Octave < Octaves; ++Octave)
    {
        Sum += Amplitude * (CoherentNoise2D(X * Frequency, Y * Frequency) * 2.0f - 1.0f);
        Frequency *= Lacunarity;
        Amplitude *= Gain;
    }
    return Sum;
}

float Ridged2D(float X, float Y, int32 Octaves, float Lacunarity, float Gain)
{
    float Sum = 0.0f;
    float Amplitude = 0.5f;
    float Frequency = 1.0f;
    float Previous = 1.0f;
    for (int32 Octave = 0; Octave < Octaves; ++Octave)
    {
        float N = CoherentNoise2D(X * Frequency, Y * Frequency);
        N = 1.0f - FMath::Abs(2.0f * N - 1.0f);
        N *= N;
        Sum += Amplitude * N * Previous;
        Previous = N;
        Frequency *= Lacunarity;
        Amplitude *= Gain;
    }
    return Sum;
}
}

float ComputeReliefForRiverDistanceCm(
    const FVector2D& Position,
    float HeightCm,
    const FVector& BaseNormal,
    float TrackDistanceCm,
    float RiverDistance,
    float ViewCorridorMask,
    const FReliefConfig& Config)
{
    const float Slope = 1.0f - BaseNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector).Z;
    const float SlopeGate = YarlungTerrain::Smooth01((Slope - Config.SlopeGateStart) / Config.SlopeGateRange);
    if (SlopeGate <= 0.001f)
    {
        return 0.0f;
    }

    const float RiverGate = YarlungTerrain::Smooth01((RiverDistance - Config.RiverProtectInnerCm) / Config.RiverProtectFadeCm);
    if (RiverGate <= 0.001f)
    {
        return 0.0f;
    }

    const float Height01 = FMath::Clamp((HeightCm - Config.EncodedMinZ) / (Config.EncodedMaxZ - Config.EncodedMinZ), 0.0f, 1.0f);
    const float HeightGate = 1.0f - Config.HighAltitudeReduction * YarlungTerrain::Smooth01(
        (Height01 - Config.HighAltitudeGateStart) / Config.HighAltitudeGateRange);

    const float WarpX = Position.X
        + Config.WarpAmplitudeCm * Fbm2D(Position.X * Config.WarpScale, Position.Y * Config.WarpScale, 2, 2.0f, 0.5f);
    const float WarpY = Position.Y
        + Config.WarpAmplitudeCm * Fbm2D(
            (Position.X + 1733.0f) * Config.WarpScale,
            (Position.Y - 911.0f) * Config.WarpScale,
            2,
            2.0f,
            0.5f);

    const float Ridged = Ridged2D(
        WarpX * Config.NoiseScale,
        WarpY * Config.NoiseScale,
        Config.Octaves,
        Config.Lacunarity,
        Config.Gain);
    const float Fbm = Fbm2D(
        WarpX * Config.NoiseScale * 1.7f,
        WarpY * Config.NoiseScale * 1.7f,
        Config.Octaves,
        Config.Lacunarity,
        Config.Gain);
    const float Detail = FMath::Clamp(
        (Ridged - Config.RidgedBias) * Config.RidgedWeight + Config.FbmWeight * Fbm,
        Config.DetailMin,
        Config.DetailMax);

    const float AmplitudeCm = FMath::Lerp(Config.MinAmplitudeCm, Config.MaxAmplitudeCm, SlopeGate);
    float Displacement = SlopeGate * RiverGate * HeightGate * AmplitudeCm * Detail;

    const float CliffFoldGate = SlopeGate
        * RiverGate
        * HeightGate
        * FMath::Clamp(ViewCorridorMask, 0.0f, 1.0f)
        * YarlungTerrain::Smooth01(
            (RiverDistance - Config.CliffFoldRiverDistanceStartCm) / Config.CliffFoldRiverDistanceFadeCm);
    if (CliffFoldGate > 0.001f)
    {
        const float FoldWarpX = Position.X
            + Config.CliffFoldWarpAmplitudeCm * Fbm2D(
                Position.X * Config.CliffFoldWarpScale,
                RiverDistance * Config.CliffFoldWarpScale,
                3,
                Config.Lacunarity,
                Config.Gain);
        const float FoldWarpY = RiverDistance
            + Config.CliffFoldWarpAmplitudeCm * Fbm2D(
                (Position.X - 3911.0f) * Config.CliffFoldWarpScale,
                (RiverDistance + 2207.0f) * Config.CliffFoldWarpScale,
                3,
                Config.Lacunarity,
                Config.Gain);
        const float FoldRidged = Ridged2D(
            FoldWarpX * Config.CliffFoldAlongRiverScale,
            FoldWarpY * Config.CliffFoldAcrossWallScale,
            Config.CliffFoldOctaves,
            Config.Lacunarity,
            Config.Gain);
        const float FoldFbm = Fbm2D(
            FoldWarpX * Config.CliffFoldAlongRiverScale * 1.65f,
            FoldWarpY * Config.CliffFoldAcrossWallScale * 1.35f,
            Config.CliffFoldOctaves,
            Config.Lacunarity,
            Config.Gain);
        const float CliffFoldDetail = FMath::Clamp(
            (FoldRidged - Config.CliffFoldBias) * Config.CliffFoldWeight + Config.CliffFoldFbmWeight * FoldFbm,
            Config.CliffFoldDetailMin,
            Config.CliffFoldDetailMax);
        const float CliffFoldAmplitudeCm = FMath::Lerp(
            Config.CliffFoldMinAmplitudeCm,
            Config.CliffFoldMaxAmplitudeCm,
            SlopeGate);
        Displacement += CliffFoldGate * CliffFoldAmplitudeCm * CliffFoldDetail;
    }

    const float NearTrackUp = 1.0f - YarlungTerrain::Smooth01(
        (TrackDistanceCm - Config.NearTrackProtectStartCm) / Config.NearTrackProtectFadeCm);
    if (Displacement > 0.0f)
    {
        Displacement *= (1.0f - (1.0f - Config.NearTrackPositiveScale) * NearTrackUp);
    }

    return Displacement;
}

float ComputeReliefCm(
    const FVector2D& Position,
    float HeightCm,
    const FVector& BaseNormal,
    float TrackDistanceCm,
    float ViewCorridorMask,
    const FReliefConfig& Config)
{
    return ComputeReliefForRiverDistanceCm(
        Position,
        HeightCm,
        BaseNormal,
        TrackDistanceCm,
        FMath::Abs(Position.Y - YarlungTerrain::RiverCenterY(Position.X)),
        ViewCorridorMask,
        Config);
}

FVector ApplyVerticalDisplacement(const FVector& BasePosition, float DisplacementCm)
{
    return BasePosition + FVector::UpVector * DisplacementCm;
}
}
