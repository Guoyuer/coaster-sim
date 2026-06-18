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

namespace
{
float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

float DistanceToSegment2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B, float& OutT)
{
    const FVector2D AB = B - A;
    const float Denom = FMath::Max(AB.SizeSquared(), 1.0f);
    OutT = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / Denom, 0.0f, 1.0f);
    return (Point - (A + AB * OutT)).Size();
}

float ApplyCoasterClearanceCut(float X, float Y, float Height)
{
    const TArray<FVector> TrackPoints = {
        FVector(0.0f, 0.0f, 1320.0f),
        FVector(1800.0f, 0.0f, 1460.0f),
        FVector(4200.0f, 280.0f, 2700.0f),
        FVector(6800.0f, 620.0f, 4450.0f),
        FVector(9300.0f, 260.0f, 2150.0f),
        FVector(10400.0f, -1800.0f, 1420.0f),
        FVector(7700.0f, -3650.0f, 2400.0f),
        FVector(4100.0f, -3900.0f, 1800.0f),
        FVector(600.0f, -2500.0f, 2750.0f),
        FVector(-1800.0f, 300.0f, 1680.0f),
        FVector(-850.0f, 1500.0f, 1360.0f)
    };

    const FVector2D Point(X, Y);
    float BestDistance = TNumericLimits<float>::Max();
    float ClearanceTarget = Height;
    for (int32 Index = 0; Index < TrackPoints.Num(); ++Index)
    {
        const FVector& A3 = TrackPoints[Index];
        const FVector& B3 = TrackPoints[(Index + 1) % TrackPoints.Num()];
        float T = 0.0f;
        const float Distance = DistanceToSegment2D(Point, FVector2D(A3.X, A3.Y), FVector2D(B3.X, B3.Y), T);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            ClearanceTarget = FMath::Lerp(A3.Z, B3.Z, T) - 1580.0f;
        }
    }

    if (BestDistance > 3300.0f)
    {
        return Height;
    }

    const float BlendToOriginal = Smooth01((BestDistance - 1450.0f) / 1850.0f);
    const float CutHeight = FMath::Min(Height, ClearanceTarget);
    return FMath::Lerp(CutHeight, Height, BlendToOriginal);
}
}
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
            const float CutHeightCm = ApplyCoasterClearanceCut(X, Y, HeightCm);
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

    UMaterialInterface* BaseLandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));
    if (!BaseLandscapeMaterial)
    {
        BaseLandscapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
    }
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
        LandscapeMaterial->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), FLinearColor(0.24f, 0.25f, 0.21f, 1.0f));
        LandscapeMaterial->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BaseColor")), FLinearColor(0.24f, 0.25f, 0.21f, 1.0f));
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
