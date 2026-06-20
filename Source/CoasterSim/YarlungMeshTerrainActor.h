#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YarlungMeshTerrainActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class COASTERSIM_API AYarlungMeshTerrainActor : public AActor
{
    GENERATED_BODY()

public:
    AYarlungMeshTerrainActor();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Yarlung Terrain")
    TObjectPtr<UStaticMeshComponent> TerrainMesh;
};
