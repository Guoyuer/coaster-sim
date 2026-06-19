#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungCliffActor.generated.h"

class UProceduralMeshComponent;

USTRUCT()
struct FYarlungCliffPathSample
{
    GENERATED_BODY()

    FVector Center = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    FVector Right = FVector::RightVector;
};

UCLASS()
class COASTERSIM_API AYarlungCliffActor : public AActor
{
    GENERATED_BODY()

public:
    AYarlungCliffActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Cliff")
    TObjectPtr<UProceduralMeshComponent> CliffMesh;

    UPROPERTY(EditAnywhere, Category = "Cliff")
    FString TrackCsvRelativePath = TEXT("Generated/YarlungLandscape/YarlungTrack.csv");

    UPROPERTY(EditAnywhere, Category = "Cliff")
    FString HeightmapRelativePath = TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16");

    void RebuildCliff();
    bool LoadOutboundPath(TArray<FYarlungCliffPathSample>& OutSamples) const;
    bool LoadHeightmap(TArray<uint16>& OutHeightData) const;
    float SampleHeightCm(const TArray<uint16>& HeightData, float X, float Y) const;
    void BuildCliffMesh(const TArray<FYarlungCliffPathSample>& PathSamples, const TArray<uint16>& HeightData);
    void ApplyMaterial();
};
