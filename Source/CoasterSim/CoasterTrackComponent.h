#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "CoasterTrackComponent.generated.h"

UENUM(BlueprintType)
enum class ECoasterSection : uint8
{
    Station,
    Lift,
    Outbound,
    Turnaround,
    Return,
    Coast,
    Launch,
    Brake
};

struct FCoasterSectionRange
{
    float StartDistanceCm = 0.0f;
    float EndDistanceCm = 0.0f;
    ECoasterSection Section = ECoasterSection::Coast;
};

UCLASS(ClassGroup = (Coaster), meta = (BlueprintSpawnableComponent))
class COASTERSIM_API UCoasterTrackComponent : public USplineComponent
{
    GENERATED_BODY()

public:
    bool LoadGeneratedTrack(const FString& CsvPath);
    float GetTrackLengthCm() const;
    void SampleBaseFrame(
        float DistanceCm,
        FVector& OutLocation,
        FRotator& OutRotation,
        FVector& OutForward,
        FVector& OutRight,
        FVector& OutUp) const;
    ECoasterSection GetSectionAtDistance(float DistanceCm) const;
    FName GetSectionNameAtDistance(float DistanceCm) const;
    float GetGeneratedBankRadiansAtDistance(float DistanceCm) const;
    float GetGeneratedTerrainZAtDistance(float DistanceCm) const;

    static FName SectionName(ECoasterSection Section);

private:
    static ECoasterSection ParseSectionName(const FString& Value);
    void RebuildFromControlPoints(const TArray<FVector>& ControlPoints);
    void BuildSectionRanges(const TArray<FVector>& Points, const TArray<ECoasterSection>& Sections);

    TArray<FCoasterSectionRange> SectionRanges;
    TArray<float> GeneratedRollSampleDistancesCm;
    TArray<float> GeneratedRollDegrees;
    TArray<float> GeneratedTerrainZCm;
};
