#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungSceneryActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

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
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> UnderstoryClumps;

    UPROPERTY(EditAnywhere, Category = "Yarlung Scenery")
    FString TrackCsvRelativePath = TEXT("Generated/YarlungLandscape/YarlungTrack.csv");

    UPROPERTY(EditAnywhere, Category = "Yarlung Scenery")
    FString HeightmapRelativePath = TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16");

    void RebuildScenery();
    bool LoadOutboundTrack(TArray<FYarlungSceneryTrackSample>& OutSamples) const;
    bool LoadHeightmap(TArray<uint16>& OutHeightData) const;
    float SampleHeightCm(const TArray<uint16>& HeightData, float X, float Y) const;
    FVector SampleNormal(const TArray<uint16>& HeightData, float X, float Y) const;
    void BuildScatter(const TArray<FYarlungSceneryTrackSample>& TrackSamples, const TArray<uint16>& HeightData);
    void ApplyMaterials();
};
