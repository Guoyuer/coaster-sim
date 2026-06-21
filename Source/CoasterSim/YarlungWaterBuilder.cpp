#include "YarlungWaterBuilder.h"

#include "YarlungAssetConfig.h"
#include "YarlungRiverField.h"
#include "YarlungTerrainProfile.h"
#include "Materials/MaterialInterface.h"
#include "WaterBodyComponent.h"
#include "WaterBodyRiverActor.h"
#include "WaterBodyRiverComponent.h"
#include "WaterSplineComponent.h"
#include "WaterSubsystem.h"
#include "WaterZoneActor.h"

namespace YarlungWaterBuilder
{
bool SpawnYarlungWater(UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung water without a world"));
        return false;
    }

    FYarlungRiverField RiverField;
    FString RiverLoadError;
    if (!RiverField.LoadFromProjectContent(&RiverLoadError))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read generated Yarlung river field for UE Water: %s"), *RiverLoadError);
        return false;
    }
    const TArray<FYarlungRiverRow>& RiverRows = RiverField.GetRows();
    const FYarlungWaterConfig& WaterConfig = YarlungAssets::Config().Water;

    AWaterZone* WaterZone = World->SpawnActor<AWaterZone>(AWaterZone::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!WaterZone)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung UE Water zone actor"));
        return false;
    }
    WaterZone->SetActorLabel(TEXT("YarlungWaterZone"));
    const YarlungTerrain::FConfig& TerrainConfig = YarlungTerrain::Config();
    const FVector2D ZoneExtent(
        (TerrainConfig.MaxXCm - TerrainConfig.MinXCm) * WaterConfig.ZoneExtentScale,
        (TerrainConfig.MaxYCm - TerrainConfig.MinYCm) * WaterConfig.ZoneExtentScale);
    WaterZone->SetActorLocation(FVector(
        (TerrainConfig.MinXCm + TerrainConfig.MaxXCm) * 0.5f,
        (TerrainConfig.MinYCm + TerrainConfig.MaxYCm) * 0.5f,
        TerrainConfig.RiverZCm));
    WaterZone->SetZoneExtent(ZoneExtent);
    WaterZone->SetRenderTargetResolution(FIntPoint(WaterConfig.ZoneRenderTargetResolution, WaterConfig.ZoneRenderTargetResolution));

    AWaterBodyRiver* River = World->SpawnActor<AWaterBodyRiver>(AWaterBodyRiver::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    if (!River)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn Yarlung UE Water river actor"));
        return false;
    }
    River->SetActorLabel(TEXT("YarlungUERiver"));

    UWaterSplineComponent* Spline = River->GetWaterSpline();
    UWaterBodyRiverComponent* RiverComponent = Cast<UWaterBodyRiverComponent>(River->GetWaterBodyComponent());
    if (!Spline || !RiverComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung UE Water river missing spline/component: spline=%s component=%s"),
            Spline ? TEXT("ok") : TEXT("missing"),
            RiverComponent ? TEXT("ok") : TEXT("missing"));
        return false;
    }

    Spline->ClearSplinePoints(false);
    for (const FYarlungRiverRow& Row : RiverRows)
    {
        Spline->AddSplinePoint(Row.PositionCm + FVector(0.0f, 0.0f, FYarlungRiverField::DefaultWaterSurfaceLiftCm), ESplineCoordinateSpace::World, false);
    }
    Spline->SetClosedLoop(false, false);
    Spline->UpdateSpline();
    Spline->K2_SynchronizeAndBroadcastDataChange();

    float MinWaterWidthCm = TNumericLimits<float>::Max();
    float MaxWaterWidthCm = -TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < RiverRows.Num(); ++Index)
    {
        const FYarlungRiverRow& Row = RiverRows[Index];
        const float InputKey = static_cast<float>(Index);
        const float RiverWidthCm = FMath::Clamp(
            Row.HalfWidthCm * WaterConfig.WidthScale,
            WaterConfig.MinWidthCm,
            WaterConfig.MaxWidthCm);
        RiverComponent->SetRiverWidthAtSplineInputKey(InputKey, RiverWidthCm);
        RiverComponent->SetRiverDepthAtSplineInputKey(InputKey, WaterConfig.DefaultDepthCm);
        RiverComponent->SetWaterVelocityAtSplineInputKey(
            InputKey,
            WaterConfig.BaseVelocityCmPerSec + FMath::Frac(Row.Flow * 11.0f) * WaterConfig.FlowVelocityJitterCmPerSec);
        RiverComponent->SetAudioIntensityAtSplineInputKey(InputKey, WaterConfig.AudioIntensity);
        MinWaterWidthCm = FMath::Min(MinWaterWidthCm, RiverWidthCm);
        MaxWaterWidthCm = FMath::Max(MaxWaterWidthCm, RiverWidthCm);
    }

    RiverComponent->SetWaterZoneOverride(TSoftObjectPtr<AWaterZone>(WaterZone));
    RiverComponent->bAffectsLandscape = false;
    RiverComponent->ShapeDilation = WaterConfig.ShapeDilation;
