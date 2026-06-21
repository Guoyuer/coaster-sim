#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungCanyonWallActor.generated.h"

class UProceduralMeshComponent;

USTRUCT()
struct FYarlungCanyonWallTrackSample
{
    GENERATED_BODY()

    FVector Position = FVector::ZeroVector;
    float TerrainZCm = 0.0f;
};

UCLASS()
class COASTERSIM_API AYarlungCanyonWallActor : public AActor
{
    GENERATED_BODY()

public:
    AYarlungCanyonWallActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Canyon Wall")
    TObjectPtr<UProceduralMeshComponent> ForestApronMesh;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Canyon Wall")
    TObjectPtr<UProceduralMeshComponent> WetCliffMesh;

    UPROPERTY(EditAnywhere, Category = "Yarlung Canyon Wall")
    FString TrackCsvRelativePath = TEXT("Generated/YarlungLandscape/YarlungTrack.csv");

    UPROPERTY(EditAnywhere, Category = "Yarlung Canyon Wall")
    FString HeightmapRelativePath = TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16");

    void RebuildCanyonWalls();
    bool LoadOutboundTrack(TArray<FYarlungCanyonWallTrackSample>& OutSamples) const;
    bool LoadHeightmap(TArray<uint16>& OutHeightData) const;
    float SampleHeightCm(const TArray<uint16>& HeightData, float X, float Y) const;
    void BuildForestApron(const TArray<FYarlungCanyonWallTrackSample>& TrackSamples, const TArray<uint16>& HeightData);
    void BuildWetCliff(const TArray<FYarlungCanyonWallTrackSample>& TrackSamples, const TArray<uint16>& HeightData);
    void ApplyMaterials();
};
