#include "../YarlungTerrainProfile.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// Guards the C++ <-> Python split-brain: the golden values below are mirrored in
// scripts/test_yarlung_parity.py. Both sides read Config/yarlung-terrain.json, so
// if anyone edits the JSON one of the two tests goes red instead of the terrain
// silently diverging from the generated source height data.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainConfigParityTest,
    "CoasterSim.Yarlung.TerrainConfigParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainConfigParityTest::RunTest(const FString& Parameters)
{
    const FString Path = FPaths::ProjectDir() / TEXT("Config/yarlung-terrain.json");
    TestTrue(TEXT("Config/yarlung-terrain.json exists (single source of truth)"), FPaths::FileExists(Path));

    const YarlungTerrain::FConfig& C = YarlungTerrain::Config();

    TestEqual(TEXT("GridSize"), C.GridSize, 1009);
    TestEqual(TEXT("EncodedMinZCm"), C.EncodedMinZCm, 260000.0f);
    TestEqual(TEXT("EncodedMaxZCm"), C.EncodedMaxZCm, 730000.0f);
    TestTrue(TEXT("MinXCm"), FMath::IsNearlyEqual(C.MinXCm, -337778.4313411617f, 1.0f));
    TestTrue(TEXT("MaxXCm"), FMath::IsNearlyEqual(C.MaxXCm, 337778.4313411617f, 1.0f));
    TestTrue(TEXT("MinYCm"), FMath::IsNearlyEqual(C.MinYCm, -416981.55087574443f, 1.0f));
    TestTrue(TEXT("MaxYCm"), FMath::IsNearlyEqual(C.MaxYCm, 416981.55087574443f, 1.0f));

    TestTrue(TEXT("HeightValueToCm(32768)"), FMath::IsNearlyEqual(YarlungTerrain::HeightValueToCm(32768), 495003.585870f, 1.0f));

    return true;
}

#endif
