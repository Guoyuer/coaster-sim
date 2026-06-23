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

FYarlungRockWallProfileConfig ParseRockWallProfile(const FString& Name, const TSharedPtr<FJsonObject>& Object, const FString& Context)
{
    FYarlungRockWallProfileConfig Config;
    Config.Name = Name;
    Config.LateralMinCm = RequiredNumberField(Object, TEXT("lateral_min_cm"), Context);
    Config.LateralMaxCm = RequiredNumberField(Object, TEXT("lateral_max_cm"), Context);
    Config.LateralStepCm = RequiredNumberField(Object, TEXT("lateral_step_cm"), Context);
    Config.TrackClearanceCm = RequiredNumberField(Object, TEXT("track_clearance_cm"), Context);
    Config.RiverClearanceCm = RequiredNumberField(Object, TEXT("river_clearance_cm"), Context);
    Config.MinHeightCm = RequiredNumberField(Object, TEXT("min_height_cm"), Context);
    Config.MaxHeightCm = RequiredNumberField(Object, TEXT("max_height_cm"), Context);
    Config.MinSlope = RequiredNumberField(Object, TEXT("min_slope"), Context);
    Config.MaxSlope = RequiredNumberField(Object, TEXT("max_slope"), Context);
    Config.ScaleMin = RequiredNumberField(Object, TEXT("scale_min"), Context);
    Config.ScaleMax = RequiredNumberField(Object, TEXT("scale_max"), Context);
    Config.HeightOffsetCm = RequiredNumberField(Object, TEXT("height_offset_cm"), Context);
    Config.EmbedDepthCm = RequiredNumberField(Object, TEXT("embed_depth_cm"), Context);
    Config.AlongJitterCm = RequiredNumberField(Object, TEXT("along_jitter_cm"), Context);
    Config.LateralJitterCm = RequiredNumberField(Object, TEXT("lateral_jitter_cm"), Context);
    Config.YawJitterDegrees = RequiredNumberField(Object, TEXT("yaw_jitter_degrees"), Context);
    return Config;
}

EYarlungSurfaceCoverAnchor ParseSurfaceCoverAnchor(const FString& Value, const FString& Context)
{
    if (Value.Equals(TEXT("track"), ESearchCase::IgnoreCase))
    {
        return EYarlungSurfaceCoverAnchor::Track;
    }
    if (Value.Equals(TEXT("river"), ESearchCase::IgnoreCase))
    {
        return EYarlungSurfaceCoverAnchor::River;
    }
    FatalAssetConfigError(FString::Printf(TEXT("%s has unknown surface cover anchor '%s'"), *Context, *Value));
    return EYarlungSurfaceCoverAnchor::Track;
}

FYarlungSurfaceCoverProfileConfig ParseSurfaceCoverProfile(const FString& Name, const TSharedPtr<FJsonObject>& Object, const FString& Context)
{
    FYarlungSurfaceCoverProfileConfig Config;
    Config.Name = Name;
    Config.Anchor = ParseSurfaceCoverAnchor(RequiredStringField(Object, TEXT("anchor"), Context), Context);
    Config.SampleStride = RequiredIntegerField(Object, TEXT("sample_stride"), Context);
    Config.LateralBandsCm = RequiredNumberArrayField(Object, TEXT("lateral_bands_cm"), Context);
    Config.Occupancy = RequiredNumberField(Object, TEXT("occupancy"), Context);
    Config.TrackClearanceCm = RequiredNumberField(Object, TEXT("track_clearance_cm"), Context);
    Config.RiverClearanceCm = RequiredNumberField(Object, TEXT("river_clearance_cm"), Context);
    Config.MinHeightCm = RequiredNumberField(Object, TEXT("min_height_cm"), Context);
    Config.MaxHeightCm = RequiredNumberField(Object, TEXT("max_height_cm"), Context);
    Config.MinSlope = RequiredNumberField(Object, TEXT("min_slope"), Context);
    Config.MaxSlope = RequiredNumberField(Object, TEXT("max_slope"), Context);
    Config.ScaleMin = RequiredNumberField(Object, TEXT("scale_min"), Context);
    Config.ScaleMax = RequiredNumberField(Object, TEXT("scale_max"), Context);
    Config.HeightOffsetCm = RequiredNumberField(Object, TEXT("height_offset_cm"), Context);
    Config.AlongJitterCm = RequiredNumberField(Object, TEXT("along_jitter_cm"), Context);
    Config.LateralJitterCm = RequiredNumberField(Object, TEXT("lateral_jitter_cm"), Context);
    Config.YawJitterDegrees = RequiredNumberField(Object, TEXT("yaw_jitter_degrees"), Context);
    Config.bAlignToSurface = RequiredBoolField(Object, TEXT("align_to_surface"), Context);
    Config.bFaceRiver = RequiredBoolField(Object, TEXT("face_river"), Context);
    return Config;
}

