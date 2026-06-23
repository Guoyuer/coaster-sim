#if WITH_DEV_AUTOMATION_TESTS

#include "../YarlungAssetConfig.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungRockWallSegmentsCoverTheCorridorTest,
    "CoasterSim.Yarlung.AssetConfig.RockWallSegmentsCoverTheCorridor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungRockWallSegmentsCoverTheCorridorTest::RunTest(const FString& Parameters)
{
    const FYarlungAssetConfig& Config = YarlungAssets::Config();
    TestTrue(TEXT("Reusable rock-wall profiles are declared"), Config.RockWallProfiles.Num() >= 3);
    TestTrue(TEXT("Full-corridor rock-wall segments are declared"), Config.RockWallSegments.Num() >= 6);

    int32 RockWallSourceCount = 0;
    for (const FYarlungSceneryComponentConfig& Component : Config.SceneryComponents)
    {
        if (Component.Name == TEXT("SlopeRockWallA") || Component.Name == TEXT("SlopeRockWallB"))
        {
            ++RockWallSourceCount;
            TestEqual(
                FString::Printf(TEXT("%s is reserved for authored rock-wall placement"), *Component.Name),
                Component.Placement,
                EYarlungSceneryPlacement::RockWallSource);
            TestEqual(
                FString::Printf(TEXT("%s uses the slope rock-wall kind"), *Component.Name),
                Component.Kind,
                FString(TEXT("slope_rock_wall")));
        }
    }
    TestEqual(TEXT("Both slope rock-wall mesh sources are configured"), RockWallSourceCount, 2);

    int32 LowestStartSample = TNumericLimits<int32>::Max();
    int32 HighestEndSample = 0;
    bool bHasLeftSide = false;
    bool bHasRightSide = false;

    for (const FYarlungRockWallSegmentConfig& Segment : Config.RockWallSegments)
    {
        TestFalse(TEXT("Rock-wall segment has a name"), Segment.Name.IsEmpty());
        TestTrue(
            FString::Printf(TEXT("%s references a known reusable profile"), *Segment.Name),
            Config.RockWallProfiles.Contains(Segment.ProfileName));
        TestTrue(
            FString::Printf(TEXT("%s targets a slope rock-wall component"), *Segment.Name),
            Segment.ComponentName == TEXT("SlopeRockWallA") || Segment.ComponentName == TEXT("SlopeRockWallB"));
        TestTrue(
            FString::Printf(TEXT("%s has an ordered sample range"), *Segment.Name),
            Segment.EndSampleIndex > Segment.StartSampleIndex);
        TestTrue(
            FString::Printf(TEXT("%s uses a valid side"), *Segment.Name),
            Segment.Side == -1.0f || Segment.Side == 1.0f);
        LowestStartSample = FMath::Min(LowestStartSample, Segment.StartSampleIndex);
        HighestEndSample = FMath::Max(HighestEndSample, Segment.EndSampleIndex);
        bHasLeftSide = bHasLeftSide || Segment.Side == -1.0f;
        bHasRightSide = bHasRightSide || Segment.Side == 1.0f;
    }

    TestEqual(TEXT("Rock-wall segment system starts at the launch corridor"), LowestStartSample, 0);
    TestTrue(TEXT("Rock-wall segment system reaches the end of the ride corridor"), HighestEndSample >= 224);
    TestTrue(TEXT("Rock-wall segment system covers the left side"), bHasLeftSide);
    TestTrue(TEXT("Rock-wall segment system covers the right side"), bHasRightSide);

    return true;
}

#endif
