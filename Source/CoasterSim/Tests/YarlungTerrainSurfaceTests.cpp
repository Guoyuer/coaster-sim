#include "../YarlungCorridorProfile.h"
#include "../YarlungRiverField.h"
#include "../YarlungTerrainProfile.h"
#include "../YarlungTerrainSurface.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TArray<uint16> FlatEncodedHeights(uint16 EncodedHeight)
{
    const int32 Size = YarlungTerrain::Config().GridSize;
    TArray<uint16> EncodedHeights;
    EncodedHeights.Init(EncodedHeight, Size * Size);
    return EncodedHeights;
}

TArray<YarlungViewCorridor::FTrackPoint> StraightTestTrack()
{
    TArray<YarlungViewCorridor::FTrackPoint> TrackPoints;
    for (const FVector2D& Position : {
        FVector2D(-240000.0f, 0.0f),
        FVector2D(-80000.0f, 0.0f),
        FVector2D(80000.0f, 0.0f),
        FVector2D(240000.0f, 0.0f)})
    {
        YarlungViewCorridor::FTrackPoint Point;
        Point.Position = Position;
        TrackPoints.Add(Point);
    }
    return TrackPoints;
}

bool FindWallBandDivergenceFixture(
    const TArray<uint16>& EncodedHeights,
    const TArray<YarlungViewCorridor::FTrackPoint>& TrackPoints,
    const FYarlungRiverField& RiverField,
    FVector2D& OutPosition,
    float& OutLegacyFullProfileHeight,
    float& OutSharedSurfaceHeight,
    float& OutBaseHeight)
{
    for (float X = -180000.0f; X <= 180000.0f; X += 30000.0f)
    {
        for (float Y = 70000.0f; Y <= 220000.0f; Y += 10000.0f)
        {
            const FVector2D Position(X, Y);
            FVector2D ProfileCenter = Position;
            float SignedOffsetCm = 0.0f;
            if (!YarlungTerrainSurface::FindNearestTrackProfileFrame(
                TrackPoints,
                Position,
                ProfileCenter,
                SignedOffsetCm))
            {
                continue;
            }

            const float BaseHeight = YarlungTerrainSurface::SourceHeightCm(EncodedHeights, Position.X, Position.Y);
            const float TrackBaseHeight = YarlungTerrainSurface::SourceHeightCm(
                EncodedHeights,
                ProfileCenter.X,
                ProfileCenter.Y);
            const float LegacyFullProfileHeight = YarlungCorridorProfile::CorridorTerrainHeightCm(
                ProfileCenter,
                SignedOffsetCm,
                TrackBaseHeight,
                BaseHeight);
            const float SharedSurfaceHeight = YarlungTerrainSurface::SurfaceZCm(
                EncodedHeights,
                TrackPoints,
                RiverField,
                Position);

            if (LegacyFullProfileHeight - BaseHeight > 40000.0f
                && LegacyFullProfileHeight - SharedSurfaceHeight > 10000.0f)
            {
                OutPosition = Position;
                OutLegacyFullProfileHeight = LegacyFullProfileHeight;
                OutSharedSurfaceHeight = SharedSurfaceHeight;
                OutBaseHeight = BaseHeight;
                return true;
            }
        }
    }

    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainSurfaceUsesRenderedHeightModelTest,
    "CoasterSim.Yarlung.TerrainSurface.UsesRenderedHeightModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainSurfaceUsesRenderedHeightModelTest::RunTest(const FString& Parameters)
{
    const TArray<uint16> EncodedHeights = FlatEncodedHeights(32768);
    const TArray<YarlungViewCorridor::FTrackPoint> TrackPoints = StraightTestTrack();
    const FYarlungRiverField EmptyRiverField;

    FVector2D WallBandPosition = FVector2D::ZeroVector;
    float LegacyFullProfileHeight = 0.0f;
    float SharedSurfaceHeight = 0.0f;
    float BaseHeight = 0.0f;
    const bool bFoundFixture = FindWallBandDivergenceFixture(
        EncodedHeights,
        TrackPoints,
        EmptyRiverField,
        WallBandPosition,
        LegacyFullProfileHeight,
        SharedSurfaceHeight,
        BaseHeight);

    TestTrue(TEXT("Fixture projects into a high-divergence track wall profile"), bFoundFixture);
    if (!bFoundFixture)
    {
        return false;
    }

    TestTrue(TEXT("Fixture has a large legacy full-profile wall lift"), LegacyFullProfileHeight - BaseHeight > 40000.0f);
    TestTrue(
        TEXT("Shared surface uses the rendered relaxed/blended terrain model, not the old full-profile scenery height"),
        LegacyFullProfileHeight - SharedSurfaceHeight > 10000.0f);
    TestTrue(
        TEXT("Surface normal is finite and normalized"),
        YarlungTerrainSurface::SurfaceNormal(EncodedHeights, TrackPoints, EmptyRiverField, WallBandPosition).IsNormalized());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungRiverWidthConstantsStayOrderedTest,
    "CoasterSim.Yarlung.RiverField.WidthConstantsStayOrdered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungRiverWidthConstantsStayOrderedTest::RunTest(const FString& Parameters)
{
    for (float RiverHalfWidthCm : { 6000.0f, 16000.0f, 32000.0f })
    {
        const float ChannelHalfWidth = FYarlungRiverField::CarvedChannelHalfWidthCm(RiverHalfWidthCm);
        const float VisibleHalfWidth = FYarlungRiverField::VisibleRibbonHalfWidthCm(RiverHalfWidthCm);
        TestTrue(TEXT("Visible water ribbon stays inside carved river channel"), VisibleHalfWidth < ChannelHalfWidth);
        TestTrue(TEXT("River channel leaves a real bank shelf outside the visible water"), ChannelHalfWidth - VisibleHalfWidth >= 2000.0f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungRiverBedMeetsWaterlineAtRibbonEdgeTest,
    "CoasterSim.Yarlung.TerrainSurface.RiverBedMeetsWaterlineAtRibbonEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungRiverBedMeetsWaterlineAtRibbonEdgeTest::RunTest(const FString& Parameters)
{
    // Offset-invariant: the carved bed profile is purely relative to the water
    // surface, so the shoreline contract must hold at any centerline elevation.
    for (const float WaterSurfaceZCm : { 0.0f, 12345.0f, -5000.0f })
    {
        const float RiverHalfWidthCm = 22000.0f;  // matches the generated Yarlung river
        const float VisibleHalfWidth = FYarlungRiverField::VisibleRibbonHalfWidthCm(RiverHalfWidthCm);

        const float CenterZ = YarlungTerrainSurface::RiverBedTargetHeightCm(0.0f, WaterSurfaceZCm, VisibleHalfWidth);
        const float EdgeZ = YarlungTerrainSurface::RiverBedTargetHeightCm(VisibleHalfWidth, WaterSurfaceZCm, VisibleHalfWidth);
        const float BankZ = YarlungTerrainSurface::RiverBedTargetHeightCm(VisibleHalfWidth + 12000.0f, WaterSurfaceZCm, VisibleHalfWidth);

        // Channel centre keeps real depth under the (opaque) ribbon.
        TestTrue(TEXT("Channel centre bed sits well below the water surface"), CenterZ <= WaterSurfaceZCm - 800.0f);

        // Core regression guard: at the visible ribbon edge the bed must hug the
        // waterline instead of sinking to the old ~11.5m sunken shelf that made
        // the water look like a slab floating above the ground.
        TestTrue(TEXT("Bed reaches the waterline at the visible ribbon edge"), WaterSurfaceZCm - EdgeZ < 250.0f);
        TestTrue(TEXT("Bed stays at or just under the water at the ribbon edge"), EdgeZ <= WaterSurfaceZCm);
        TestTrue(TEXT("Bed is shallower at the ribbon edge than at the channel centre"), EdgeZ > CenterZ);

        // The bank climbs out of the water beyond the ribbon edge.
        TestTrue(TEXT("Bank rises above the water surface past the ribbon edge"), BankZ > WaterSurfaceZCm + 3000.0f);
        TestTrue(TEXT("Bank does not form an immediate hard wall at the ribbon edge"), BankZ < WaterSurfaceZCm + 9000.0f);
    }
    return true;
}

#endif
