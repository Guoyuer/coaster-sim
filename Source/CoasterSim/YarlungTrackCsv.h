#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"

// Single source of truth for parsing Content/Generated/YarlungLandscape/YarlungTrack.csv.
// The same file used to be hand-parsed (with subtly different column handling and
// error logging) in CoasterTrackComponent, the import commandlet, and the scenery
// actor. Centralizing the column contract here removes that drift risk.
//
// Columns: 0=distance_cm, 1=x, 2=y, 3=z, 4=roll_deg, 5=section, 6=terrain_z
struct FYarlungTrackRow
{
    double DistanceCm = 0.0;
    FVector PositionCm = FVector::ZeroVector;
    float RollDegrees = 0.0f;
    FString Section;
    float TerrainZCm = 0.0f;
};

namespace YarlungTrackCsv
{
    inline bool Load(const FString& CsvPath, TArray<FYarlungTrackRow>& OutRows, FString* OutError = nullptr)
    {
        OutRows.Reset();

        TArray<FString> Lines;
        if (!FFileHelper::LoadFileToStringArray(Lines, *CsvPath))
        {
            if (OutError)
            {
                *OutError = FString::Printf(TEXT("Unable to read track CSV: %s"), *CsvPath);
            }
            return false;
        }

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
                if (OutError)
                {
                    *OutError = FString::Printf(
                        TEXT("Invalid YarlungTrack.csv row %d: expected >=7 columns, got %d"),
                        LineIndex + 1,
                        Columns.Num());
                }
                return false;
            }

            FYarlungTrackRow Row;
            Row.DistanceCm = FCString::Atod(*Columns[0]);
            Row.PositionCm = FVector(
                FCString::Atof(*Columns[1]),
                FCString::Atof(*Columns[2]),
                FCString::Atof(*Columns[3]));
            Row.RollDegrees = FCString::Atof(*Columns[4]);
            Row.Section = Columns[5].TrimStartAndEnd();
            Row.TerrainZCm = FCString::Atof(*Columns[6]);
            OutRows.Add(Row);
        }

        return true;
    }
}
