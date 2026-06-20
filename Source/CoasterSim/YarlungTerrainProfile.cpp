#include "YarlungTerrainProfile.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace YarlungTerrain
{
namespace
{
FConfig LoadConfigFromDisk()
{
    FConfig Cfg; // struct defaults act as the fallback

    const FString Path = FPaths::ProjectDir() / TEXT("Config/yarlung-terrain.json");
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        UE_LOG(LogTemp, Error,
            TEXT("YarlungTerrain: missing %s; using fallback constants (TerrainConfigParity test should flag this)"),
            *Path);
        return Cfg;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("YarlungTerrain: failed to parse %s; using fallback constants"), *Path);
        return Cfg;
    }

    Cfg.GridSize = Root->GetIntegerField(TEXT("grid_size"));

    const TSharedPtr<FJsonObject> Bounds = Root->GetObjectField(TEXT("world_bounds_cm"));
    Cfg.MinXCm = Bounds->GetNumberField(TEXT("min_x"));
    Cfg.MaxXCm = Bounds->GetNumberField(TEXT("max_x"));
    Cfg.MinYCm = Bounds->GetNumberField(TEXT("min_y"));
    Cfg.MaxYCm = Bounds->GetNumberField(TEXT("max_y"));

    const TSharedPtr<FJsonObject> Height = Root->GetObjectField(TEXT("encoded_height_cm"));
    Cfg.EncodedMinZCm = Height->GetNumberField(TEXT("min"));
    Cfg.EncodedMaxZCm = Height->GetNumberField(TEXT("max"));

    const TSharedPtr<FJsonObject> River = Root->GetObjectField(TEXT("river"));
    Cfg.RiverAnchorXCm = River->GetNumberField(TEXT("anchor_x_cm"));
    Cfg.RiverAnchorYCm = River->GetNumberField(TEXT("anchor_y_cm"));
    Cfg.RiverZCm = River->GetNumberField(TEXT("z_cm"));
    Cfg.RiverMaskHalfWidthCm = River->GetNumberField(TEXT("mask_half_width_cm"));

    const TArray<TSharedPtr<FJsonValue>>* Terms = nullptr;
    if (River->TryGetArrayField(TEXT("centerline_terms"), Terms) && Terms)
    {
        Cfg.RiverCenterlineTerms.Reset();
        for (const TSharedPtr<FJsonValue>& Value : *Terms)
        {
            const TSharedPtr<FJsonObject> Term = Value->AsObject();
            if (!Term.IsValid())
            {
                continue;
            }
            FCenterlineTerm Out;
            Out.AmpCm = Term->GetNumberField(TEXT("amp_cm"));
            Out.Freq = Term->GetNumberField(TEXT("freq"));
            Out.Phase = Term->GetNumberField(TEXT("phase"));
            Cfg.RiverCenterlineTerms.Add(Out);
        }
    }

    return Cfg;
}
} // namespace

const FConfig& Config()
{
    static const FConfig Cfg = LoadConfigFromDisk();
    return Cfg;
}

float HeightValueToCm(uint16 Encoded)
{
    const FConfig& C = Config();
    return FMath::Lerp(C.EncodedMinZCm, C.EncodedMaxZCm, static_cast<float>(Encoded) / 65535.0f);
}

float NormalizeEncodedHeightCm(float HeightCm)
{
    const FConfig& C = Config();
    return FMath::Clamp((HeightCm - C.EncodedMinZCm) / (C.EncodedMaxZCm - C.EncodedMinZCm), 0.0f, 1.0f);
}

float RiverCenterY(float X)
{
    const FConfig& C = Config();
    const float OffsetX = X - C.RiverAnchorXCm;
    float Sum = C.RiverAnchorYCm;
    for (const FCenterlineTerm& Term : C.RiverCenterlineTerms)
    {
        Sum += Term.AmpCm * FMath::Sin(OffsetX * Term.Freq + Term.Phase);
    }
    return Sum;
}
}
