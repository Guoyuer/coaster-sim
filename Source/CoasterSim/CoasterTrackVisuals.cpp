#include "CoasterTrackVisuals.h"

#include "CoasterTrackComponent.h"

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

    UMaterialInterface* TintMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));
    if (TintMaterial)
    {
        Component->SetMaterial(0, TintMaterial);
    }

    UMaterialInstanceDynamic* Material = Component->CreateAndSetMaterialInstanceDynamic(0);
    if (!Material)
    {
        return;
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
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports)
{
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!CylinderMesh)
    {
        return;
    }

    for (UInstancedStaticMeshComponent* Component : { LeftRail, RightRail, Ties, Supports })
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
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports)
{
    ApplyTint(LeftRail, FLinearColor(0.52f, 0.54f, 0.57f), 1.0f, 0.24f);
    ApplyTint(RightRail, FLinearColor(0.52f, 0.54f, 0.57f), 1.0f, 0.24f);
    ApplyTint(Ties, FLinearColor(0.20f, 0.21f, 0.23f), 0.85f, 0.55f);
    ApplyTint(Supports, FLinearColor(0.26f, 0.30f, 0.34f), 0.9f, 0.45f);
}

void SetVisible(
    UInstancedStaticMeshComponent* LeftRail,
    UInstancedStaticMeshComponent* RightRail,
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports,
    bool bVisible)
{
    for (UInstancedStaticMeshComponent* Component : { LeftRail, RightRail, Ties, Supports })
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
    UInstancedStaticMeshComponent* Ties,
    UInstancedStaticMeshComponent* Supports,
    const UCoasterTrackComponent* TrackSpline,
    float TrackLengthCm,
    float RailGaugeCm,
    FSampleFrameFn SampleFrame)
{
    if (!LeftRail || !RightRail || !Ties || !Supports || !TrackSpline || TrackLengthCm <= 1.0f)
    {
        return;
    }

    LeftRail->ClearInstances();
    RightRail->ClearInstances();
    Ties->ClearInstances();
    Supports->ClearInstances();

    const float RailHalfGauge = RailGaugeCm * 0.5f;
    constexpr float SegmentStep = 180.0f;
    constexpr float TieStep = 360.0f;
    constexpr float SupportStep = 14400.0f;

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
        LeftRail->AddInstance(MakeTubeTransform(LocationA - RightA * RailHalfGauge - RailDropA, LocationB - RightB * RailHalfGauge - RailDropB, 12.0f));
        RightRail->AddInstance(MakeTubeTransform(LocationA + RightA * RailHalfGauge - RailDropA, LocationB + RightB * RailHalfGauge - RailDropB, 12.0f));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += TieStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector TieCenter = Location - Up * 42.0f;
        const FVector TieStart = TieCenter - Right * (RailHalfGauge - 8.0f);
        const FVector TieEnd = TieCenter + Right * (RailHalfGauge - 8.0f);
        Ties->AddInstance(MakeTubeTransform(TieStart, TieEnd, 6.0f));
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
