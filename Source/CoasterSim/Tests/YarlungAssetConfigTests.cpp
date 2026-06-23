#if WITH_DEV_AUTOMATION_TESTS

#include "../YarlungAssetConfig.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungHeroRockWallGroupsAreAuthoredSetPiecesTest,
    "CoasterSim.Yarlung.AssetConfig.HeroRockWallGroupsAreAuthoredSetPieces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungHeroRockWallGroupsAreAuthoredSetPiecesTest::RunTest(const FString& Parameters)
{
    const FYarlungAssetConfig& Config = YarlungAssets::Config();
    TestTrue(TEXT("Hero rock-wall groups are declared"), Config.HeroRockWallGroups.Num() >= 6);
    TestTrue(TEXT("Foreground rock apron groups are declared"), Config.ForegroundRockApronGroups.Num() >= 3);

    int32 HeroOnlySourceCount = 0;
    for (const FYarlungSceneryComponentConfig& Component : Config.SceneryComponents)
    {
        if (Component.Name == TEXT("SlopeRockWallA") || Component.Name == TEXT("SlopeRockWallB"))
        {
            ++HeroOnlySourceCount;
            TestEqual(
                FString::Printf(TEXT("%s is reserved for authored hero placement"), *Component.Name),
                Component.Placement,
                EYarlungSceneryPlacement::HeroRockWallOnly);
            TestEqual(
                FString::Printf(TEXT("%s uses the slope rock-wall kind"), *Component.Name),
                Component.Kind,
                FString(TEXT("slope_rock_wall")));
        }
    }
    TestEqual(TEXT("Both slope rock-wall mesh sources are configured"), HeroOnlySourceCount, 2);

    TArray<FYarlungRockWallGroupConfig> AuthoredGroups;
    AuthoredGroups.Append(Config.HeroRockWallGroups);
    AuthoredGroups.Append(Config.ForegroundRockApronGroups);

    for (const FYarlungRockWallGroupConfig& Group : AuthoredGroups)
    {
        TestFalse(TEXT("Hero group has a name"), Group.Name.IsEmpty());
        TestTrue(
            FString::Printf(TEXT("%s targets a slope rock-wall component"), *Group.Name),
            Group.ComponentName == TEXT("SlopeRockWallA") || Group.ComponentName == TEXT("SlopeRockWallB"));
        TestTrue(
            FString::Printf(TEXT("%s has an ordered sample range"), *Group.Name),
            Group.EndSampleIndex > Group.StartSampleIndex);
        TestTrue(
            FString::Printf(TEXT("%s uses a valid side"), *Group.Name),
            Group.Side == -1.0f || Group.Side == 1.0f);
        TestTrue(
            FString::Printf(TEXT("%s has overlapping lateral lanes"), *Group.Name),
            Group.LateralStepCm > 0.0f && Group.LateralMaxCm > Group.LateralMinCm);
    }

    return true;
}

#endif
