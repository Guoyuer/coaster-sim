#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

namespace CoasterRideCapture
{
class FState
{
public:
    bool ConfigureFromCommandLine();
    bool Tick(TFunctionRef<void(float)> PositionRideForSeconds);
    bool IsActive() const { return bActive; }

private:
    void RequestCurrentScreenshot();

    bool bActive = false;
    TArray<float> ShotTimes;
    FString OutputDir;
    FString Prefix = TEXT("visual-survey");
    int32 ResX = 1920;
    int32 ResY = 1080;
    int32 ShotIndex = 0;
    int32 Stage = 0;
    int32 FramesRemaining = 0;
    int32 SettleFrames = 4;
    int32 PostFrames = 8;
};
}
