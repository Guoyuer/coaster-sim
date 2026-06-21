#include "YarlungAssetConfig.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace YarlungAssets
{
namespace
{
float NumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, float DefaultValue = 0.0f)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Name, Value) ? static_cast<float>(Value) : DefaultValue;
}

int32 IntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, int32 DefaultValue = 0)
{
    double Value = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Name, Value) ? FMath::RoundToInt(Value) : DefaultValue;
}

FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
{
    FString Value;
    return Object.IsValid() && Object->TryGetStringField(Name, Value) ? Value : FString();
}

bool BoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, bool DefaultValue = false)
{
    bool Value = false;
    return Object.IsValid() && Object->TryGetBoolField(Name, Value) ? Value : DefaultValue;
}

TArray<float> NumberArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
{
    TArray<float> Values;
    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Name, JsonValues) || !JsonValues)
    {
        return Values;
    }

    for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValues)
    {
        Values.Add(static_cast<float>(JsonValue->AsNumber()));
    }
    return Values;
}

bool ColorArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FLinearColor& OutColor)
{
    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Name, JsonValues) || !JsonValues || JsonValues->Num() < 3)
    {
        return false;
    }

    OutColor = FLinearColor(
        static_cast<float>((*JsonValues)[0]->AsNumber()),
        static_cast<float>((*JsonValues)[1]->AsNumber()),
        static_cast<float>((*JsonValues)[2]->AsNumber()),
        JsonValues->Num() > 3 ? static_cast<float>((*JsonValues)[3]->AsNumber()) : 1.0f);
    return true;
}

FYarlungScatterKindConfig ParseScatterKind(const TSharedPtr<FJsonObject>& Object)
{
    FYarlungScatterKindConfig Config;
    Config.SideBias = NumberField(Object, TEXT("side_bias"));
    Config.DistancePower = NumberField(Object, TEXT("distance_power"), 1.0f);
    Config.MinLateralCm = NumberField(Object, TEXT("min_lateral_cm"));
    Config.MaxLateralCm = NumberField(Object, TEXT("max_lateral_cm"));
    Config.RiverClearanceCm = NumberField(Object, TEXT("river_clearance_cm"));
    Config.MinHeightCm = NumberField(Object, TEXT("min_height_cm"));
    Config.MaxHeightCm = NumberField(Object, TEXT("max_height_cm"));
    Config.MinSlope = NumberField(Object, TEXT("min_slope"));
    Config.MaxSlope = NumberField(Object, TEXT("max_slope"), 1.0f);
    Config.ScaleMin = NumberField(Object, TEXT("scale_min"), 1.0f);
    Config.ScaleMax = NumberField(Object, TEXT("scale_max"), 1.0f);
    Config.HeightOffsetCm = NumberField(Object, TEXT("height_offset_cm"));
    Config.bAlignToSurface = BoolField(Object, TEXT("align_to_surface"));
    return Config;
}

