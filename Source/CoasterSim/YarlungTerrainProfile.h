#pragma once

#include "CoreMinimal.h"

// Yarlung terrain georeferencing + river layout. The numeric values live in a
// SINGLE source of truth — Config/yarlung-terrain.json — which BOTH this C++ code
// (via Config()) and the Python asset pipeline (scripts/yarlung_config.py) read,
// so the runtime mesh and the generated source height data can never silently
// drift apart. There are deliberately NO compiled-in constants here: the fields
// default to zero and are populated only from the JSON. A missing/invalid file is
// a hard setup error: Config() fails the process rather than returning a zeroed
// config that could generate plausible-looking but wrong terrain.
namespace YarlungTerrain
{
struct FCenterlineTerm
{
    float AmpCm = 0.0f;
    float Freq = 0.0f;
    float Phase = 0.0f;
};

struct FConfig
{
    int32 GridSize = 0;
    float MinXCm = 0.0f;
    float MaxXCm = 0.0f;
    float MinYCm = 0.0f;
    float MaxYCm = 0.0f;
    float EncodedMinZCm = 0.0f;
    float EncodedMaxZCm = 0.0f;
    float RiverAnchorXCm = 0.0f;
    float RiverAnchorYCm = 0.0f;
    float RiverZCm = 0.0f;
    float RiverMaskHalfWidthCm = 0.0f;
    TArray<FCenterlineTerm> RiverCenterlineTerms;
};

// Lazily loads + caches Config/yarlung-terrain.json on first call.
const FConfig& Config();

inline float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

float HeightValueToCm(uint16 Encoded);
float NormalizeEncodedHeightCm(float HeightCm);
float RiverCenterY(float X);
}
