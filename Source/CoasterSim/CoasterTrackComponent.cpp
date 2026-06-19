#include "CoasterTrackComponent.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool UCoasterTrackComponent::LoadGeneratedTrack(const FString& CsvPath)
{
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *CsvPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Generated coaster track CSV is required but missing: %s"), *CsvPath);
        ClearSplinePoints(false);
        SectionRanges.Reset();
        GeneratedRollSampleDistancesCm.Reset();
        GeneratedRollDegrees.Reset();
        UpdateSpline();
        return false;
    }

    TArray<FVector> Points;
    TArray<ECoasterSection> Sections;
    TArray<float> RollDegrees;

    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        const FString Line = Lines[LineIndex].TrimStartAndEnd();
        if (Line.IsEmpty())
        {
            continue;
        }

        TArray<FString> Columns;
        Line.ParseIntoArray(Columns, TEXT(","), true);
        if (Columns.Num() < 7)
        {
            UE_LOG(LogTemp, Error, TEXT("Invalid YarlungTrack.csv row %d: expected 7 columns, got %d"), LineIndex + 1, Columns.Num());
            ClearSplinePoints(false);
            SectionRanges.Reset();
            GeneratedRollSampleDistancesCm.Reset();
            GeneratedRollDegrees.Reset();
            UpdateSpline();
            return false;
        }

        Points.Add(FVector(
            FCString::Atof(*Columns[1]),
            FCString::Atof(*Columns[2]),
            FCString::Atof(*Columns[3])));
        RollDegrees.Add(FCString::Atof(*Columns[4]));
        Sections.Add(ParseSectionName(Columns[5]));
    }

    if (Points.Num() < 4)
    {
        UE_LOG(LogTemp, Error, TEXT("Generated coaster track CSV must contain at least 4 points: %s"), *CsvPath);
        ClearSplinePoints(false);
        SectionRanges.Reset();
        GeneratedRollSampleDistancesCm.Reset();
        GeneratedRollDegrees.Reset();
        UpdateSpline();
        return false;
    }

    RebuildFromControlPoints(Points);
    GeneratedRollDegrees = MoveTemp(RollDegrees);
    BuildSectionRanges(Points, Sections);

    UE_LOG(LogTemp, Display, TEXT("Loaded generated coaster track: %s (%d points, %.1fm)"),
        *FPaths::GetCleanFilename(CsvPath),
        Points.Num(),
        GetTrackLengthCm() / 100.0f);
    return true;
}

void UCoasterTrackComponent::RebuildFromControlPoints(const TArray<FVector>& ControlPoints)
{
    ClearSplinePoints(false);
    SectionRanges.Reset();
    GeneratedRollSampleDistancesCm.Reset();
    GeneratedRollDegrees.Reset();

    for (int32 Index = 0; Index < ControlPoints.Num(); ++Index)
    {
        AddSplinePoint(ControlPoints[Index], ESplineCoordinateSpace::Local, false);
        SetSplinePointType(Index, ESplinePointType::Curve, false);
    }

    SetClosedLoop(true, false);
    UpdateSpline();
}

float UCoasterTrackComponent::GetTrackLengthCm() const
{
    return FMath::Max(GetSplineLength(), 1.0f);
}

void UCoasterTrackComponent::SampleBaseFrame(
    float DistanceCm,
    FVector& OutLocation,
    FRotator& OutRotation,
    FVector& OutForward,
    FVector& OutRight,
    FVector& OutUp) const
{
    const float TrackLengthCm = GetTrackLengthCm();
    const float WrappedDistance = FMath::Fmod(FMath::Max(DistanceCm, 0.0f), TrackLengthCm);
    OutLocation = GetLocationAtDistanceAlongSpline(WrappedDistance, ESplineCoordinateSpace::Local);
    OutForward = GetDirectionAtDistanceAlongSpline(WrappedDistance, ESplineCoordinateSpace::Local).GetSafeNormal();

    OutRight = FVector::CrossProduct(FVector::UpVector, OutForward).GetSafeNormal();
    if (OutRight.IsNearlyZero())
    {
        OutRight = FVector::RightVector;
    }

    OutUp = FVector::CrossProduct(OutForward, OutRight).GetSafeNormal();
    OutRotation = FRotationMatrix::MakeFromXZ(OutForward, OutUp).Rotator();
}

ECoasterSection UCoasterTrackComponent::GetLegacySection(float TrackRatio) const
{
    if (TrackRatio < 0.04f)
    {
        return ECoasterSection::Station;
    }
    if (TrackRatio < 0.24f)
    {
        return ECoasterSection::Lift;
    }
    if (TrackRatio > 0.56f && TrackRatio < 0.62f)
    {
        return ECoasterSection::Launch;
    }
    if (TrackRatio > 0.88f)
    {
        return ECoasterSection::Brake;
    }
    return ECoasterSection::Coast;
}

FName UCoasterTrackComponent::GetLegacySectionName(float TrackRatio) const
{
    return SectionName(GetLegacySection(TrackRatio));
}

ECoasterSection UCoasterTrackComponent::GetSectionAtDistance(float DistanceCm) const
{
    if (SectionRanges.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Generated coaster section ranges are unavailable; returning Coast without legacy fallback."));
        return ECoasterSection::Coast;
    }

    const float TrackLengthCm = GetTrackLengthCm();
    float WrappedDistance = FMath::Fmod(DistanceCm, TrackLengthCm);
    if (WrappedDistance < 0.0f)
    {
        WrappedDistance += TrackLengthCm;
    }

    for (const FCoasterSectionRange& Range : SectionRanges)
    {
        if (WrappedDistance >= Range.StartDistanceCm && WrappedDistance < Range.EndDistanceCm)
        {
            return Range.Section;
        }
    }

    return SectionRanges.Last().Section;
}

