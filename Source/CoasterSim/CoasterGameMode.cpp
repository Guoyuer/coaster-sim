#include "CoasterGameMode.h"

#include "CoasterHud.h"
#include "CoasterRideActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

ACoasterGameMode::ACoasterGameMode()
{
    HUDClass = ACoasterHud::StaticClass();
    DefaultPawnClass = nullptr;
}

void ACoasterGameMode::BeginPlay()
{
    Super::BeginPlay();

    ACoasterRideActor* Ride = nullptr;
    for (TActorIterator<ACoasterRideActor> It(GetWorld()); It; ++It)
    {
        Ride = *It;
        break;
    }

    if (!Ride)
    {
        Ride = GetWorld()->SpawnActor<ACoasterRideActor>(ACoasterRideActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    }

    if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
    {
        PlayerController->SetViewTarget(Ride);
        PlayerController->bShowMouseCursor = false;
    }
}
