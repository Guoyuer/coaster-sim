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
void ConfigureTrackProxyComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh, bool bCastShadow)
{
    if (!Component)
    {
        return;
    }

    Component->SetStaticMesh(Mesh);
    // These generated tube instances are first-person composition guides, not
    // authored coaster assets. Their thin VSM shadows read as giant black lines
    // across the valley/water in screenshots, so only the visible geometry remains.
    Component->SetCastShadow(bCastShadow);
}

void ConfigureSupportProxyComponent(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh)
{
    ConfigureTrackProxyComponent(Component, Mesh, false);
    if (Component)
    {
        // Long generated support legs become hairline clutter in distant
        // first-person canyon shots. Keep nearby structure for scale and cull
        // far proxy supports until authored coaster assets replace them.
        Component->SetCullDistances(30000, 60000);
    }
}

FTransform MakeTubeTransform(const FVector& Start, const FVector& End, float DiameterCm)
{
    const FVector Mid = (Start + End) * 0.5f;
    const FVector Delta = End - Start;
    const float Length = FMath::Max(Delta.Length(), 1.0f);
    const FRotator Rotation = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
    const float Diameter = FMath::Max(DiameterCm, 1.0f);
    return FTransform(Rotation, Mid, FVector(Diameter / 100.0f, Diameter / 100.0f, Length / 100.0f));
}

FTransform MakeBeamTransform(const FVector& Start, const FVector& End, float WidthCm, float DepthCm)
{
    const FVector Mid = (Start + End) * 0.5f;
    const FVector Delta = End - Start;
    const float Length = FMath::Max(Delta.Length(), 1.0f);
    const FRotator Rotation = FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator();
    return FTransform(
        Rotation,
        Mid,
        FVector(FMath::Max(WidthCm, 1.0f) / 100.0f, FMath::Max(DepthCm, 1.0f) / 100.0f, Length / 100.0f));
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
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!CubeMesh)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Required beam track mesh is missing: /Engine/BasicShapes/Cube.Cube"));
    }

    ConfigureTrackProxyComponent(LeftRail, CylinderMesh, true);
    ConfigureTrackProxyComponent(RightRail, CylinderMesh, true);
    ConfigureTrackProxyComponent(CenterSpine, CylinderMesh, false);
    ConfigureTrackProxyComponent(LeftGuardRail, CylinderMesh, false);
    ConfigureTrackProxyComponent(RightGuardRail, CylinderMesh, false);
    ConfigureTrackProxyComponent(Ties, CubeMesh, false);
    ConfigureTrackProxyComponent(TrackBraces, CubeMesh, false);
    ConfigureSupportProxyComponent(Supports, CylinderMesh);
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
    const FLinearColor OxideRail(0.44f, 0.045f, 0.030f);
    const FLinearColor DarkOxide(0.18f, 0.025f, 0.020f);
    const FLinearColor WeatheredSteel(0.30f, 0.34f, 0.32f);
    const FLinearColor DarkSteel(0.095f, 0.110f, 0.105f);
    ApplyTint(LeftRail, OxideRail, 0.85f, 0.38f);
    ApplyTint(RightRail, OxideRail, 0.85f, 0.38f);
    ApplyTint(CenterSpine, DarkSteel, 0.82f, 0.46f);
    ApplyTint(LeftGuardRail, DarkOxide, 0.70f, 0.48f);
    ApplyTint(RightGuardRail, DarkOxide, 0.70f, 0.48f);
    ApplyTint(Ties, DarkSteel, 0.78f, 0.52f);
    ApplyTint(TrackBraces, WeatheredSteel, 0.70f, 0.50f);
    ApplyTint(Supports, WeatheredSteel, 0.62f, 0.62f);
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
    constexpr float SegmentStep = 130.0f;
    constexpr float TieStep = 190.0f;
    constexpr float BraceStep = 380.0f;
    constexpr float SupportStep = 18000.0f;

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

        const FVector RailDropA = UpA * 20.0f;
        const FVector RailDropB = UpB * 20.0f;
        LeftRail->AddInstance(MakeTubeTransform(LocationA - RightA * RailHalfGauge - RailDropA, LocationB - RightB * RailHalfGauge - RailDropB, 18.0f));
        RightRail->AddInstance(MakeTubeTransform(LocationA + RightA * RailHalfGauge - RailDropA, LocationB + RightB * RailHalfGauge - RailDropB, 18.0f));

        const FVector SpineDropA = UpA * 74.0f;
        const FVector SpineDropB = UpB * 74.0f;
        CenterSpine->AddInstance(MakeTubeTransform(LocationA - SpineDropA, LocationB - SpineDropB, 16.0f));

        const FVector GuardLiftA = UpA * 18.0f;
        const FVector GuardLiftB = UpB * 18.0f;
        LeftGuardRail->AddInstance(MakeTubeTransform(LocationA - RightA * (RailHalfGauge + 42.0f) + GuardLiftA, LocationB - RightB * (RailHalfGauge + 42.0f) + GuardLiftB, 5.0f));
        RightGuardRail->AddInstance(MakeTubeTransform(LocationA + RightA * (RailHalfGauge + 42.0f) + GuardLiftA, LocationB + RightB * (RailHalfGauge + 42.0f) + GuardLiftB, 5.0f));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += TieStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector TieCenter = Location - Up * 48.0f;
        const FVector TieStart = TieCenter - Right * (RailHalfGauge + 50.0f);
        const FVector TieEnd = TieCenter + Right * (RailHalfGauge + 50.0f);
        Ties->AddInstance(MakeBeamTransform(TieStart, TieEnd, 14.0f, 10.0f));
    }

    for (float Distance = 0.0f; Distance < TrackLengthCm; Distance += BraceStep)
    {
        FVector Location;
        FVector Forward;
        FVector Right;
        FVector Up;
        FRotator Rotation;
        SampleFrame(Distance, Location, Rotation, Forward, Right, Up);

        const FVector Spine = Location - Up * 74.0f;
        const FVector LeftRailPoint = Location - Right * RailHalfGauge - Up * 22.0f;
        const FVector RightRailPoint = Location + Right * RailHalfGauge - Up * 22.0f;
        TrackBraces->AddInstance(MakeBeamTransform(Spine, LeftRailPoint, 6.0f, 5.0f));
        TrackBraces->AddInstance(MakeBeamTransform(Spine, RightRailPoint, 6.0f, 5.0f));
        TrackBraces->AddInstance(MakeBeamTransform(LeftRailPoint - Forward * 68.0f, RightRailPoint + Forward * 68.0f, 4.5f, 4.5f));
        TrackBraces->AddInstance(MakeBeamTransform(RightRailPoint - Forward * 68.0f, LeftRailPoint + Forward * 68.0f, 4.5f, 4.5f));
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
        const float TerrainFootZ = TerrainRefZ + 45.0f;
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