void ParseSurfaceCoverProfiles(
    const TSharedPtr<FJsonObject>& Scenery,
    const FString& Path,
    TMap<FString, FYarlungSurfaceCoverProfileConfig>& OutProfiles)
{
    const TSharedPtr<FJsonObject> Profiles = RequiredObject(Scenery, TEXT("surface_cover_profiles"), Path);
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Profiles->Values)
    {
        const TSharedPtr<FJsonObject> ProfileObject = Pair.Value->AsObject();
        if (!ProfileObject.IsValid())
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s scenery.surface_cover_profiles.%s is not an object"), *Path, *Pair.Key));
        }
        OutProfiles.Add(Pair.Key, ParseSurfaceCoverProfile(
            Pair.Key,
            ProfileObject,
            FString::Printf(TEXT("%s scenery.surface_cover_profiles.%s"), *Path, *Pair.Key)));
    }
}

FYarlungRockWallSegmentConfig ParseRockWallSegment(const TSharedPtr<FJsonObject>& Object, const FString& Context)
{
    FYarlungRockWallSegmentConfig Config;
    Config.Name = RequiredStringField(Object, TEXT("name"), Context);
    Config.ProfileName = RequiredStringField(Object, TEXT("profile"), Context);
    Config.ComponentName = RequiredStringField(Object, TEXT("component"), Context);
    Config.StartSampleIndex = RequiredIntegerField(Object, TEXT("start_sample"), Context);
    Config.EndSampleIndex = RequiredIntegerField(Object, TEXT("end_sample"), Context);
    Config.SampleStride = RequiredIntegerField(Object, TEXT("sample_stride"), Context);
    Config.Side = RequiredNumberField(Object, TEXT("side"), Context);
    Config.Seed = RequiredNumberField(Object, TEXT("seed"), Context);
    return Config;
}

void ParseRockWallProfiles(
    const TSharedPtr<FJsonObject>& Scenery,
    const FString& Path,
    TMap<FString, FYarlungRockWallProfileConfig>& OutProfiles)
{
    const TSharedPtr<FJsonObject> Profiles = RequiredObject(Scenery, TEXT("rock_wall_profiles"), Path);
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Profiles->Values)
    {
        const TSharedPtr<FJsonObject> ProfileObject = Pair.Value->AsObject();
        if (!ProfileObject.IsValid())
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s scenery.rock_wall_profiles.%s is not an object"), *Path, *Pair.Key));
        }
        OutProfiles.Add(Pair.Key, ParseRockWallProfile(
            Pair.Key,
            ProfileObject,
            FString::Printf(TEXT("%s scenery.rock_wall_profiles.%s"), *Path, *Pair.Key)));
    }
}

