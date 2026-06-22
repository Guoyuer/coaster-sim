#include "CoasterTrackVisuals.h"

#include "CoasterTrackComponent.h"
#include "YarlungGeneratedPaths.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
FTransform MakeTubeTransform(const FVector& Start, const FVector& End, float DiameterCm)
{
    const FVector Mid = (Start + End) * 0.5f;
    const FVector Delta = End - Start;
    const float Length = FMath::Max(Delta.Length(), 1.0f);
    const FRotator Rotation = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
    const float Diameter = FMath::Max(DiameterCm, 1.0f);
    return FTransform(Rotation, Mid, FVector(Diameter / 100.0f, Diameter / 100.0f, Length / 100.0f));
}

void ApplyTint(UMeshComponent* Component, const FLinearColor& Color, float Metallic, float Roughness)
{
    if (!Component)
    {
        return;
    }

    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, YarlungGeneratedPaths::CoasterTintMaterialObjectPath);
    if (!TintMaterial)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Required track tint material is missing: %s"), YarlungGeneratedPaths::CoasterTintMaterialObjectPath);
    }
    Component->SetMaterial(0, TintMaterial);

    UMaterialInstanceDynamic* Material = Component->CreateAndSetMaterialInstanceDynamic(0);
    if (!Material)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Unable to create dynamic track material instance."));
    }

    Material->SetVectorParameterValue(TEXT("Color"), Color);
    Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    Material->SetScalarParameterValue(TEXT("Metallic"), Metallic);
    Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    Material->SetScalarParameterValue(TEXT("Specular"), 0.5f);
}
}

namespace CoasterTrackVisuals
{
void ConfigureMeshes(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* CenterSpine,
    UInstancedStaticMeshComponent* LeftGuardRail,
    UInstancedStaticMeshComponent* RightGuardRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* TrackBraces,
    UInstancedStaticMeshComponent* Supports)
{
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!CylinderMesh)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Required tube track mesh is missing: /Engine/BasicShapes/Cylinder.Cylinder"));
    }

    for (UInstancedStaticMeshComponent* Component : { LeftRail, RightRail, CenterSpine, LeftGuardRail, RightGuardRail, Ties, TrackBraces, Supports })
    {
        if (Component)
        {
            Component->SetStaticMesh(CylinderMesh);
        }
    }
}

void ApplyMaterials(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* CenterSpine,
    UInstancedStaticMeshComponent* LeftGuardRail,
    UInstancedStaticMeshComponent* RightGuardRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* TrackBraces,
    UInstancedStaticMeshComponent* Supports)
{
    const FLinearColor TrackRed(0.76f, 0.12f, 0.08f);
    const FLinearColor DarkRed(0.45f, 0.06f, 0.04f);
    const FLinearColor SupportTeal(0.10f, 0.34f, 0.38f);
    ApplyTint(LeftRail, TrackRed, 0.9f, 0.30f);
    ApplyTint(RightRail, TrackRed, 0.9f, 0.30f);
    ApplyTint(CenterSpine, DarkRed, 0.9f, 0.34f);
    ApplyTint(LeftGuardRail, TrackRed, 0.85f, 0.36f);
    ApplyTint(RightGuardRail, TrackRed, 0.85f, 0.36f);
    ApplyTint(Ties, DarkRed, 0.85f, 0.42f);
    ApplyTint(TrackBraces, TrackRed, 0.85f, 0.40f);
    ApplyTint(Supports, SupportTeal, 0.85f, 0.46f);
}

void SetVisible(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* CenterSpine,
    UInstancedStaticMeshComponent* LeftGuardRail,
    UInstancedStaticMeshComponent* RightGuardRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* TrackBraces,
    UInstancedStaticMeshComponent* Supports,
    bool bVisible)
{
    for (UInstancedStaticMeshComponent* Component : { LeftRail, RightRail, CenterSpine, LeftGuardRail, RightGuardRail, Ties, TrackBraces, Supports })
    {
        if (Component)
        {
            Component->SetVisibility(bVisible, true);
        }
    }
}

