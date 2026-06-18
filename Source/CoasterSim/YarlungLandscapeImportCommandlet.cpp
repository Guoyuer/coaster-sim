#include "YarlungLandscapeImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
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
    const FString LandscapeMaterialPackagePath = TEXT("/Game/Generated/YarlungLandscape/MI_YarlungLandscape_Ground");

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

    UMaterialInterface* BaseLandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    UMaterialInstanceConstant* LandscapeMaterial = LoadObject<UMaterialInstanceConstant>(nullptr, *LandscapeMaterialPackagePath);
    if (!LandscapeMaterial && BaseLandscapeMaterial)
    {
        UPackage* MaterialPackage = CreatePackage(*LandscapeMaterialPackagePath);
        LandscapeMaterial = NewObject<UMaterialInstanceConstant>(
            MaterialPackage,
            FName(TEXT("MI_YarlungLandscape_Ground")),
            RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(LandscapeMaterial);
    }

    if (LandscapeMaterial && BaseLandscapeMaterial)
    {
        LandscapeMaterial->SetParentEditorOnly(BaseLandscapeMaterial);
        LandscapeMaterial->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), FLinearColor(0.50f, 0.43f, 0.30f, 1.0f));
        LandscapeMaterial->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BaseColor")), FLinearColor(0.50f, 0.43f, 0.30f, 1.0f));
        LandscapeMaterial->PostEditChange();
        LandscapeMaterial->MarkPackageDirty();

        UPackage* MaterialPackage = LandscapeMaterial->GetPackage();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;
        const FString MaterialFilename = FPackageName::LongPackageNameToFilename(LandscapeMaterialPackagePath, FPackageName::GetAssetPackageExtension());
        UPackage::SavePackage(MaterialPackage, LandscapeMaterial, *MaterialFilename, SaveArgs);

        Landscape->LandscapeMaterial = LandscapeMaterial;
    }
    else if (BaseLandscapeMaterial)
    {
        Landscape->LandscapeMaterial = BaseLandscapeMaterial;
    }

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