void ParseRockWallSegments(
    const TSharedPtr<FJsonObject>& Scenery,
    const FString& Path,
    TArray<FYarlungRockWallSegmentConfig>& OutSegments)
{
    const TArray<TSharedPtr<FJsonValue>>* Segments = nullptr;
    if (!Scenery->TryGetArrayField(TEXT("rock_wall_segments"), Segments) || !Segments)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing required array 'scenery.rock_wall_segments'"), *Path));
    }
    if (Segments)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Segments)
        {
            const TSharedPtr<FJsonObject> SegmentObject = Value->AsObject();
            if (!SegmentObject.IsValid())
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s has a non-object rock wall segment"), *Path));
            }
            OutSegments.Add(ParseRockWallSegment(
                SegmentObject,
                FString::Printf(TEXT("%s scenery.rock_wall_segments"), *Path)));
        }
    }
}

void ValidateRockWallProfiles(
    const TMap<FString, FYarlungRockWallProfileConfig>& Profiles,
    const FString& Path)
{
    if (Profiles.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has no scenery.rock_wall_profiles"), *Path));
    }
    for (const TPair<FString, FYarlungRockWallProfileConfig>& Pair : Profiles)
    {
        const FYarlungRockWallProfileConfig& Profile = Pair.Value;
        if (Profile.Name.IsEmpty() ||
            Profile.LateralMinCm < 0.0f || Profile.LateralMaxCm <= Profile.LateralMinCm || Profile.LateralStepCm <= 0.0f ||
            Profile.TrackClearanceCm < 0.0f || Profile.RiverClearanceCm < 0.0f ||
            Profile.MaxHeightCm < Profile.MinHeightCm ||
            Profile.MinSlope < 0.0f || Profile.MaxSlope < Profile.MinSlope ||
            Profile.ScaleMin <= 0.0f || Profile.ScaleMax < Profile.ScaleMin ||
            Profile.EmbedDepthCm < 0.0f || Profile.AlongJitterCm < 0.0f || Profile.LateralJitterCm < 0.0f)
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s has invalid rock wall profile '%s'"), *Path, *Pair.Key));
        }
    }
}

void ValidateRockWallSegments(
    const TMap<FString, FYarlungRockWallProfileConfig>& Profiles,
    const TArray<FYarlungRockWallSegmentConfig>& Segments,
    const FString& Path)
{
    if (Segments.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has no scenery.rock_wall_segments"), *Path));
    }
    int32 LowestStartSample = TNumericLimits<int32>::Max();
    int32 HighestEndSample = 0;
    bool bHasLeftSide = false;
    bool bHasRightSide = false;
    for (const FYarlungRockWallSegmentConfig& Segment : Segments)
    {
        if (Segment.Name.IsEmpty() || Segment.ProfileName.IsEmpty() || Segment.ComponentName.IsEmpty() ||
            Segment.StartSampleIndex < 0 || Segment.EndSampleIndex <= Segment.StartSampleIndex || Segment.SampleStride <= 0 ||
            (Segment.Side != -1.0f && Segment.Side != 1.0f))
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s has invalid rock wall segment '%s'"), *Path, *Segment.Name));
        }
        if (!Profiles.Contains(Segment.ProfileName))
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s rock wall segment '%s' references unknown profile '%s'"), *Path, *Segment.Name, *Segment.ProfileName));
        }
        if (Segment.ComponentName != TEXT("SlopeRockWallA") && Segment.ComponentName != TEXT("SlopeRockWallB"))
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s rock wall segment '%s' must target SlopeRockWallA or SlopeRockWallB"), *Path, *Segment.Name));
        }
        LowestStartSample = FMath::Min(LowestStartSample, Segment.StartSampleIndex);
        HighestEndSample = FMath::Max(HighestEndSample, Segment.EndSampleIndex);
        bHasLeftSide = bHasLeftSide || Segment.Side == -1.0f;
        bHasRightSide = bHasRightSide || Segment.Side == 1.0f;
    }
    if (LowestStartSample > 0 || HighestEndSample < 224 || !bHasLeftSide || !bHasRightSide)
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s scenery.rock_wall_segments must cover the full ride corridor on both sides"), *Path));
    }
}