FName UCoasterTrackComponent::GetSectionNameAtDistance(float DistanceCm) const
{
    return SectionName(GetSectionAtDistance(DistanceCm));
}

float UCoasterTrackComponent::GetGeneratedBankRadiansAtDistance(float DistanceCm) const
{
    if (GeneratedRollDegrees.Num() < 2 || GeneratedRollSampleDistancesCm.Num() != GeneratedRollDegrees.Num() + 1)
    {
        UE_LOG(LogTemp, Error, TEXT("Generated coaster roll samples are unavailable; using zero bank."));
        return 0.0f;
    }

    const float TrackLengthCm = GetTrackLengthCm();
    float WrappedDistance = FMath::Fmod(DistanceCm, TrackLengthCm);
    if (WrappedDistance < 0.0f)
    {
        WrappedDistance += TrackLengthCm;
    }

    for (int32 Index = 0; Index < GeneratedRollDegrees.Num(); ++Index)
    {
        const float StartDistance = GeneratedRollSampleDistancesCm[Index];
        const float EndDistance = GeneratedRollSampleDistancesCm[Index + 1];
        if (WrappedDistance >= StartDistance && WrappedDistance < EndDistance)
        {
            const float Alpha = (EndDistance > StartDistance)
                ? (WrappedDistance - StartDistance) / (EndDistance - StartDistance)
                : 0.0f;
            const float StartRoll = GeneratedRollDegrees[Index];
            const float EndRoll = GeneratedRollDegrees[(Index + 1) % GeneratedRollDegrees.Num()];
            return FMath::DegreesToRadians(FMath::Lerp(StartRoll, EndRoll, Alpha));
        }
    }

    return FMath::DegreesToRadians(GeneratedRollDegrees.Last());
}

FName UCoasterTrackComponent::SectionName(ECoasterSection Section)
{
    switch (Section)
    {
    case ECoasterSection::Station:
        return TEXT("Station");
    case ECoasterSection::Lift:
        return TEXT("Lift");
    case ECoasterSection::Outbound:
        return TEXT("Outbound");
    case ECoasterSection::Turnaround:
        return TEXT("Turnaround");
    case ECoasterSection::Return:
        return TEXT("Return");
    case ECoasterSection::Launch:
        return TEXT("Launch");
    case ECoasterSection::Brake:
        return TEXT("Brake");
    case ECoasterSection::Coast:
    default:
        return TEXT("Coast");
    }
}

ECoasterSection UCoasterTrackComponent::ParseSectionName(const FString& Value)
{
    const FString Normalized = Value.TrimStartAndEnd().ToLower();
    if (Normalized == TEXT("station"))
    {
        return ECoasterSection::Station;
    }
    if (Normalized == TEXT("lift"))
    {
        return ECoasterSection::Lift;
    }
    if (Normalized == TEXT("outbound"))
    {
        return ECoasterSection::Outbound;
    }
    if (Normalized == TEXT("turnaround"))
    {
        return ECoasterSection::Turnaround;
    }
    if (Normalized == TEXT("return"))
    {
        return ECoasterSection::Return;
    }
    if (Normalized == TEXT("launch"))
    {
        return ECoasterSection::Launch;
    }
    if (Normalized == TEXT("brake"))
    {
        return ECoasterSection::Brake;
    }
    return ECoasterSection::Coast;
}

void UCoasterTrackComponent::BuildSectionRanges(const TArray<FVector>& Points, const TArray<ECoasterSection>& Sections)
{
    SectionRanges.Reset();
    if (Points.Num() < 2 || Sections.Num() != Points.Num())
    {
        return;
    }

    TArray<float> CumulativeChordDistances;
    CumulativeChordDistances.Reserve(Points.Num() + 1);
    CumulativeChordDistances.Add(0.0f);

    float TotalChordDistance = 0.0f;
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        const int32 NextIndex = (Index + 1) % Points.Num();
        TotalChordDistance += FVector::Distance(Points[Index], Points[NextIndex]);
        CumulativeChordDistances.Add(TotalChordDistance);
    }

    if (TotalChordDistance <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float SplineLengthCm = GetTrackLengthCm();
    GeneratedRollSampleDistancesCm.Reset();
    GeneratedRollSampleDistancesCm.Reserve(Points.Num() + 1);
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        GeneratedRollSampleDistancesCm.Add(CumulativeChordDistances[Index] / TotalChordDistance * SplineLengthCm);

        FCoasterSectionRange Range;
        Range.StartDistanceCm = CumulativeChordDistances[Index] / TotalChordDistance * SplineLengthCm;
        Range.EndDistanceCm = CumulativeChordDistances[Index + 1] / TotalChordDistance * SplineLengthCm;
        Range.Section = Sections[Index];

        if (!SectionRanges.IsEmpty()
            && SectionRanges.Last().Section == Range.Section
            && FMath::IsNearlyEqual(SectionRanges.Last().EndDistanceCm, Range.StartDistanceCm, 1.0f))
        {
            SectionRanges.Last().EndDistanceCm = Range.EndDistanceCm;
        }
        else
        {
            SectionRanges.Add(Range);
        }
    }
    GeneratedRollSampleDistancesCm.Add(SplineLengthCm);
}
