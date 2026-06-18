#pragma once

#include "Commandlets/Commandlet.h"
#include "CoreMinimal.h"
#include "YarlungLandscapeImportCommandlet.generated.h"

UCLASS()
class COASTERSIM_API UYarlungLandscapeImportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UYarlungLandscapeImportCommandlet();

    virtual int32 Main(const FString& Params) override;
};