void ValidateSurfaceCoverProfiles(
    const TMap<FString, FYarlungSurfaceCoverProfileConfig>& Profiles,
    const FString& Path)
{
    if (Profiles.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has no scenery.surface_cover_profiles"), *Path));
    }
    for (const TPair<FString, FYarlungSurfaceCoverProfileConfig>& Pair : Profiles)
    {
        const FYarlungSurfaceCoverProfileConfig& Profile = Pair.Value;
        if (Profile.Name.IsEmpty() ||
            Profile.SampleStride <= 0 || Profile.LateralBandsCm.IsEmpty() ||
            Profile.Occupancy < 0.0f || Profile.Occupancy > 1.0f ||
            Profile.TrackClearanceCm < 0.0f || Profile.RiverClearanceCm < 0.0f ||
            Profile.MaxHeightCm < Profile.MinHeightCm ||
            Profile.MinSlope < 0.0f || Profile.MaxSlope < Profile.MinSlope ||
            Profile.ScaleMin <= 0.0f || Profile.ScaleMax < Profile.ScaleMin ||
            Profile.AlongJitterCm < 0.0f || Profile.LateralJitterCm < 0.0f)
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s has invalid surface cover profile '%s'"), *Path, *Pair.Key));
        }
    }
}

EYarlungSceneryPlacement ParsePlacement(const FString& Value, const FString& Context)
{
    if (Value.Equals(TEXT("scatter"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::Scatter;
    }
    if (Value.Equals(TEXT("cliff_belt"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::CliffBelt;
    }
    if (Value.Equals(TEXT("surface_cover"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::SurfaceCover;
    }
    if (Value.Equals(TEXT("rock_wall_source"), ESearchCase::IgnoreCase))
    {
        return EYarlungSceneryPlacement::RockWallSource;
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
    if (Scenery->HasField(TEXT("canopy_belt")) || Scenery->HasField(TEXT("ground_cover_belt")))
    {
        FatalAssetConfigError(FString::Printf(
            TEXT("%s uses removed scenery canopy/ground-cover belt fields; use scenery.surface_cover_profiles instead"),
            *Path));
    }

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
            if (Component.Placement == EYarlungSceneryPlacement::SurfaceCover)
            {
                Component.SurfaceCoverProfileName = RequiredStringField(ComponentObject, TEXT("profile"), PerComponentContext);
            }
            else if (ComponentObject->HasField(TEXT("profile")))
            {
                FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' declares profile but is not surface_cover"), *Path, *Component.Name));
            }
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
                FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' uses non-scatter placement but still declares count"), *Path, *Component.Name));
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

    ParseSurfaceCoverProfiles(Scenery, Path, Config.SurfaceCoverProfiles);
    ParseRockWallProfiles(Scenery, Path, Config.RockWallProfiles);
    ParseRockWallSegments(Scenery, Path, Config.RockWallSegments);

    const TSharedPtr<FJsonObject> Water = RequiredObject(Root, TEXT("water"), Path);
    const FString WaterContext = FString::Printf(TEXT("%s water"), *Path);
    Config.Water.SurfaceMaterialPath = RequiredStringField(Water, TEXT("surface_material"), WaterContext);

    if (Config.SceneryComponents.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s has no scenery.components"), *Path));
    }
    if (Config.Water.SurfaceMaterialPath.IsEmpty())
    {
        FatalAssetConfigError(FString::Printf(TEXT("%s is missing water.surface_material"), *Path));
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
    ValidateSurfaceCoverProfiles(Config.SurfaceCoverProfiles, Path);
    for (const FYarlungSceneryComponentConfig& Component : Config.SceneryComponents)
    {
        if (Component.Placement == EYarlungSceneryPlacement::SurfaceCover &&
            !Config.SurfaceCoverProfiles.Contains(Component.SurfaceCoverProfileName))
        {
            FatalAssetConfigError(FString::Printf(TEXT("%s component '%s' references unknown surface cover profile '%s'"), *Path, *Component.Name, *Component.SurfaceCoverProfileName));
        }
    }
    ValidateRockWallProfiles(Config.RockWallProfiles, Path);
    ValidateRockWallSegments(Config.RockWallProfiles, Config.RockWallSegments, Path);

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
