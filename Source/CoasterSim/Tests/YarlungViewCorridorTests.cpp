#if WITH_DEV_AUTOMATION_TESTS

#include "../YarlungViewCorridor.h"

#include "Misc/AutomationTest.h"

namespace
{
TArray<YarlungViewCorridor::FTrackPoint> MakeSquareTrack()
{
    return {
        { FVector2D(-100000.0f, 0.0f) },
        { FVector2D(0.0f, 0.0f) },
        { FVector2D(100000.0f, 0.0f) },
        { FVector2D(200000.0f, 0.0f) },
        { FVector2D(200000.0f, 100000.0f) },
        { FVector2D(0.0f, 100000.0f) },
    };
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungViewCorridorDistanceTest,
    "CoasterSim.Yarlung.ViewCorridor.DistanceToTrack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungViewCorridorDistanceTest::RunTest(const FString& Parameters)
{
    const TArray<YarlungViewCorridor::FTrackPoint> Track = MakeSquareTrack();
    TestTrue(
        TEXT("Distance is effectively zero on the track"),
        FMath::IsNearlyZero(YarlungViewCorridor::DistanceToTrackCm(Track, FVector2D(42000.0f, 0.0f)), 0.01f));
    TestTrue(
        TEXT("Distance clamps to the nearest segment"),
        FMath::IsNearlyEqual(YarlungViewCorridor::DistanceToTrackCm(Track, FVector2D(42000.0f, 9000.0f)), 9000.0f, 0.01f));
    TestTrue(
        TEXT("Too few track points fail closed"),
        YarlungViewCorridor::DistanceToTrackCm({ { FVector2D::ZeroVector } }, FVector2D(0.0f, 0.0f)) > 1.0e20f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungViewCorridorMaskTest,
    "CoasterSim.Yarlung.ViewCorridor.Mask",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungViewCorridorMaskTest::RunTest(const FString& Parameters)
{
    const TArray<YarlungViewCorridor::FTrackPoint> Track = MakeSquareTrack();
    const float OnCorridor = YarlungViewCorridor::ComputeMask(Track, FVector2D(52000.0f, 1000.0f));
    const float FarOutside = YarlungViewCorridor::ComputeMask(Track, FVector2D(420000.0f, 420000.0f));
    TestTrue(TEXT("Interior first-person corridor point receives coverage"), OnCorridor > 0.5f);
    TestEqual(TEXT("Far outside point receives no coverage"), FarOutside, 0.0f);
    TestEqual(
        TEXT("Too few track points fail closed"),
        YarlungViewCorridor::ComputeMask({ { FVector2D::ZeroVector }, { FVector2D(1000.0f, 0.0f) } }, FVector2D(0.0f, 0.0f)),
        0.0f);
    return true;
}

#endif