FYarlungAssetConfig LoadConfigFromDisk()
{
    FYarlungAssetConfig Config;
    const FString LocalPath = FPaths::ProjectDir() / TEXT("Config/yarlung-assets.local.json");
    const FString DefaultPath = FPaths::ProjectDir() / TEXT("Config/yarlung-assets.json");
    const FString Path = FPaths::FileExists(LocalPath) ? LocalPath : DefaultPath;

    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("YarlungAssets: required config missing: %s"), *DefaultPath);
        return Config;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("YarlungAssets: failed to parse %s"), *Path);
        return Config;
    }

    const TSharedPtr<FJsonObject> Scenery = Root->GetObjectField(TEXT("scenery"));
    const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
    if (Scenery->TryGetArrayField(TEXT("components"), Components) && Components)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Components)
        {
            const TSharedPtr<FJsonObject> ComponentObject = Value->AsObject();
            if (!ComponentObject.IsValid())
            {
                continue;
            }

            FYarlungSceneryComponentConfig Component;
            Component.Name = StringField(ComponentObject, TEXT("name"));
            Component.MeshPath = StringField(ComponentObject, TEXT("mesh"));
            Component.Kind = StringField(ComponentObject, TEXT("kind"));
            Component.Count = IntegerField(ComponentObject, TEXT("count"));
            Component.Seed = NumberField(ComponentObject, TEXT("seed"));
            Component.bUseCanopyBelt = ComponentObject->HasField(TEXT("canopy_belt_seed"));
            Component.CanopyBeltSeed = NumberField(ComponentObject, TEXT("canopy_belt_seed"));
            Component.bUseTint = ColorArrayField(ComponentObject, TEXT("tint"), Component.Tint);
            Config.SceneryComponents.Add(Component);
        }
    }

    const TSharedPtr<FJsonObject> Kinds = Scenery->GetObjectField(TEXT("kinds"));
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Kinds->Values)
    {
        Config.ScatterKinds.Add(Pair.Key, ParseScatterKind(Pair.Value->AsObject()));
    }

    const TSharedPtr<FJsonObject> CanopyBelt = Scenery->GetObjectField(TEXT("canopy_belt"));
    Config.CanopyBelt.SampleStride = IntegerField(CanopyBelt, TEXT("sample_stride"), 2);
    Config.CanopyBelt.LateralBandsCm = NumberArrayField(CanopyBelt, TEXT("lateral_bands_cm"));
    Config.CanopyBelt.NearBandCount = IntegerField(CanopyBelt, TEXT("near_band_count"));
    Config.CanopyBelt.NearOccupancy = NumberField(CanopyBelt, TEXT("near_occupancy"));
    Config.CanopyBelt.FarOccupancy = NumberField(CanopyBelt, TEXT("far_occupancy"));
    Config.CanopyBelt.RiverClearanceCm = NumberField(CanopyBelt, TEXT("river_clearance_cm"));
    Config.CanopyBelt.MinHeightCm = NumberField(CanopyBelt, TEXT("min_height_cm"));
    Config.CanopyBelt.MaxHeightCm = NumberField(CanopyBelt, TEXT("max_height_cm"));
    Config.CanopyBelt.MaxSlope = NumberField(CanopyBelt, TEXT("max_slope"), 1.0f);
    Config.CanopyBelt.ScaleMin = NumberField(CanopyBelt, TEXT("scale_min"), 1.0f);
    Config.CanopyBelt.ScaleMax = NumberField(CanopyBelt, TEXT("scale_max"), 1.0f);
    Config.CanopyBelt.HeightOffsetCm = NumberField(CanopyBelt, TEXT("height_offset_cm"));

    const TSharedPtr<FJsonObject> Water = Root->GetObjectField(TEXT("water"));
    Config.Water.RiverMaterialPath = StringField(Water, TEXT("river_material"));
    Config.Water.FallbackRiverMaterialPath = StringField(Water, TEXT("fallback_river_material"));
    Config.Water.SurfaceMaterialPath = StringField(Water, TEXT("surface_material"));
    Config.Water.ZoneRenderTargetResolution = IntegerField(Water, TEXT("zone_render_target_resolution"), 1024);
    Config.Water.ZoneExtentScale = NumberField(Water, TEXT("zone_extent_scale"), 0.55f);
    Config.Water.DefaultDepthCm = NumberField(Water, TEXT("default_depth_cm"));
    Config.Water.BaseVelocityCmPerSec = NumberField(Water, TEXT("base_velocity_cm_per_sec"));
    Config.Water.FlowVelocityJitterCmPerSec = NumberField(Water, TEXT("flow_velocity_jitter_cm_per_sec"));
    Config.Water.WidthScale = NumberField(Water, TEXT("width_scale"), 1.0f);
    Config.Water.MinWidthCm = NumberField(Water, TEXT("min_width_cm"));
    Config.Water.MaxWidthCm = NumberField(Water, TEXT("max_width_cm"));
    Config.Water.ShapeDilation = NumberField(Water, TEXT("shape_dilation"));
    Config.Water.AudioIntensity = NumberField(Water, TEXT("audio_intensity"));

    UE_LOG(LogTemp, Display, TEXT("YarlungAssets: loaded %d scenery components, %d scatter kinds from %s"),
        Config.SceneryComponents.Num(),
        Config.ScatterKinds.Num(),
        *Path);
    return Config;
}
}

const FYarlungAssetConfig& Config()
{
    static const FYarlungAssetConfig LoadedConfig = LoadConfigFromDisk();
    return LoadedConfig;
}
}
