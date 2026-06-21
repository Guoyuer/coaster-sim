#include "../YarlungCorridorProfile.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungCorridorProfileEnvelopeTest,
    "CoasterSim.Yarlung.CorridorProfile.Envelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungCorridorProfileEnvelopeTest::RunTest(const FString& Parameters)
{
    const float TrackBaseHeight = 360000.0f;

    TestEqual(
        TEXT("Near-track terrain is no longer force-carved into a ride-envelope trough"),
        YarlungCorridorProfile::NearTrackBlend(0.0f),
        0.0f);
    TestEqual(
        TEXT("Far corridor authored profile does not use the ride envelope"),
        YarlungCorridorProfile::NearTrackBlend(42000.0f),
        0.0f);
    TestEqual(
        TEXT("Ride envelope helper preserves the DEM track-base height"),
        YarlungCorridorProfile::RideEnvelopeHeightCm(TrackBaseHeight),
        TrackBaseHeight);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungCorridorProfileAuthoredHeightTest,
    "CoasterSim.Yarlung.CorridorProfile.AuthoredHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungCorridorProfileAuthoredHeightTest::RunTest(const FString& Parameters)
{
    const FVector2D Center(12000.0f, -34000.0f);
    const float TrackBaseHeight = 360000.0f;
    const float BaseHeight = 346000.0f;

    const float NearHeight = YarlungCorridorProfile::AuthoredHeightCm(Center, 24000.0f, TrackBaseHeight, BaseHeight);
    const float WallHeight = YarlungCorridorProfile::AuthoredHeightCm(Center, 150000.0f, TrackBaseHeight, BaseHeight);
    const float RaisedBaseWallHeight = YarlungCorridorProfile::AuthoredHeightCm(Center, 150000.0f, TrackBaseHeight, BaseHeight + 50000.0f);

    TestTrue(TEXT("Authored corridor height is finite"), FMath::IsFinite(NearHeight) && FMath::IsFinite(WallHeight));
    TestTrue(TEXT("Far corridor profile creates moderate terrain relief without restoring procedural canyon walls"), WallHeight - BaseHeight >= 12000.0f);
    TestTrue(TEXT("Far corridor profile avoids implausible track-height hard walls"), WallHeight - BaseHeight <= 108000.0f);
    TestTrue(TEXT("Near track remains close to the sampled DEM surface"), FMath::Abs(NearHeight - BaseHeight) <= 7000.0f);
    TestTrue(
        TEXT("Authored profile preserves broad DEM elevation deltas"),
        FMath::Abs((RaisedBaseWallHeight - WallHeight) - 50000.0f) <= 1.0f);

    return true;
}

#endif
