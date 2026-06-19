#include "CoasterTrackComponent.h"

void UCoasterTrackComponent::RebuildFromControlPoints(const TArray<FVector>& ControlPoints)
{
    ClearSplinePoints(false);

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

FName UCoasterTrackComponent::SectionName(ECoasterSection Section)
{
    switch (Section)
    {
    case ECoasterSection::Station:
        return TEXT("Station");
    case ECoasterSection::Lift:
        return TEXT("Lift");
    case ECoasterSection::Launch:
        return TEXT("Launch");
    case ECoasterSection::Brake:
        return TEXT("Brake");
    case ECoasterSection::Coast:
    default:
        return TEXT("Coast");
    }
}
