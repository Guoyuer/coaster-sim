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

    int32 BestIndex = INDEX_NONE;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const FVector2D RiverPosition(Rows[Index].PositionCm.X, Rows[Index].PositionCm.Y);
        const float DistanceSquared = FVector2D::DistSquared(Position, RiverPosition);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestIndex = Index;
        }
    }

    if (BestIndex == INDEX_NONE)
    {
        return Query;
    }

    Query.Row = Rows[BestIndex];
    Query.DistanceCm = FMath::Sqrt(BestDistanceSquared);
    Query.WaterSurfaceZCm = Query.Row.PositionCm.Z + DefaultWaterSurfaceLiftCm;
    Query.WaterHalfWidthCm = FMath::Clamp(Query.Row.HalfWidthCm * 0.275f, 4500.0f, 8000.0f);
    Query.bIsValid = true;

    const int32 PreviousIndex = FMath::Max(0, BestIndex - 1);
    const int32 NextIndex = FMath::Min(Rows.Num() - 1, BestIndex + 1);
    const FVector2D Previous(Rows[PreviousIndex].PositionCm.X, Rows[PreviousIndex].PositionCm.Y);
    const FVector2D Next(Rows[NextIndex].PositionCm.X, Rows[NextIndex].PositionCm.Y);
    const FVector2D Tangent = (Next - Previous).GetSafeNormal();
    const FVector2D Right(-Tangent.Y, Tangent.X);
    const FVector2D Delta = Position - FVector2D(Query.Row.PositionCm.X, Query.Row.PositionCm.Y);
    Query.SignedDistanceCm = FVector2D::DotProduct(Delta, Right);
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