void Rebuild(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* CenterSpine,
    UInstancedStaticMeshComponent* LeftGuardRail,
    UInstancedStaticMeshComponent* RightGuardRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* TrackBraces,
    UInstancedStaticMeshComponent* Supports,
    const UCoasterTrackComponent* TrackSpline,
    float TrackLengthCm,
    float RailGaugeCm,
    FSampleFrameFn SampleFrame)
{
    if (!LeftRail || !RightRail || !CenterSpine || !LeftGuardRail || !RightGuardRail || !Ties || !TrackBraces || !Supports || !TrackSpline || TrackLengthCm <= 1.0f)
    {
        return;
    }

    LeftRail->ClearInstances();
    RightRail->ClearInstances();
    CenterSpine->ClearInstances();
    LeftGuardRail->ClearInstances();
    RightGuardRail->ClearInstances();
    Ties->ClearInstances();
    TrackBraces->ClearInstances();
    Supports->ClearInstances();

    const float RailHalfGauge = RailGaugeCm * 0.5f;
    constexpr float SegmentStep = 160.0f;
    constexpr float TieStep = 260.0f;
    constexpr float BraceStep = 520.0f;
    constexpr float SupportStep = 9600.0f;

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += SegmentStep)
    {
        FVector LocationA;
        FVector ForwardA;
        FVector RightA;
        FVector UpA;
        FRotator RotationA;
        SampleFrame(Distance, LocationA, RotationA, ForwardA, RightA, UpA);

        FVector LocationB;
        FVector ForwardB;
        FVector RightB;
        FVector UpB;
        FRotator RotationB;
        SampleFrame(FMath::Fmod(Distance + SegmentStep, TrackLengthCm), LocationB, RotationB, ForwardB, RightB, UpB);

        const FVector RailDropA = UpA * 18.0f;
        const FVector RailDropB = UpB * 18.0f;
        LeftRail->AddInstance(MakeTubeTransform(LocationA - RightA * RailHalfGauge - RailDropA, LocationB - RightB * RailHalfGauge - RailDropB, 16.0f));
        RightRail->AddInstance(MakeTubeTransform(LocationA + RightA * RailHalfGauge - RailDropA, LocationB + RightB * RailHalfGauge - RailDropB, 16.0f));

        const FVector SpineDropA = UpA * 52.0f;
        const FVector SpineDropB = UpB * 52.0f;
        CenterSpine->AddInstance(MakeTubeTransform(LocationA - SpineDropA, LocationB - SpineDropB, 22.0f));

        const FVector GuardLiftA = UpA * 16.0f;
        const FVector GuardLiftB = UpB * 16.0f;
        LeftGuardRail->AddInstance(MakeTubeTransform(LocationA - RightA * (RailHalfGauge + 34.0f) + GuardLiftA, LocationB - RightB * (RailHalfGauge + 34.0f) + GuardLiftB, 4.0f));
        RightGuardRail->AddInstance(MakeTubeTransform(LocationA + RightA * (RailHalfGauge + 34.0f) + GuardLiftA, LocationB + RightB * (RailHalfGauge + 34.0f) + GuardLiftB, 4.0f));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += TieStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector TieCenter = Location - Up * 38.0f;
        const FVector TieStart = TieCenter - Right * (RailHalfGauge + 34.0f);
        const FVector TieEnd = TieCenter + Right * (RailHalfGauge + 34.0f);
        Ties->AddInstance(MakeTubeTransform(TieStart, TieEnd, 7.0f));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += BraceStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector Spine = Location - Up * 58.0f;
        const FVector LeftRailPoint = Location - Right * RailHalfGauge - Up * 20.0f;
        const FVector RightRailPoint = Location + Right * RailHalfGauge - Up * 20.0f;
        TrackBraces->AddInstance(MakeTubeTransform(Spine, LeftRailPoint, 5.0f));
        TrackBraces->AddInstance(MakeTubeTransform(Spine, RightRailPoint, 5.0f));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += SupportStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        if (Location.Z <= 250.0f)
        {
            continue;
        }

        const FVector YokeCenter = Location - Up * 62.0f;
        const FVector YokeLeft = YokeCenter - Right * (RailHalfGauge + 34.0f);
        const FVector YokeRight = YokeCenter + Right * (RailHalfGauge + 34.0f);
        const float TerrainRefZ = TrackSpline->GetGeneratedTerrainZAtDistance(Distance);
        const float TerrainFootZ = TerrainRefZ - 6000.0f;
        const FVector LeftFoot = FVector(YokeLeft.X, YokeLeft.Y, TerrainFootZ);
        const FVector RightFoot = FVector(YokeRight.X, YokeRight.Y, TerrainFootZ);

        const float PierHeight = FMath::Max(YokeCenter.Z - TerrainRefZ, 1.0f);
        const float LegThickness = FMath::Clamp(PierHeight * 0.004f, 42.0f, 120.0f);

        Supports->AddInstance(MakeTubeTransform(LeftFoot, YokeLeft, LegThickness));
        Supports->AddInstance(MakeTubeTransform(RightFoot, YokeRight, LegThickness));

        const FVector LeftSurface = FVector(YokeLeft.X, YokeLeft.Y, TerrainRefZ);
        const FVector RightSurface = FVector(YokeRight.X, YokeRight.Y, TerrainRefZ);
        const int32 BraceCount = FMath::Clamp(FMath::FloorToInt(PierHeight / 9000.0f), 0, 2);
        for (int32 BraceIndex = 1; BraceIndex <= BraceCount; ++BraceIndex)
        {
            const float BraceT = static_cast<float>(BraceIndex) / static_cast<float>(BraceCount + 1);
            const FVector BraceLeft = FMath::Lerp(LeftSurface, YokeLeft, BraceT);
            const FVector BraceRight = FMath::Lerp(RightSurface, YokeRight, BraceT);
            Supports->AddInstance(MakeTubeTransform(BraceLeft, BraceRight, LegThickness * 0.35f));
        }
    }
}
}
