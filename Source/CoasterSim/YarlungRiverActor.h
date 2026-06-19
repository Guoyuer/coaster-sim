#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungRiverActor.generated.h"

class UProceduralMeshComponent;

USTRUCT()
struct FYarlungRiverSample
{
    GENERATED_BODY()

    FVector Center = FVector::ZeroVector;
    float HalfWidthCm = 22000.0f;
    float Flow = 0.0f;
};

UCLASS()
class COASTERSIM_API AYarlungRiverActor : public AActor
{
    GENERATED_BODY()

public:
    AYarlungRiverActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "River")
    TObjectPtr<UProceduralMeshComponent> WaterMesh;

    UPROPERTY(VisibleAnywhere, Category = "River")
    TObjectPtr<UProceduralMeshComponent> FoamMesh;

    UPROPERTY(EditAnywhere, Category = "River")
    FString RiverCsvRelativePath = TEXT("Generated/YarlungLandscape/YarlungRiver.csv");

    void RebuildRiver();
    bool LoadRiverSamples(TArray<FYarlungRiverSample>& OutSamples) const;
    void BuildWaterMesh(const TArray<FYarlungRiverSample>& Samples);
    void BuildFoamMesh(const TArray<FYarlungRiverSample>& Samples);
    void ApplyMaterials();
};
