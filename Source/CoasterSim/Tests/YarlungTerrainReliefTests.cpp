#include "../YarlungTerrainRelief.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const FVector SteepNormal = FVector(0.55f, 0.0f, 0.835f).GetSafeNormal();
constexpr float FarRiverDistanceCm = 180000.0f;

bool FindPositiveFarRelief(FVector2D& OutPosition, float& OutRelief)
{
    for (int32 YIndex = 0; YIndex < 32; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < 32; ++XIndex)
        {
            const FVector2D Position(
                -260000.0f + static_cast<float>(XIndex) * 16000.0f,
                90000.0f + static_cast<float>(YIndex) * 14000.0f);
            const float Relief = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
                Position,
                380000.0f,
                SteepNormal,
                60000.0f,
                FarRiverDistanceCm);
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

bool FindViewCorridorSensitiveRelief(FVector2D& OutPosition, float& OutMaskedRelief, float& OutOpenRelief)
{
    for (int32 YIndex = 0; YIndex < 32; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < 32; ++XIndex)
        {
            const FVector2D Position(
                -240000.0f + static_cast<float>(XIndex) * 18000.0f,
                120000.0f + static_cast<float>(YIndex) * 16000.0f);
            const float Masked = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
                Position,
                410000.0f,
                SteepNormal,
                60000.0f,
                FarRiverDistanceCm,
                0.0f);
            const float Open = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
                Position,
                410000.0f,
                SteepNormal,
                60000.0f,
                FarRiverDistanceCm,
                1.0f);
            if (FMath::Abs(Open - Masked) > 50.0f)
            {
                OutPosition = Position;
                OutMaskedRelief = Masked;
                OutOpenRelief = Open;
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
        YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
            Upland,
            360000.0f,
            FVector::UpVector,
            60000.0f,
            FarRiverDistanceCm),
        0.0f);

    const FVector2D RiverCenter(50000.0f, -100000.0f);
    TestEqual(
        TEXT("River field distance protects the thalweg from relief"),
        YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
            RiverCenter,
            270000.0f,
            SteepNormal,
            60000.0f,
            0.0f),
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

    const float NearRelief = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
        Position,
        380000.0f,
        SteepNormal,
        0.0f,
        FarRiverDistanceCm);
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
    const float BoundCm = Config.MaxAmplitudeCm * FMath::Max(FMath::Abs(Config.DetailMin), FMath::Abs(Config.DetailMax))
        + Config.CliffFoldMaxAmplitudeCm * FMath::Max(FMath::Abs(Config.CliffFoldDetailMin), FMath::Abs(Config.CliffFoldDetailMax))
        + 1.0f;

    for (int32 Index = 0; Index < 48; ++Index)
    {
        const FVector2D Position(
            -280000.0f + static_cast<float>((Index * 7919) % 560000),
            -360000.0f + static_cast<float>((Index * 3571) % 720000));
        const float First = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
            Position,
            390000.0f,
            SteepNormal,
            50000.0f,
            FarRiverDistanceCm);
        const float Second = YarlungTerrainRelief::ComputeReliefForRiverDistanceCm(
            Position,
            390000.0f,
            SteepNormal,
            50000.0f,
            FarRiverDistanceCm);

        TestTrue(TEXT("Relief is finite"), FMath::IsFinite(First));
        TestEqual(TEXT("Relief is deterministic"), First, Second);
        TestTrue(TEXT("Relief remains within configured amplitude bounds"), FMath::Abs(First) <= BoundCm);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainReliefViewCorridorGateTest,
    "CoasterSim.Yarlung.TerrainRelief.ViewCorridorGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainReliefViewCorridorGateTest::RunTest(const FString& Parameters)
{
    FVector2D Position = FVector2D::ZeroVector;
    float MaskedRelief = 0.0f;
    float OpenRelief = 0.0f;
    TestTrue(
        TEXT("Test fixture finds relief affected by the first-person view corridor mask"),
        FindViewCorridorSensitiveRelief(Position, MaskedRelief, OpenRelief));
    TestTrue(TEXT("View corridor mask changes cliff-fold relief"), FMath::Abs(OpenRelief - MaskedRelief) > 50.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FYarlungTerrainReliefVerticalDisplacementTest,
    "CoasterSim.Yarlung.TerrainRelief.VerticalDisplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FYarlungTerrainReliefVerticalDisplacementTest::RunTest(const FString& Parameters)
{
    const FVector BasePosition(100.0f, 200.0f, 300.0f);
    const FVector Result = YarlungTerrainRelief::ApplyVerticalDisplacement(BasePosition, 100.0f);

    TestEqual(TEXT("Terrain relief keeps X stable to avoid folding corridor walls"), Result.X, BasePosition.X);
    TestEqual(TEXT("Terrain relief keeps Y stable to avoid folding corridor walls"), Result.Y, BasePosition.Y);
    TestEqual(TEXT("Terrain relief applies height detail in world Z"), Result.Z, BasePosition.Z + 100.0f);
    return true;
}

#endif
