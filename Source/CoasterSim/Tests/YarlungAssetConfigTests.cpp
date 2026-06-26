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

    const FYarlungRockWallProfileConfig* NearSlopeProfile = Config.RockWallProfiles.Find(TEXT("near_slope_coverage"));
    const FYarlungRockWallProfileConfig* LeftValleyWindowProfile =
        Config.RockWallProfiles.Find(TEXT("left_valley_window_coverage"));
    TestNotNull(TEXT("Right-side near-slope profile exists"), NearSlopeProfile);
    TestNotNull(TEXT("Left-side valley-window profile exists"), LeftValleyWindowProfile);
    if (NearSlopeProfile && LeftValleyWindowProfile)
    {
        TestTrue(
            TEXT("Left valley-window profile preserves first-person valley visibility"),
            LeftValleyWindowProfile->TrackClearanceCm > NearSlopeProfile->TrackClearanceCm);
        TestTrue(
            TEXT("Left valley-window profile uses smaller wall modules than the close near-slope wall"),
            LeftValleyWindowProfile->ScaleMax < NearSlopeProfile->ScaleMax);
    }

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
    bool bLeftCoverageUsesValleyWindow = false;

    for (const FYarlungRockWallSegmentConfig& Segment : Config.RockWallSegments)
    {
        TestFalse(TEXT("Rock-wall segment has a name"), Segment.Name.IsEmpty());
        TestTrue(
            FString::Printf(TEXT("%s references a known reusable profile"), *Segment.Name),
            Config.RockWallProfiles.Contains(Segment.ProfileName));
        if (const FYarlungRockWallProfileConfig* Profile = Config.RockWallProfiles.Find(Segment.ProfileName))
        {
            TestTrue(
                FString::Printf(TEXT("%s uses surface-aligned rock placement"), *Segment.Name),
                Profile->bAlignToSurface);
            TestTrue(
                FString::Printf(TEXT("%s only places rock-wall modules on cliff-like slopes"), *Segment.Name),
                Profile->MinSlope >= 0.16f);
        }
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
        if (Segment.Name == TEXT("left_near_slope_coverage"))
        {
            bLeftCoverageUsesValleyWindow = Segment.ProfileName == TEXT("left_valley_window_coverage");
        }
    }

    TestEqual(TEXT("Rock-wall segment system starts at the launch corridor"), LowestStartSample, 0);
    TestTrue(TEXT("Rock-wall segment system reaches the end of the ride corridor"), HighestEndSample >= 224);
    TestTrue(TEXT("Rock-wall segment system covers the left side"), bHasLeftSide);
    TestTrue(TEXT("Rock-wall segment system covers the right side"), bHasRightSide);
    TestTrue(TEXT("Left full-corridor coverage is a valley window, not a close wall"), bLeftCoverageUsesValleyWindow);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungSurfaceCoverProfilesDriveGroundAndCanopyTest,
    "CoasterSim.Yarlung.AssetConfig.SurfaceCoverProfilesDriveGroundAndCanopy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungSurfaceCoverProfilesDriveGroundAndCanopyTest::RunTest(const FString& Parameters)
{
    const FYarlungAssetConfig& Config = YarlungAssets::Config();
    TestTrue(TEXT("Reusable surface-cover profiles are declared"), Config.SurfaceCoverProfiles.Num() >= 4);

    const FYarlungSurfaceCoverProfileConfig* WetRockShore = Config.SurfaceCoverProfiles.Find(TEXT("wet_rock_shore"));
    const FYarlungSurfaceCoverProfileConfig* TalusScree = Config.SurfaceCoverProfiles.Find(TEXT("talus_scree"));
    const FYarlungSurfaceCoverProfileConfig* ForestFloor = Config.SurfaceCoverProfiles.Find(TEXT("forest_floor"));
    const FYarlungSurfaceCoverProfileConfig* CanopyMass = Config.SurfaceCoverProfiles.Find(TEXT("canopy_mass"));

    TestNotNull(TEXT("wet_rock_shore profile exists"), WetRockShore);
    TestNotNull(TEXT("talus_scree profile exists"), TalusScree);
    TestNotNull(TEXT("forest_floor profile exists"), ForestFloor);
    TestNotNull(TEXT("canopy_mass profile exists"), CanopyMass);

    if (WetRockShore)
    {
        TestEqual(TEXT("wet_rock_shore anchors to the river"), WetRockShore->Anchor, EYarlungSurfaceCoverAnchor::River);
        TestTrue(TEXT("wet_rock_shore keeps explicit track clearance"), WetRockShore->TrackClearanceCm > 0.0f);
        TestTrue(TEXT("wet_rock_shore keeps explicit river clearance"), WetRockShore->RiverClearanceCm > 0.0f);
    }
    if (TalusScree)
    {
        TestEqual(TEXT("talus_scree anchors to the track corridor"), TalusScree->Anchor, EYarlungSurfaceCoverAnchor::Track);
        TestTrue(TEXT("talus_scree reaches the near first-person slope"), TalusScree->TrackClearanceCm <= 12000.0f);
        TestTrue(TEXT("talus_scree stays in small rubble scale"), TalusScree->ScaleMax <= 1.5f);
    }
    if (ForestFloor)
    {
        TestEqual(TEXT("forest_floor anchors to the track corridor"), ForestFloor->Anchor, EYarlungSurfaceCoverAnchor::Track);
        TestTrue(TEXT("forest_floor reaches the near first-person slope"), ForestFloor->TrackClearanceCm <= 16000.0f);
        TestTrue(TEXT("forest_floor can cover steep corridor banks"), ForestFloor->MaxSlope >= 0.9f);
    }
    if (CanopyMass)
    {
        TestEqual(TEXT("canopy_mass anchors to the track corridor"), CanopyMass->Anchor, EYarlungSurfaceCoverAnchor::Track);
        TestTrue(TEXT("canopy_mass preserves the previously validated near-track clearance"), CanopyMass->TrackClearanceCm >= 30000.0f);
    }

    TSet<FString> ExpectedSurfaceCoverComponents;
    ExpectedSurfaceCoverComponents.Add(TEXT("RiverbankBoulders"));
    ExpectedSurfaceCoverComponents.Add(TEXT("ScreeBoulders"));
    ExpectedSurfaceCoverComponents.Add(TEXT("UnderstoryClumps"));
    ExpectedSurfaceCoverComponents.Add(TEXT("ForestShrubsA"));
    ExpectedSurfaceCoverComponents.Add(TEXT("ForestShrubsB"));
    ExpectedSurfaceCoverComponents.Add(TEXT("CanopyTreesA"));
    ExpectedSurfaceCoverComponents.Add(TEXT("CanopyTreesB"));
    ExpectedSurfaceCoverComponents.Add(TEXT("CanopyTreesC"));

    for (const FYarlungSceneryComponentConfig& Component : Config.SceneryComponents)
    {
        if (!ExpectedSurfaceCoverComponents.Contains(Component.Name))
        {
            continue;
        }

        TestEqual(
            FString::Printf(TEXT("%s is generated by the surface-cover system"), *Component.Name),
            Component.Placement,
            EYarlungSceneryPlacement::SurfaceCover);
        TestTrue(
            FString::Printf(TEXT("%s references a known surface-cover profile"), *Component.Name),
            Config.SurfaceCoverProfiles.Contains(Component.SurfaceCoverProfileName));
        ExpectedSurfaceCoverComponents.Remove(Component.Name);
    }

    TestEqual(TEXT("All ground/canopy surface-cover components are configured"), ExpectedSurfaceCoverComponents.Num(), 0);

    return true;
}

#endif
