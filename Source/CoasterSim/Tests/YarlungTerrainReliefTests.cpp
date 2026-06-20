#include "../YarlungTerrainRelief.h"
#include "../YarlungTerrainProfile.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const FVector SteepNormal = FVector(0.55f, 0.0f, 0.835f).GetSafeNormal();

bool FindPositiveFarRelief(FVector2D& OutPosition, float& OutRelief)
{
    for (int32 YIndex = 0; YIndex < 32; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < 32; ++XIndex)
        {
            const FVector2D Position(
                -260000.0f + static_cast<float>(XIndex) * 16000.0f,
                90000.0f + static_cast<float>(YIndex) * 14000.0f);
            const float Relief = YarlungTerrainRelief::ComputeReliefCm(Position, 380000.0f, SteepNormal, 60000.0f);
            if (Relief > 100.0f)
            {
                OutPosition = Position;
                OutRelief = Relief;
                return true;
            }
        }
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainReliefFlatAndRiverProtectionTest,
    "CoasterSim.Yarlung.TerrainRelief.FlatAndRiverProtection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainReliefFlatAndRiverProtectionTest::RunTest(const FString& Parameters)
{
    const FVector2D Upland(180000.0f, 210000.0f);
    TestEqual(
        TEXT("Flat ground receives no synthesized geometry relief"),
        YarlungTerrainRelief::ComputeReliefCm(Upland, 360000.0f, FVector::UpVector, 60000.0f),
        0.0f);

    const float RiverX = 50000.0f;
    const FVector2D RiverCenter(RiverX, YarlungTerrain::RiverCenterY(RiverX));
    TestEqual(
        TEXT("River centerline is protected from relief"),
        YarlungTerrainRelief::ComputeReliefCm(RiverCenter, 270000.0f, SteepNormal, 60000.0f),
        0.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainReliefNearTrackProtectionTest,
    "CoasterSim.Yarlung.TerrainRelief.NearTrackPositiveReliefIsSuppressed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainReliefNearTrackProtectionTest::RunTest(const FString& Parameters)
{
    FVector2D Position = FVector2D::ZeroVector;
    float FarRelief = 0.0f;
    TestTrue(TEXT("Test fixture finds a positive far-track relief sample"), FindPositiveFarRelief(Position, FarRelief));
    if (FarRelief <= 0.0f)
    {
        return false;
    }

    const float NearRelief = YarlungTerrainRelief::ComputeReliefCm(Position, 380000.0f, SteepNormal, 0.0f);
    TestTrue(TEXT("Near-track positive relief is clamped down"), NearRelief <= FarRelief * 0.16f);
    TestTrue(TEXT("Near-track protection does not invert positive relief"), NearRelief >= 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainReliefDeterminismAndBoundsTest,
    "CoasterSim.Yarlung.TerrainRelief.DeterminismAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainReliefDeterminismAndBoundsTest::RunTest(const FString& Parameters)
{
    const YarlungTerrainRelief::FReliefConfig Config;
    const float BoundCm = Config.MaxAmplitudeCm * Config.DetailMax + 1.0f;

    for (int32 Index = 0; Index < 48; ++Index)
    {
        const FVector2D Position(
            -280000.0f + static_cast<float>((Index * 7919) % 560000),
            -360000.0f + static_cast<float>((Index * 3571) % 720000));
        const float First = YarlungTerrainRelief::ComputeReliefCm(Position, 390000.0f, SteepNormal, 50000.0f);
        const float Second = YarlungTerrainRelief::ComputeReliefCm(Position, 390000.0f, SteepNormal, 50000.0f);

        TestTrue(TEXT("Relief is finite"), FMath::IsFinite(First));
        TestEqual(TEXT("Relief is deterministic"), First, Second);
        TestTrue(TEXT("Relief remains within configured amplitude bounds"), FMath::Abs(First) <= BoundCm);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainReliefNormalDisplacementTest,
    "CoasterSim.Yarlung.TerrainRelief.NormalDisplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainReliefNormalDisplacementTest::RunTest(const FString& Parameters)
{
    const FVector BasePosition(100.0f, 200.0f, 300.0f);
    const FVector Normal = FVector(0.6f, 0.0f, 0.8f).GetSafeNormal();
    const FVector Result = YarlungTerrainRelief::ApplyNormalDisplacement(BasePosition, Normal, 100.0f);

    TestTrue(TEXT("Normal displacement moves steep faces laterally"), Result.X > BasePosition.X + 50.0f);
    TestTrue(TEXT("Normal displacement is not world-Z-only"), Result.Z < BasePosition.Z + 95.0f);
    return true;
}

#endif
