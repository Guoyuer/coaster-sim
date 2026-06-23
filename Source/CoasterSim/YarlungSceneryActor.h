#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungGeneratedPaths.h"
#include "YarlungViewCorridor.h"
#include "YarlungSceneryActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
struct FYarlungAssetConfig;
struct FYarlungCanopyBeltConfig;
struct FYarlungCliffBeltConfig;
struct FYarlungGroundCoverBeltConfig;
struct FYarlungSceneryComponentConfig;
struct FYarlungScatterKindConfig;
struct FYarlungSlopeRockWallBeltConfig;

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
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CliffRockFacesA;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CliffRockFacesB;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CliffRockFacesC;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CliffRockFacesD;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CliffRockFacesE;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Scenery")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CliffRockFacesF;

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
    void AddCanopyBelt(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungCanopyBeltConfig& BeltConfig,
        float Seed,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddCliffBelt(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungCliffBeltConfig& BeltConfig,
        float Seed,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddRiverWallCliffs(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungCliffBeltConfig& BeltConfig,
        float Seed,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddGroundCoverBelt(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungSceneryComponentConfig& ComponentConfig,
        const FYarlungScatterKindConfig& KindConfig,
        const FYarlungGroundCoverBeltConfig& BeltConfig,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void AddSlopeRockWallBelt(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FYarlungSceneryComponentConfig& ComponentConfig,
        const FYarlungScatterKindConfig& KindConfig,
        const FYarlungSlopeRockWallBeltConfig& BeltConfig,
        const TArray<FYarlungSceneryTrackSample>& TrackSamples,
        const TArray<YarlungViewCorridor::FTrackPoint>& TerrainTrackPoints,
        const TArray<uint16>& EncodedHeights,
        const class FYarlungRiverField& RiverField);
    void ApplyMaterials(const FYarlungAssetConfig& AssetConfig);
};
