#include "YarlungRiverField.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

bool LoadRiverCsv(const FString& CsvPath, TArray<FYarlungRiverRow>& OutRows, FString* OutError)
{
    OutRows.Reset();

    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *CsvPath))
    {
        if (OutError)
        {
            *OutError = FString::Printf(TEXT("Unable to read river CSV: %s"), *CsvPath);
        }
        return false;
    }

    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        TArray<FString> Columns;
        Lines[LineIndex].ParseIntoArray(Columns, TEXT(","), true);
        if (Columns.Num() < 6)
        {
            continue;
        }

        FYarlungRiverRow Row;
        Row.DistanceCm = FCString::Atod(*Columns[0]);
        Row.PositionCm = FVector(
            FCString::Atof(*Columns[1]),
            FCString::Atof(*Columns[2]),
            FCString::Atof(*Columns[3]));
        Row.HalfWidthCm = FCString::Atof(*Columns[4]);
        Row.Flow = FCString::Atof(*Columns[5]);
        OutRows.Add(Row);
    }

    return true;
}
}

FString FYarlungRiverField::ProjectContentRiverPath()
{
    return FPaths::ProjectContentDir() / TEXT("Generated/YarlungLandscape/YarlungRiver.csv");
}

bool FYarlungRiverField::LoadFromProjectContent(FString* OutError)
{
    const FString Path = ProjectContentRiverPath();
    if (!LoadRiverCsv(Path, Rows, OutError))
    {
        return false;
    }

    if (Rows.Num() < 4)
    {
        if (OutError)
        {
            *OutError = FString::Printf(TEXT("Generated Yarlung river has too few samples: %d (%s)"), Rows.Num(), *Path);
        }
        Rows.Reset();
        return false;
    }

    return true;
}

FYarlungRiverQuery FYarlungRiverField::QueryNearest(const FVector2D& Position) const
{
    FYarlungRiverQuery Query;
    if (Rows.IsEmpty())
    {
        Query.DistanceCm = TNumericLimits<float>::Max();
        return Query;
    }

    if (Rows.Num() == 1)
    {
        Query.Row = Rows[0];
        Query.DistanceCm = FVector2D::Distance(Position, FVector2D(Query.Row.PositionCm.X, Query.Row.PositionCm.Y));
        Query.SignedDistanceCm = Query.DistanceCm;
        Query.WaterSurfaceZCm = Query.Row.PositionCm.Z + DefaultWaterSurfaceLiftCm;
        Query.WaterHalfWidthCm = FMath::Clamp(Query.Row.HalfWidthCm * 0.38f, 4000.0f, 9000.0f);
        Query.bIsValid = true;
        return Query;
    }

    int32 BestSegmentIndex = INDEX_NONE;
    float BestSegmentT = 0.0f;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    FVector2D BestTangent(1.0f, 0.0f);
    FVector2D BestDelta = FVector2D::ZeroVector;
    FVector BestPosition = Rows[0].PositionCm;

    for (int32 Index = 0; Index + 1 < Rows.Num(); ++Index)
    {
        const FVector2D A(Rows[Index].PositionCm.X, Rows[Index].PositionCm.Y);
        const FVector2D B(Rows[Index + 1].PositionCm.X, Rows[Index + 1].PositionCm.Y);
        const FVector2D Segment = B - A;
        const float SegmentLengthSquared = Segment.SizeSquared();
        if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const float T = FMath::Clamp(FVector2D::DotProduct(Position - A, Segment) / SegmentLengthSquared, 0.0f, 1.0f);
        const FVector2D Closest = A + Segment * T;
        const FVector2D Delta = Position - Closest;
        const float DistanceSquared = Delta.SizeSquared();
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestSegmentIndex = Index;
            BestSegmentT = T;
            BestTangent = Segment.GetSafeNormal();
            if (BestTangent.IsNearlyZero())
            {
                BestTangent = FVector2D(1.0f, 0.0f);
            }
            BestDelta = Delta;
            BestPosition = FMath::Lerp(Rows[Index].PositionCm, Rows[Index + 1].PositionCm, T);
        }
    }

    if (BestSegmentIndex == INDEX_NONE)
    {
        return Query;
    }

    const FYarlungRiverRow& A = Rows[BestSegmentIndex];
    const FYarlungRiverRow& B = Rows[BestSegmentIndex + 1];
    Query.Row.DistanceCm = FMath::Lerp(A.DistanceCm, B.DistanceCm, static_cast<double>(BestSegmentT));
    Query.Row.PositionCm = BestPosition;
    Query.Row.HalfWidthCm = FMath::Lerp(A.HalfWidthCm, B.HalfWidthCm, BestSegmentT);
    Query.Row.Flow = FMath::Lerp(A.Flow, B.Flow, BestSegmentT);
    Query.DistanceCm = FMath::Sqrt(BestDistanceSquared);
    Query.WaterSurfaceZCm = Query.Row.PositionCm.Z + DefaultWaterSurfaceLiftCm;
    Query.WaterHalfWidthCm = FMath::Clamp(Query.Row.HalfWidthCm * 0.38f, 4000.0f, 9000.0f);
    Query.bIsValid = true;

    const FVector2D Right(-BestTangent.Y, BestTangent.X);
    Query.SignedDistanceCm = FVector2D::DotProduct(BestDelta, Right);
    return Query;
}

float FYarlungRiverField::DistanceCm(const FVector2D& Position) const
{
    return QueryNearest(Position).DistanceCm;
}

float FYarlungRiverField::ProtectionMask(const FVector2D& Position, float InnerCm, float FadeCm) const
{
    return Smooth01((DistanceCm(Position) - InnerCm) / FadeCm);
}
