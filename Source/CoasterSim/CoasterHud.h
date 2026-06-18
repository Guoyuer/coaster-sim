#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CoasterHud.generated.h"

UCLASS()
class COASTERSIM_API ACoasterHud : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
