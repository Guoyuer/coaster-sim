#include "CoasterRideCapture.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace CoasterRideCapture
{
bool FState::ConfigureFromCommandLine()
{
    const TCHAR* CommandLine = FCommandLine::Get();
    FString TimesValue;
    if (!FParse::Value(CommandLine, TEXT("YarlungBatchShotTimes="), TimesValue))
    {
        return false;
    }

    TimesValue.ReplaceInline(TEXT(","), TEXT("+"));
    TimesValue.ReplaceInline(TEXT(";"), TEXT("+"));
    TArray<FString> Tokens;
    TimesValue.ParseIntoArray(Tokens, TEXT("+"), true);
    for (FString& Token : Tokens)
    {
        Token.TrimStartAndEndInline();
        if (!Token.IsEmpty())
        {
            ShotTimes.Add(FCString::Atof(*Token));
        }
    }

    if (ShotTimes.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("YarlungBatchShotTimes was provided but contained no usable times: %s"), *TimesValue);
        return false;
    }

    OutputDir = FPaths::ProjectSavedDir() / TEXT("OffscreenShots");
    FParse::Value(CommandLine, TEXT("YarlungBatchShotDir="), OutputDir);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotPrefix="), Prefix);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotResX="), ResX);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotResY="), ResY);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotSettleFrames="), SettleFrames);
    FParse::Value(CommandLine, TEXT("YarlungBatchShotPostFrames="), PostFrames);

    ResX = FMath::Max(ResX, 320);
    ResY = FMath::Max(ResY, 180);
    SettleFrames = FMath::Clamp(SettleFrames, 1, 60);
    PostFrames = FMath::Clamp(PostFrames, 1, 120);
    FPaths::NormalizeDirectoryName(OutputDir);
    IFileManager::Get().MakeDirectory(*OutputDir, true);

    ShotIndex = 0;
    Stage = 0;
    FramesRemaining = 0;
    bActive = true;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Yarlung batch screenshots enabled: count=%d prefix=%s output=%s res=%dx%d settle=%d post=%d"),
        ShotTimes.Num(),
        *Prefix,
        *OutputDir,
        ResX,
        ResY,
        SettleFrames,
        PostFrames);
    return true;
}

bool FState::Tick(TFunctionRef<void(float)> PositionRideForSeconds)
{
    if (!bActive)
    {
        return false;
    }

    if (ShotIndex >= ShotTimes.Num())
    {
        bActive = false;
        FPlatformMisc::RequestExit(false);
        return true;
    }

    if (Stage == 0)
    {
        PositionRideForSeconds(ShotTimes[ShotIndex]);
        FramesRemaining = SettleFrames;
        Stage = 1;
        return true;
    }

    if (Stage == 1)
    {
        --FramesRemaining;
        if (FramesRemaining > 0)
        {
            return true;
        }
        RequestCurrentScreenshot();
        FramesRemaining = PostFrames;
        Stage = 2;
        return true;
    }

    --FramesRemaining;
    if (FramesRemaining > 0)
    {
        return true;
    }

    ++ShotIndex;
    Stage = 0;
    return true;
}

void FState::RequestCurrentScreenshot()
{
    if (ShotIndex >= ShotTimes.Num())
    {
        return;
    }

    const int32 TimeLabel = FMath::RoundToInt(ShotTimes[ShotIndex]);
    const FString Filename = OutputDir / FString::Printf(TEXT("%s-t%d.png"), *Prefix, TimeLabel);
    FString StandardFilename = Filename;
    FPaths::MakeStandardFilename(StandardFilename);
    UE_LOG(LogTemp, Display, TEXT("Yarlung batch screenshot %d/%d at t=%.2fs -> %s"), ShotIndex + 1, ShotTimes.Num(), ShotTimes[ShotIndex], *StandardFilename);
    FScreenshotRequest::RequestScreenshot(StandardFilename, false, false);
}
}
