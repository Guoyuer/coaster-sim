#include "YarlungMeshTerrainActor.h"

#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

void AYarlungMeshTerrainActor::BeginPlay()
{
    Super::BeginPlay();

    if (FParse::Param(FCommandLine::Get(), TEXT("YarlungHideTerrain")))
    {
        SetActorHiddenInGame(true);
        TerrainMesh->SetVisibility(false, true);
    }
}
