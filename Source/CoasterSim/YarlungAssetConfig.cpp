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
void FatalAssetConfigError(const FString& Message)
{
    UE_LOG(LogTemp, Fatal, TEXT("YarlungAssets config setup error: %s"), *Message);
}

TSharedPtr<FJsonObject> RequiredObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Path)
{
    const TSharedPtr<FJsonObject>* Child = nullptr;
    if (!Object.IsValid() || !Object->TryGetObjectField(Name, Child) || !Child || !Child->IsValid())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required object '%s'"), *Path, Name));
    }
    return *Child;
}

float RequiredNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Context)
{
    double Value = 0.0;
    if (!Object.IsValid() || !Object->TryGetNumberField(Name, Value))
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required number '%s'"), *Context, Name));
    }
    return static_cast<float>(Value);
}

int32 RequiredIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Context)
{
    double Value = 0.0;
    if (!Object.IsValid() || !Object->TryGetNumberField(Name, Value))
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required integer '%s'"), *Context, Name));
    }
    return FMath::RoundToInt(Value);
}

FString OptionalStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
{
    FString Value;
    return Object.IsValid() && Object->TryGetStringField(Name, Value) ? Value : FString();
}

FString RequiredStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Context)
{
    FString Value;
    if (!Object.IsValid() || !Object->TryGetStringField(Name, Value) || Value.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required string '%s'"), *Context, Name));
    }
    return Value;
}

bool RequiredBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Context)
{
    bool Value = false;
    if (!Object.IsValid() || !Object->TryGetBoolField(Name, Value))
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required bool '%s'"), *Context, Name));
    }
    return Value;
}

TArray<float> RequiredNumberArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const FString& Context)
{
    TArray<float> Values;
    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Name, JsonValues) || !JsonValues)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required number array '%s'"), *Context, Name));
    }

    for (const TSharedPtr<FJsonValue>& JsonValue : *JsonValues)
    {
        if (!JsonValue.IsValid() || JsonValue->Type != EJson::Number)
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s.%s has a non-number value"), *Context, Name));
        }
        Values.Add(static_cast<float>(JsonValue->AsNumber()));
    }
    return Values;
}

bool OptionalColorArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FLinearColor& OutColor, const FString& Context)
{
    if (!Object.IsValid() || !Object->HasField(Name))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
    if (!Object->TryGetArrayField(Name, JsonValues) || !JsonValues || JsonValues->Num() < 3)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s.%s must be an RGB or RGBA number array"), *Context, Name));
    }
    for (int32 Index = 0; Index < JsonValues->Num(); ++Index)
    {
        if (!(*JsonValues)[Index].IsValid() || (*JsonValues)[Index]->Type != EJson::Number)
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s.%s has a non-number value"), *Context, Name));
        }
    }

    OutColor = FLinearColor(
        static_cast<float>((*JsonValues)[0]->AsNumber()),
        static_cast<float>((*JsonValues)[1]->AsNumber()),
        static_cast<float>((*JsonValues)[2]->AsNumber()),
        JsonValues->Num() > 3 ? static_cast<float>((*JsonValues)[3]->AsNumber()) : 1.0f);
    return true;
}

FYarlungScatterKindConfig ParseScatterKind(const TSharedPtr<FJsonObject>& Object, const FString& Context)
{
    FYarlungScatterKindConfig Config;
    Config.SideBias = RequiredNumberField(Object, TEXT("side_bias"), Context);
    Config.DistancePower = RequiredNumberField(Object, TEXT("distance_power"), Context);
    Config.MinLateralCm = RequiredNumberField(Object, TEXT("min_lateral_cm"), Context);
    Config.MaxLateralCm = RequiredNumberField(Object, TEXT("max_lateral_cm"), Context);
    Config.RiverClearanceCm = RequiredNumberField(Object, TEXT("river_clearance_cm"), Context);
    Config.MinHeightCm = RequiredNumberField(Object, TEXT("min_height_cm"), Context);
    Config.MaxHeightCm = RequiredNumberField(Object, TEXT("max_height_cm"), Context);
    Config.MinSlope = RequiredNumberField(Object, TEXT("min_slope"), Context);
    Config.MaxSlope = RequiredNumberField(Object, TEXT("max_slope"), Context);
    Config.ScaleMin = RequiredNumberField(Object, TEXT("scale_min"), Context);
    Config.ScaleMax = RequiredNumberField(Object, TEXT("scale_max"), Context);
    Config.HeightOffsetCm = RequiredNumberField(Object, TEXT("height_offset_cm"), Context);
    Config.bAlignToSurface = RequiredBoolField(Object, TEXT("align_to_surface"), Context);
    return Config;
}

