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
void FatalTerrainConfigError(const FString& Message)
{
    UE_LOG(LogTemp, Fatal, TEXT("YarlungTerrain config setup error: %s"), *Message);
}

TSharedPtr<FJsonObject> RequiredObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Path)
{
    const TSharedPtr<FJsonObject>* Child = nullptr;
    if (!Object.IsValid() || !Object->TryGetObjectField(Name, Child) || !Child || !Child->IsValid())
    {
        FatalTerrainConfigError(FString::Printf(TEXT("%s is missing required object '%s'"), *Path, Name));
    }
    return *Child;
}

FConfig LoadConfigFromDisk()
{
    FConfig Cfg; // zero-initialized; the JSON is the only source of real values

    const FString Path = FPaths::ProjectDir() / TEXT("Config/yarlung-terrain.json");
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        FatalTerrainConfigError(FString::Printf(TEXT("required config is missing: %s"), *Path));
        return Cfg;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        FatalTerrainConfigError(FString::Printf(TEXT("failed to parse %s"), *Path));
        return Cfg;
    }

    Cfg.GridSize = Root->GetIntegerField(TEXT("grid_size"));

    const TSharedPtr<FJsonObject> Bounds = RequiredObject(Root, TEXT("world_bounds_cm"), Path);
    Cfg.MinXCm = Bounds->GetNumberField(TEXT("min_x"));
    Cfg.MaxXCm = Bounds->GetNumberField(TEXT("max_x"));
    Cfg.MinYCm = Bounds->GetNumberField(TEXT("min_y"));
    Cfg.MaxYCm = Bounds->GetNumberField(TEXT("max_y"));

    const TSharedPtr<FJsonObject> Height = RequiredObject(Root, TEXT("encoded_height_cm"), Path);
    Cfg.EncodedMinZCm = Height->GetNumberField(TEXT("min"));
    Cfg.EncodedMaxZCm = Height->GetNumberField(TEXT("max"));

    const TSharedPtr<FJsonObject> River = RequiredObject(Root, TEXT("river"), Path);
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

    if (Cfg.GridSize <= 0)
    {
        FatalTerrainConfigError(FString::Printf(TEXT("%s has invalid grid_size=%d"), *Path, Cfg.GridSize));
    }
    if (Cfg.MinXCm >= Cfg.MaxXCm || Cfg.MinYCm >= Cfg.MaxYCm)
    {
        FatalTerrainConfigError(FString::Printf(TEXT("%s has invalid world_bounds_cm"), *Path));
    }
    if (Cfg.EncodedMinZCm >= Cfg.EncodedMaxZCm)
    {
        FatalTerrainConfigError(FString::Printf(TEXT("%s has invalid encoded_height_cm range"), *Path));
    }
    if (Cfg.RiverCenterlineTerms.IsEmpty())
    {
        FatalTerrainConfigError(FString::Printf(TEXT("%s has no river.centerline_terms"), *Path));
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
