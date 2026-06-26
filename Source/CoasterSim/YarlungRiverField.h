#pragma once

#include "CoreMinimal.h"

// Columns in Content/Generated/YarlungLandscape/YarlungRiver.csv:
// 0=distance_cm, 1=x, 2=y, 3=z, 4=half_width_cm, 5=flow.
struct FYarlungRiverRow
{
    double DistanceCm = 0.0;
    FVector PositionCm = FVector::ZeroVector;
    float HalfWidthCm = 0.0f;
    float Flow = 0.0f;
};

struct FYarlungRiverQuery
{
    FYarlungRiverRow Row;
    float DistanceCm = 0.0f;
    float SignedDistanceCm = 0.0f;
    float WaterSurfaceZCm = 0.0f;
    float WaterHalfWidthCm = 0.0f;
    bool bIsValid = false;
};

// Deep river spatial module: all generated Yarlung systems should ask this for
// river distance, water surface height, and authored channel width instead of
// mixing CSV samples with the older analytic centerline helper.
class FYarlungRiverField
{
public:
    static constexpr float DefaultWaterSurfaceLiftCm = -650.0f;
    static constexpr float ChannelHalfWidthFactor = 0.66f;
    static constexpr float ChannelHalfWidthLowerBoundCm = 9500.0f;
    static constexpr float ChannelHalfWidthUpperBoundCm = 16500.0f;
    static constexpr float VisibleRibbonHalfWidthFactor = 0.52f;
    static constexpr float VisibleRibbonHalfWidthLowerBoundCm = 7000.0f;
    static constexpr float VisibleRibbonHalfWidthUpperBoundCm = 13000.0f;

    static float CarvedChannelHalfWidthCm(float RiverHalfWidthCm);
    static float VisibleRibbonHalfWidthCm(float RiverHalfWidthCm);

    bool LoadGeneratedCsv(FString* OutError = nullptr);

    bool IsValid() const { return Rows.Num() > 0; }
    const TArray<FYarlungRiverRow>& GetRows() const { return Rows; }

    FYarlungRiverQuery QueryNearest(const FVector2D& Position) const;
    float DistanceCm(const FVector2D& Position) const;
    float ProtectionMask(const FVector2D& Position, float InnerCm, float FadeCm) const;

private:
    static FString GeneratedRiverCsvPath();

    TArray<FYarlungRiverRow> Rows;
};