EYarlungSceneryPlacement ParsePlacement(const FString& Value, const FString& Context)
{
    if (Value.Equals(TEXT("scatter"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::Scatter;
    }
    if (Value.Equals(TEXT("canopy_belt"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::CanopyBelt;
    }
    if (Value.Equals(TEXT("cliff_belt"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::CliffBelt;
    }

    FatalAssetConfigError(FString::Printf(TEXT("%s has unknown placement '%s'"), *Context, *Value));
    return EYarlungSceneryPlacement::Scatter;
}

FYarlungAssetConfig LoadConfigFromDisk()
{
    FYarlungAssetConfig Config;
    const FString Path = FPaths::ProjectDir() / TEXT("Config/yarlung-assets.json");

    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        FatalAssetConfigError(FString::Printf(TEXT("required config is missing: %s"), *Path));
        return Config;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        FatalAssetConfigError(FString::Printf(TEXT("failed to parse %s"), *Path));
        return Config;
    }

    const TSharedPtr<FJsonObject> Scenery = RequiredObject(Root, TEXT("scenery"), Path);
    const TArray<TSharedPtr<FJsonValue>>* Components = nullptr;
    if (!Scenery->TryGetArrayField(TEXT("components"), Components) || !Components)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required array 'scenery.components'"), *Path));
    }
    if (Components)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Components)
        {
            const TSharedPtr<FJsonObject> ComponentObject = Value->AsObject();
            if (!ComponentObject.IsValid())
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s has a non-object scenery component"), *Path));
            }

            FYarlungSceneryComponentConfig Component;
            const FString ComponentContext = FString::Printf(TEXT("%s scenery component"), *Path);
            Component.Name = RequiredStringField(ComponentObject, TEXT("name"), ComponentContext);
            Component.MeshPath = OptionalStringField(ComponentObject, TEXT("mesh"));
            Component.Kind = RequiredStringField(ComponentObject, TEXT("kind"), ComponentContext);
            const FString PerComponentContext = FString::Printf(TEXT("%s component '%s'"), *Path, *Component.Name);
            Component.Placement = ParsePlacement(RequiredStringField(ComponentObject, TEXT("placement"), PerComponentContext), PerComponentContext);
            Component.Count = Component.Placement == EYarlungSceneryPlacement::Scatter
                ? RequiredIntegerField(ComponentObject, TEXT("count"), PerComponentContext)
                : 0;
            Component.Seed = RequiredNumberField(ComponentObject, TEXT("seed"), PerComponentContext);
            Component.bUseTint = OptionalColorArrayField(ComponentObject, TEXT("tint"), Component.Tint, PerComponentContext);
            if (Component.Name.IsEmpty())
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s has a scenery component without a name"), *Path));
            }
            if (Component.Kind.IsEmpty())
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' has no kind"), *Path, *Component.Name));
            }
            if (Component.Count < 0)
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' has negative count"), *Path, *Component.Name));
            }
            if (Component.Placement != EYarlungSceneryPlacement::Scatter && ComponentObject->HasField(TEXT("count")))
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' uses belt placement but still declares count"), *Path, *Component.Name));
            }
            if (Component.MeshPath.IsEmpty())
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' is enabled but has no mesh path"), *Path, *Component.Name));
            }
            Config.SceneryComponents.Add(Component);
        }
    }

    const TSharedPtr<FJsonObject> Kinds = RequiredObject(Scenery, TEXT("kinds"), Path);
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Kinds->Values)
    {
        const TSharedPtr<FJsonObject> KindObject = Pair.Value->AsObject();
        if (!KindObject.IsValid())
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s scenery kind '%s' is not an object"), *Path, *Pair.Key));
        }
        Config.ScatterKinds.Add(Pair.Key, ParseScatterKind(KindObject, FString::Printf(TEXT("%s scenery.kinds.%s"), *Path, *Pair.Key)));
    }
    if (Config.ScatterKinds.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has no scenery.kinds"), *Path));
    }
    for (const FYarlungSceneryComponentConfig& Component : Config.SceneryComponents)
    {
        if (!Config.ScatterKinds.Contains(Component.Kind))
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' references unknown kind '%s'"), *Path, *Component.Name, *Component.Kind));
        }
    }

    const TSharedPtr<FJsonObject> CanopyBelt = RequiredObject(Scenery, TEXT("canopy_belt"), Path);
    const FString CanopyBeltContext = FString::Printf(TEXT("%s scenery.canopy_belt"), *Path);
    Config.CanopyBelt.SampleStride = RequiredIntegerField(CanopyBelt, TEXT("sample_stride"), CanopyBeltContext);
    Config.CanopyBelt.LateralBandsCm = RequiredNumberArrayField(CanopyBelt, TEXT("lateral_bands_cm"), CanopyBeltContext);
    Config.CanopyBelt.NearBandCount = RequiredIntegerField(CanopyBelt, TEXT("near_band_count"), CanopyBeltContext);
    Config.CanopyBelt.NearOccupancy = RequiredNumberField(CanopyBelt, TEXT("near_occupancy"), CanopyBeltContext);
    Config.CanopyBelt.FarOccupancy = RequiredNumberField(CanopyBelt, TEXT("far_occupancy"), CanopyBeltContext);
    Config.CanopyBelt.TrackClearanceCm = RequiredNumberField(CanopyBelt, TEXT("track_clearance_cm"), CanopyBeltContext);
    Config.CanopyBelt.RiverClearanceCm = RequiredNumberField(CanopyBelt, TEXT("river_clearance_cm"), CanopyBeltContext);
    Config.CanopyBelt.MinHeightCm = RequiredNumberField(CanopyBelt, TEXT("min_height_cm"), CanopyBeltContext);
    Config.CanopyBelt.MaxHeightCm = RequiredNumberField(CanopyBelt, TEXT("max_height_cm"), CanopyBeltContext);
    Config.CanopyBelt.MaxSlope = RequiredNumberField(CanopyBelt, TEXT("max_slope"), CanopyBeltContext);
    Config.CanopyBelt.ScaleMin = RequiredNumberField(CanopyBelt, TEXT("scale_min"), CanopyBeltContext);
    Config.CanopyBelt.ScaleMax = RequiredNumberField(CanopyBelt, TEXT("scale_max"), CanopyBeltContext);
    Config.CanopyBelt.HeightOffsetCm = RequiredNumberField(CanopyBelt, TEXT("height_offset_cm"), CanopyBeltContext);

    const TSharedPtr<FJsonObject> CliffBelt = RequiredObject(Scenery, TEXT("cliff_belt"), Path);
    const FString CliffBeltContext = FString::Printf(TEXT("%s scenery.cliff_belt"), *Path);
    Config.CliffBelt.SampleStride = RequiredIntegerField(CliffBelt, TEXT("sample_stride"), CliffBeltContext);
    Config.CliffBelt.LateralBandsCm = RequiredNumberArrayField(CliffBelt, TEXT("lateral_bands_cm"), CliffBeltContext);
    Config.CliffBelt.Occupancy = RequiredNumberField(CliffBelt, TEXT("occupancy"), CliffBeltContext);
    Config.CliffBelt.TrackClearanceCm = RequiredNumberField(CliffBelt, TEXT("track_clearance_cm"), CliffBeltContext);
    Config.CliffBelt.RiverClearanceCm = RequiredNumberField(CliffBelt, TEXT("river_clearance_cm"), CliffBeltContext);
    Config.CliffBelt.MinHeightCm = RequiredNumberField(CliffBelt, TEXT("min_height_cm"), CliffBeltContext);
    Config.CliffBelt.MaxHeightCm = RequiredNumberField(CliffBelt, TEXT("max_height_cm"), CliffBeltContext);
    Config.CliffBelt.MinSlope = RequiredNumberField(CliffBelt, TEXT("min_slope"), CliffBeltContext);
    Config.CliffBelt.MaxSlope = RequiredNumberField(CliffBelt, TEXT("max_slope"), CliffBeltContext);
    Config.CliffBelt.ScaleMin = RequiredNumberField(CliffBelt, TEXT("scale_min"), CliffBeltContext);
    Config.CliffBelt.ScaleMax = RequiredNumberField(CliffBelt, TEXT("scale_max"), CliffBeltContext);
    Config.CliffBelt.HeightOffsetCm = RequiredNumberField(CliffBelt, TEXT("height_offset_cm"), CliffBeltContext);
    Config.CliffBelt.AlongJitterCm = RequiredNumberField(CliffBelt, TEXT("along_jitter_cm"), CliffBeltContext);
    Config.CliffBelt.LateralJitterCm = RequiredNumberField(CliffBelt, TEXT("lateral_jitter_cm"), CliffBeltContext);
    Config.CliffBelt.YawJitterDegrees = RequiredNumberField(CliffBelt, TEXT("yaw_jitter_degrees"), CliffBeltContext);
    Config.CliffBelt.RiverWallSampleStride = RequiredIntegerField(CliffBelt, TEXT("river_wall_sample_stride"), CliffBeltContext);
    Config.CliffBelt.RiverWallLateralBandsCm = RequiredNumberArrayField(CliffBelt, TEXT("river_wall_lateral_bands_cm"), CliffBeltContext);
    Config.CliffBelt.RiverWallOccupancy = RequiredNumberField(CliffBelt, TEXT("river_wall_occupancy"), CliffBeltContext);
    Config.CliffBelt.RiverWallScaleMin = RequiredNumberField(CliffBelt, TEXT("river_wall_scale_min"), CliffBeltContext);
    Config.CliffBelt.RiverWallScaleMax = RequiredNumberField(CliffBelt, TEXT("river_wall_scale_max"), CliffBeltContext);
    Config.CliffBelt.RiverWallHeightOffsetCm = RequiredNumberField(CliffBelt, TEXT("river_wall_height_offset_cm"), CliffBeltContext);
    Config.CliffBelt.RiverWallAlongJitterCm = RequiredNumberField(CliffBelt, TEXT("river_wall_along_jitter_cm"), CliffBeltContext);
    Config.CliffBelt.RiverWallLateralJitterCm = RequiredNumberField(CliffBelt, TEXT("river_wall_lateral_jitter_cm"), CliffBeltContext);
    Config.CliffBelt.RiverWallYawJitterDegrees = RequiredNumberField(CliffBelt, TEXT("river_wall_yaw_jitter_degrees"), CliffBeltContext);

    const TSharedPtr<FJsonObject> Water = RequiredObject(Root, TEXT("water"), Path);
    const FString WaterContext = FString::Printf(TEXT("%s water"), *Path);
    Config.Water.SurfaceMaterialPath = RequiredStringField(Water, TEXT("surface_material"), WaterContext);
    Config.Water.WidthScale = RequiredNumberField(Water, TEXT("width_scale"), WaterContext);
    Config.Water.MinWidthCm = RequiredNumberField(Water, TEXT("min_width_cm"), WaterContext);
    Config.Water.MaxWidthCm = RequiredNumberField(Water, TEXT("max_width_cm"), WaterContext);

    if (Config.SceneryComponents.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has no scenery.components"), *Path));
    }
    if (Config.Water.SurfaceMaterialPath.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing water.surface_material"), *Path));
    }
    if (Config.Water.MinWidthCm <= 0.0f || Config.Water.MaxWidthCm < Config.Water.MinWidthCm)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has invalid water width bounds"), *Path));
    }
    if (Config.CanopyBelt.SampleStride <= 0 || Config.CanopyBelt.LateralBandsCm.IsEmpty() ||
        Config.CanopyBelt.NearOccupancy < 0.0f || Config.CanopyBelt.NearOccupancy > 1.0f ||
        Config.CanopyBelt.FarOccupancy < 0.0f || Config.CanopyBelt.FarOccupancy > 1.0f ||
        Config.CanopyBelt.TrackClearanceCm < 0.0f ||
        Config.CanopyBelt.ScaleMin <= 0.0f || Config.CanopyBelt.ScaleMax < Config.CanopyBelt.ScaleMin)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has invalid scenery.canopy_belt settings"), *Path));
    }
    if (Config.CliffBelt.SampleStride <= 0 || Config.CliffBelt.LateralBandsCm.IsEmpty() ||
        Config.CliffBelt.Occupancy < 0.0f || Config.CliffBelt.Occupancy > 1.0f ||
        Config.CliffBelt.TrackClearanceCm < 0.0f ||
        Config.CliffBelt.ScaleMin <= 0.0f || Config.CliffBelt.ScaleMax < Config.CliffBelt.ScaleMin ||
        Config.CliffBelt.RiverWallSampleStride <= 0 || Config.CliffBelt.RiverWallLateralBandsCm.IsEmpty() ||
        Config.CliffBelt.RiverWallOccupancy < 0.0f || Config.CliffBelt.RiverWallOccupancy > 1.0f ||
        Config.CliffBelt.RiverWallScaleMin <= 0.0f || Config.CliffBelt.RiverWallScaleMax < Config.CliffBelt.RiverWallScaleMin)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has invalid scenery.cliff_belt settings"), *Path));
    }

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
