#include "YarlungLandscapeImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungCoasterProfile.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"

#endif

UYarlungLandscapeImportCommandlet::UYarlungLandscapeImportCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UYarlungLandscapeImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
    constexpr int32 HeightmapSize = 505;
    constexpr float MinX = -4200.0f;
    constexpr float MaxX = 12200.0f;
    constexpr float MinY = -10500.0f;
    constexpr float MaxY = 8200.0f;
    constexpr float EncodedMinZ = -360.0f;
    constexpr float EncodedMaxZ = 3900.0f;
    constexpr int32 SectionsPerComponent = 1;
    constexpr int32 SectionSizeQuads = 63;

    const FString HeightmapPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectContentDir(),
        TEXT("Generated/YarlungLandscape/YarlungTsangpo_505.r16")));
    const FString MapPackagePath = TEXT("/Game/Generated/YarlungLandscape/YarlungLandscape_Level");

    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *HeightmapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read landscape heightmap: %s"), *HeightmapPath);
        return 1;
    }

    const int32 ExpectedByteCount = HeightmapSize * HeightmapSize * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Heightmap has %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return 1;
    }

    TArray<uint16> HeightData;
    HeightData.SetNumUninitialized(HeightmapSize * HeightmapSize);
    FMemory::Memcpy(HeightData.GetData(), RawBytes.GetData(), RawBytes.Num());

    for (int32 YIndex = 0; YIndex < HeightmapSize; ++YIndex)
    {
        const float V = static_cast<float>(YIndex) / static_cast<float>(HeightmapSize - 1);
        const float Y = FMath::Lerp(MinY, MaxY, V);
        for (int32 XIndex = 0; XIndex < HeightmapSize; ++XIndex)
        {
            const float U = static_cast<float>(XIndex) / static_cast<float>(HeightmapSize - 1);
            const float X = FMath::Lerp(MinX, MaxX, U);
            const int32 DataIndex = YIndex * HeightmapSize + XIndex;
            const float EncodedT = static_cast<float>(HeightData[DataIndex]) / 65535.0f;
            const float HeightCm = FMath::Lerp(EncodedMinZ, EncodedMaxZ, EncodedT);
            const float CutHeightCm = YarlungCoaster::ApplyTrackClearanceCut(X, Y, HeightCm);
            const float CutT = FMath::Clamp((CutHeightCm - EncodedMinZ) / (EncodedMaxZ - EncodedMinZ), 0.0f, 1.0f);
            HeightData[DataIndex] = static_cast<uint16>(FMath::RoundToInt(CutT * 65535.0f));
        }
    }

    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to create blank map"));
        return 1;
    }

    const float XYScaleX = (MaxX - MinX) / static_cast<float>(HeightmapSize - 1);
    const float XYScaleY = (MaxY - MinY) / static_cast<float>(HeightmapSize - 1);
    const float ZScale = (EncodedMaxZ - EncodedMinZ) / 512.0f;
    const float ZMidpoint = (EncodedMaxZ + EncodedMinZ) * 0.5f;

    ALandscape* Landscape = World->SpawnActor<ALandscape>(
        FVector(MinX, MinY, ZMidpoint),
        FRotator::ZeroRotator);
    if (!Landscape)
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to spawn ALandscape"));
        return 1;
    }

    Landscape->SetActorLabel(TEXT("YarlungTsangpoLandscape"));
    Landscape->SetActorScale3D(FVector(XYScaleX, XYScaleY, ZScale));
    Landscape->StaticLightingLOD = 0;
    Landscape->bCastStaticShadow = true;
    Landscape->bCastDynamicShadow = true;

    TMap<FGuid, TArray<uint16>> HeightDataPerLayer;
    HeightDataPerLayer.Add(FGuid(), MoveTemp(HeightData));

    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayer;
    MaterialLayerDataPerLayer.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

    Landscape->Import(
        FGuid::NewGuid(),
        0,
        0,
        HeightmapSize - 1,
        HeightmapSize - 1,
        SectionsPerComponent,
        SectionSizeQuads,
        HeightDataPerLayer,
        *HeightmapPath,
        MaterialLayerDataPerLayer,
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>());
    Landscape->RegisterAllComponents();

    UMaterialInterface* LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_YarlungLandscapeGround.M_YarlungLandscapeGround"));
    if (!LandscapeMaterial)
    {
        LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));
    }
    if (!LandscapeMaterial)
    {
        LandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
    }
    if (LandscapeMaterial)
    {
        Landscape->LandscapeMaterial = LandscapeMaterial;
        Landscape->UpdateAllComponentMaterialInstances(true);
        UE_LOG(LogTemp, Display, TEXT("Assigned Yarlung landscape material: %s"), *LandscapeMaterial->GetPathName());
    }

    Landscape->PostEditChange();

    ACoasterRideActor* Ride = World->SpawnActor<ACoasterRideActor>(
        ACoasterRideActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (Ride)
    {
        Ride->SetActorLabel(TEXT("YarlungCoasterRide"));
    }

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, MapPackagePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to save imported landscape map: %s"), *MapPackagePath);
        return 1;
    }

    FAssetRegistryModule::AssetCreated(World);
    UE_LOG(LogTemp, Display, TEXT("Imported Yarlung landscape map: %s"), *MapPackagePath);
    return 0;
#else
    UE_LOG(LogTemp, Error, TEXT("YarlungLandscapeImport commandlet requires an editor build."));
    return 1;
#endif
}
