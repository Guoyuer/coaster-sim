#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"

// Single source of truth for parsing Content/Generated/YarlungLandscape/YarlungRiver.csv.
// Previously hand-parsed in two places (AYarlungRiverActor for the water mesh and
// ACoasterRideActor for the valley-fog anchor) with different column handling.
//
// Columns: 0=distance_cm, 1=x, 2=y, 3=z, 4=half_width_cm, 5=flow
// Rows with fewer than 6 columns are skipped (matches the prior river-actor behaviour).
struct FYarlungRiverRow
{
    double DistanceCm = 0.0;
    FVector PositionCm = FVector::ZeroVector;
    float HalfWidthCm = 0.0f;
    float Flow = 0.0f;
};

namespace YarlungRiverCsv
{
    inline bool Load(const FString& CsvPath, TArray<FYarlungRiverRow>& OutRows, FString* OutError = nullptr)
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
