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
        TEXT("Near-track authored profile fully blends to the ride envelope"),
        YarlungCorridorProfile::NearTrackBlend(0.0f),
        1.0f);
    TestEqual(
        TEXT("Far corridor authored profile does not use the ride envelope"),
        YarlungCorridorProfile::NearTrackBlend(42000.0f),
        0.0f);
    TestEqual(
        TEXT("Ride envelope stays below the track"),
        YarlungCorridorProfile::RideEnvelopeHeightCm(TrackBaseHeight),
        TrackBaseHeight - 9500.0f);

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
    const float WallHeight = YarlungCorridorProfile::AuthoredHeightCm(Center, 90000.0f, TrackBaseHeight, BaseHeight);

    TestTrue(TEXT("Authored corridor height is finite"), FMath::IsFinite(NearHeight) && FMath::IsFinite(WallHeight));
    TestTrue(TEXT("Wall profile rises above the talus apron"), WallHeight > NearHeight + 8000.0f);
    TestTrue(TEXT("Authored wall remains in a plausible Yarlung canyon range"), WallHeight > 350000.0f && WallHeight < 430000.0f);

    return true;
}

#endif
