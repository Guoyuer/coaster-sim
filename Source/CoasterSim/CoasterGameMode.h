#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CoasterGameMode.generated.h"

UCLASS()
class COASTERSIM_API ACoasterGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACoasterGameMode();

    virtual void BeginPlay() override;
};
