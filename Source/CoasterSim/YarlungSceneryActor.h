#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungGeneratedPaths.h"
#include "YarlungViewCorridor.h"
#include "YarlungSceneryActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
struct FYarlungAssetConfig;
struct FYarlungRockWallProfileConfig;
struct FYarlungRockWallSegmentConfig;
struct FYarlungSceneryComponentConfig;
struct FYarlungScatterKindConfig;
struct FYarlungSurfaceCoverProfileConfig;

USTRUCT()
struct FYarlungSceneryTrackSample
{
    GENERATED_BODY()

    FVector Position = FVector::ZeroVector;
    FString Section;
};

UCLASS()
class COASTERSIM_API AYarlungSceneryActor : public AActor
{
    GENERATED_BODY()

public:
    AYarlungSceneryActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockOutcrops;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RiverbankBoulders;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ScreeBoulders;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UnderstoryClumps;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SlopeRockWallA;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SlopeRockWallB;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ForestShrubsA;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ForestShrubsB;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanopyTreesA;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanopyTreesB;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CanopyTreesC;

    UPROPERTY(EditAnywhere, Category = "Yarlung Scenery")
    FString TrackCsvRelativePath = YarlungGeneratedPaths::TrackCsvRelative;

    UPROPERTY(EditAnywhere, Category = "Yarlung Scenery")
    FString HeightmapRelativePath = YarlungGeneratedPaths::HeightmapRelative;

    void RebuildScenery();
    bool LoadSceneryTrack(TArray<FYarlungSceneryTrackSample>& OutSamples) const;
    bool LoadCorridorSourceHeights(TArray<uint16>& OutEncodedHeights) const;
    void ConfigureMeshesFromAssets(const FYarlungAssetConfig& AssetConfig);
    UHierarchicalInstancedStaticMeshComponent* ComponentByName(const FString& Name) const;
    void ClearAllInstances();
    void BuildScatter(const TArray<FYarlungSceneryTrackSample>& TrackSamples, const TArray<uint16>& EncodedHeights, const FYarlungAssetConfig& AssetConfig);
    bool TryResolvePlacement(
        const TArray<uint16>& EncodedHeights,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const class FYarlungRiverField& RiverField,
        const FVector& Location2D,
        float RiverClearanceCm,
        float MinHeightCm,
        float MaxHeightCm,
        float MinSlope,
        float MaxSlope,
        float& OutHeightCm,
        FVector& OutNormal) const;
    void AddScatterRule(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungSceneryComponentConfig& ComponentConfig,
        const FYarlungScatterKindConfig& KindConfig,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddSurfaceCoverLayer(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungSceneryComponentConfig& ComponentConfig,
        const FYarlungSurfaceCoverProfileConfig& Profile,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddRockWallSegments(
        const FYarlungAssetConfig& AssetConfig,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddRockWallSegment(
        const FYarlungRockWallSegmentConfig& Segment,
        const FYarlungRockWallProfileConfig& Profile,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField,
        int32& InOutPlacedCount);
    void ApplyMaterials(const FYarlungAssetConfig& AssetConfig);
};
