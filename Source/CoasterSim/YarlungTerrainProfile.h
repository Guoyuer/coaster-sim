#pragma once

#include "CoreMinimal.h"

// Yarlung terrain georeferencing + river layout. The numeric values live in a
// single source of truth — Config/yarlung-terrain.json — which BOTH this C++ code
// (via Config()) and the Python asset pipeline (scripts/yarlung_config.py) read,
// so the runtime mesh and the generated heightmap/textures can never silently
// drift apart. The compiled-in defaults below are only a last-resort fallback if
// the JSON is missing; the CoasterSim.Yarlung.TerrainConfigParity automation test
// enforces that they (and the JSON) match the expected golden values.
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
    int32 GridSize = 1009;
    float MinXCm = -337778.4313411617f;
    float MaxXCm = 337778.4313411617f;
    float MinYCm = -416981.55087574443f;
    float MaxYCm = 416981.55087574443f;
    float EncodedMinZCm = 260000.0f;
    float EncodedMaxZCm = 730000.0f;
    float RiverAnchorXCm = 95543.0f;
    float RiverAnchorYCm = -142330.0f;
    float RiverZCm = 265200.0f;
    float RiverMaskHalfWidthCm = 26000.0f;
    TArray<FCenterlineTerm> RiverCenterlineTerms = {
        {9000.0f, 0.00009f, 0.25f},
        {4200.0f, 0.00021f, -0.6f}};
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
