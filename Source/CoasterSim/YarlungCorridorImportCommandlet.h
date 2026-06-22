#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "YarlungCorridorImportCommandlet.generated.h"

UCLASS()
class COASTERSIM_API UYarlungCorridorImportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UYarlungCorridorImportCommandlet();

    virtual int32 Main(const FString& Params) override;
};
