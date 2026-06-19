#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "CoasterTrackComponent.generated.h"

UENUM(BlueprintType)
enum class ECoasterSection : uint8
{
    Station,
    Lift,
    Coast,
    Launch,
    Brake
};

UCLASS(ClassGroup = (Coaster), meta = (BlueprintSpawnableComponent))
class COASTERSIM_API UCoasterTrackComponent : public USplineComponent
{
    GENERATED_BODY()

public:
    void RebuildFromControlPoints(const TArray<FVector>& ControlPoints);
    float GetTrackLengthCm() const;
    void SampleBaseFrame(
        float DistanceCm,
        FVector& OutLocation,
        FRotator& OutRotation,
        FVector& OutForward,
        FVector& OutRight,
        FVector& OutUp) const;
    ECoasterSection GetLegacySection(float TrackRatio) const;
    FName GetLegacySectionName(float TrackRatio) const;

    static FName SectionName(ECoasterSection Section);
};
