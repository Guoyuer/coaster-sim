#include "YarlungMeshTerrainActor.h"

#include "Components/StaticMeshComponent.h"

AYarlungMeshTerrainActor::AYarlungMeshTerrainActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    TerrainMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("YarlungMeshTerrain"));
    TerrainMesh->SetupAttachment(SceneRoot);
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainMesh->SetCastShadow(false);
}