#if WITH_EDITOR
    RiverComponent->SetWaterBodyStaticMeshEnabled(true);
#endif

    UMaterialInterface* RiverMaterial = LoadObject<UMaterialInterface>(nullptr, *WaterConfig.RiverMaterialPath);
    if (!RiverMaterial && !WaterConfig.FallbackRiverMaterialPath.IsEmpty())
    {
        RiverMaterial = LoadObject<UMaterialInterface>(nullptr, *WaterConfig.FallbackRiverMaterialPath);
    }
    UMaterialInterface* RiverSurfaceMaterial = LoadObject<UMaterialInterface>(nullptr, *WaterConfig.SurfaceMaterialPath);
    if (!RiverSurfaceMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung UE Water surface material not found: %s"), *WaterConfig.SurfaceMaterialPath);
        return false;
    }

    if (!RiverMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung UE Water river material not found: %s fallback=%s"),
            *WaterConfig.RiverMaterialPath,
            *WaterConfig.FallbackRiverMaterialPath);
        return false;
    }

    RiverComponent->SetWaterMaterial(RiverMaterial);
    RiverComponent->SetWaterStaticMeshMaterial(RiverSurfaceMaterial);

    FOnWaterBodyChangedParams ChangedParams;
    ChangedParams.bShapeOrPositionChanged = true;
    RiverComponent->UpdateAll(ChangedParams);
#if WITH_EDITOR
    RiverComponent->UpdateWaterBodyRenderData();
#endif
    RiverComponent->UpdateWaterZones(true);
    RiverComponent->UpdateVisibility();

    TArray<UPrimitiveComponent*> WaterRenderables = RiverComponent->GetStandardRenderableComponents();
    int32 ForcedVisibleRenderables = 0;
    for (UPrimitiveComponent* Renderable : WaterRenderables)
    {
        if (!Renderable)
        {
            continue;
        }

        Renderable->SetVisibility(true, true);
        Renderable->SetHiddenInGame(false);
        Renderable->MarkRenderStateDirty();
        ++ForcedVisibleRenderables;
    }

    WaterZone->Update();

    UE_LOG(LogTemp, Display, TEXT("Yarlung UE Water render path: mesh_override=%s material=%s static_material=%s generates_water_mesh_tile=%s renderables=%d forced_visible=%d"),
        RiverComponent->GetWaterMeshOverride() ? *RiverComponent->GetWaterMeshOverride()->GetName() : TEXT("none"),
        RiverComponent->GetWaterMaterial() ? *RiverComponent->GetWaterMaterial()->GetName() : TEXT("none"),
        RiverComponent->GetWaterStaticMeshMaterial() ? *RiverComponent->GetWaterStaticMeshMaterial()->GetName() : TEXT("none"),
        RiverComponent->ShouldGenerateWaterMeshTile() ? TEXT("true") : TEXT("false"),
        WaterRenderables.Num(),
        ForcedVisibleRenderables);

    UE_LOG(LogTemp, Display, TEXT("Spawned Yarlung UE Water river: samples=%d water_width_cm=%.0f..%.0f lift_cm=%.0f"),
        RiverRows.Num(),
        MinWaterWidthCm,
        MaxWaterWidthCm,
        FYarlungRiverField::DefaultWaterSurfaceLiftCm);
    return true;
}
}
