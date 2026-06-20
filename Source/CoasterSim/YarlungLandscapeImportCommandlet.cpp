#include "YarlungLandscapeImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "CoasterRideActor.h"
#include "YarlungRiverActor.h"
#include "YarlungSceneryActor.h"
#include "YarlungMeshTerrainActor.h"
#include "YarlungCoasterProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "Misc/FileHelper.h"
#include "StaticMeshAttributes.h"

#endif

#if WITH_EDITOR
namespace
{
constexpr int32 YarlungHeightmapSize = 1009;
constexpr int32 YarlungTerrainMeshGridSize = 2017;
constexpr float YarlungMinX = -337778.431f;
constexpr float YarlungMaxX = 337778.431f;
constexpr float YarlungMinY = -416981.551f;
constexpr float YarlungMaxY = 416981.551f;
constexpr float YarlungEncodedMinZ = 260000.0f;
constexpr float YarlungEncodedMaxZ = 730000.0f;
const TCHAR* YarlungTerrainMeshPackagePath = TEXT("/Game/Generated/YarlungLandscape/SM_YarlungMeshTerrain");
const TCHAR* YarlungTerrainMeshAssetName = TEXT("SM_YarlungMeshTerrain");

float YarlungHeightValueToCm(uint16 Encoded)
{
    return FMath::Lerp(YarlungEncodedMinZ, YarlungEncodedMaxZ, static_cast<float>(Encoded) / 65535.0f);
}

float YarlungCubicBsplineInterp(float P0, float P1, float P2, float P3, float T)
{
    const float OneMinusT = 1.0f - T;
    const float W0 = OneMinusT * OneMinusT * OneMinusT / 6.0f;
    const float W1 = (3.0f * T * T * T - 6.0f * T * T + 4.0f) / 6.0f;
    const float W2 = (-3.0f * T * T * T + 3.0f * T * T + 3.0f * T + 1.0f) / 6.0f;
    const float W3 = T * T * T / 6.0f;
    return P0 * W0 + P1 * W1 + P2 * W2 + P3 * W3;
}

float YarlungSmooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

float YarlungValueNoise(float X, float Y)
{
    return FMath::Frac(FMath::Sin(X * 0.00173f + Y * 0.00291f) * 43758.5453f);
}

float YarlungRiverCenterY(float X)
{
    const float OffsetX = X - 95543.0f;
    return -142330.0f
        + 9000.0f * FMath::Sin(OffsetX * 0.00009f + 0.25f)
        + 4200.0f * FMath::Sin(OffsetX * 0.00021f - 0.6f);
}

float YarlungHeightAtGrid(const TArray<uint16>& HeightData, int32 XIndex, int32 YIndex)
{
    const int32 ClampedX = FMath::Clamp(XIndex, 0, YarlungHeightmapSize - 1);
    const int32 ClampedY = FMath::Clamp(YIndex, 0, YarlungHeightmapSize - 1);
    return YarlungHeightValueToCm(HeightData[ClampedY * YarlungHeightmapSize + ClampedX]);
}

float YarlungHeightAtSourceGrid(const TArray<uint16>& HeightData, float SourceX, float SourceY)
{
    const int32 BaseX = FMath::FloorToInt(SourceX);
    const int32 BaseY = FMath::FloorToInt(SourceY);
    const float TX = SourceX - static_cast<float>(BaseX);
    const float TY = SourceY - static_cast<float>(BaseY);
    float Rows[4] = {};
    float MinNeighbor = TNumericLimits<float>::Max();
    float MaxNeighbor = -TNumericLimits<float>::Max();

    for (int32 Row = 0; Row < 4; ++Row)
    {
        float Samples[4] = {};
        for (int32 Column = 0; Column < 4; ++Column)
        {
            Samples[Column] = YarlungHeightAtGrid(HeightData, BaseX + Column - 1, BaseY + Row - 1);
            MinNeighbor = FMath::Min(MinNeighbor, Samples[Column]);
            MaxNeighbor = FMath::Max(MaxNeighbor, Samples[Column]);
        }
        Rows[Row] = YarlungCubicBsplineInterp(Samples[0], Samples[1], Samples[2], Samples[3], TX);
    }

    return FMath::Clamp(
        YarlungCubicBsplineInterp(Rows[0], Rows[1], Rows[2], Rows[3], TY),
        MinNeighbor,
        MaxNeighbor);
}

float YarlungHeightAtMeshGrid(const TArray<uint16>& HeightData, int32 XIndex, int32 YIndex)
{
    const float U = static_cast<float>(XIndex) / static_cast<float>(YarlungTerrainMeshGridSize - 1);
    const float V = static_cast<float>(YIndex) / static_cast<float>(YarlungTerrainMeshGridSize - 1);
    return YarlungHeightAtSourceGrid(
        HeightData,
        U * static_cast<float>(YarlungHeightmapSize - 1),
        V * static_cast<float>(YarlungHeightmapSize - 1));
}

FVector YarlungPositionAtMeshGrid(const TArray<uint16>& HeightData, int32 XIndex, int32 YIndex)
{
    const float U = static_cast<float>(XIndex) / static_cast<float>(YarlungTerrainMeshGridSize - 1);
    const float V = static_cast<float>(YIndex) / static_cast<float>(YarlungTerrainMeshGridSize - 1);
    return FVector(
        FMath::Lerp(YarlungMinX, YarlungMaxX, U),
        FMath::Lerp(YarlungMinY, YarlungMaxY, V),
        YarlungHeightAtMeshGrid(HeightData, XIndex, YIndex));
}

FVector YarlungSmoothNormalAtMeshGrid(const TArray<uint16>& HeightData, int32 XIndex, int32 YIndex)
{
    const float XSpacing = (YarlungMaxX - YarlungMinX) / static_cast<float>(YarlungTerrainMeshGridSize - 1);
    const float YSpacing = (YarlungMaxY - YarlungMinY) / static_cast<float>(YarlungTerrainMeshGridSize - 1);
    const float Left = YarlungHeightAtMeshGrid(HeightData, XIndex - 1, YIndex);
    const float Right = YarlungHeightAtMeshGrid(HeightData, XIndex + 1, YIndex);
    const float Down = YarlungHeightAtMeshGrid(HeightData, XIndex, YIndex - 1);
    const float Up = YarlungHeightAtMeshGrid(HeightData, XIndex, YIndex + 1);
    return FVector(Left - Right, Down - Up, XSpacing + YSpacing).GetSafeNormal();
}

FLinearColor YarlungColorAtPosition(float X, float Y, float Height)
{
    const float Height01 = FMath::Clamp((Height - YarlungEncodedMinZ) / (YarlungEncodedMaxZ - YarlungEncodedMinZ), 0.0f, 1.0f);
    const float RiverDistance = FMath::Abs(Y - YarlungRiverCenterY(X));
    const float River = FMath::Clamp(1.0f - RiverDistance / 26000.0f, 0.0f, 1.0f);
    const float Forest = YarlungSmooth01((RiverDistance - 26000.0f) / 90000.0f) * (1.0f - YarlungSmooth01((Height01 - 0.43f) / 0.20f));
    const float Noise = YarlungValueNoise(X, Y);

    FLinearColor Base(0.24f + Noise * 0.04f, 0.36f + Noise * 0.08f, 0.29f + Noise * 0.04f, 1.0f);
    const FLinearColor Rock(0.36f + Height01 * 0.18f, 0.43f + Height01 * 0.14f, 0.40f + Height01 * 0.10f, 1.0f);
    const FLinearColor ForestColor(0.06f + Noise * 0.05f, 0.25f + Noise * 0.14f, 0.11f + Noise * 0.05f, 1.0f);
    const FLinearColor RiverColor(0.50f, 0.78f, 0.74f, 1.0f);
    const FLinearColor Snow(0.78f, 0.83f, 0.82f, 1.0f);

    Base = FMath::Lerp(Base, Rock, FMath::Clamp(Height01 * 0.95f, 0.0f, 0.65f));
    Base = FMath::Lerp(Base, ForestColor, FMath::Clamp(Forest, 0.0f, 0.82f));
    Base = FMath::Lerp(Base, RiverColor, River * 0.72f);
    Base = FMath::Lerp(Base, Snow, YarlungSmooth01((Height01 - 0.74f) / 0.11f));
    return Base;
}

UStaticMesh* BuildYarlungTerrainStaticMesh(const TArray<uint16>& HeightData)
{
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/Generated/Materials/M_YarlungMeshTerrain.M_YarlungMeshTerrain"));
    if (!Material)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing required mesh terrain material: /Game/Generated/Materials/M_YarlungMeshTerrain.M_YarlungMeshTerrain"));
        return nullptr;
    }

    UPackage* MeshPackage = CreatePackage(YarlungTerrainMeshPackagePath);
    MeshPackage->FullyLoad();
    MeshPackage->Modify();

    if (UObject* Existing = StaticFindObject(UStaticMesh::StaticClass(), MeshPackage, YarlungTerrainMeshAssetName))
    {
        Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
    }

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
        MeshPackage,
        FName(YarlungTerrainMeshAssetName),
        RF_Public | RF_Standalone);
    StaticMesh->InitResources();
    StaticMesh->SetLightingGuid();
    StaticMesh->SetLightMapResolution(64);
    StaticMesh->SetLightMapCoordinateIndex(0);
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("YarlungMeshTerrain"), TEXT("YarlungMeshTerrain")));
    StaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);

    FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
    SourceModel.BuildSettings.bRecomputeNormals = false;
    SourceModel.BuildSettings.bRecomputeTangents = false;
    SourceModel.BuildSettings.bRemoveDegenerates = false;
    SourceModel.BuildSettings.bUseHighPrecisionTangentBasis = true;
    SourceModel.BuildSettings.bUseFullPrecisionUVs = true;

    FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
    FStaticMeshAttributes(*MeshDescription).Register();

    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(MeshDescription);
    Builder.SetNumUVLayers(1);
    Builder.ReserveNewVertices(YarlungTerrainMeshGridSize * YarlungTerrainMeshGridSize);

    FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("YarlungMeshTerrain"));
    TArray<FVertexID> Vertices;
    Vertices.Reserve(YarlungTerrainMeshGridSize * YarlungTerrainMeshGridSize);
    for (int32 YIndex = 0; YIndex < YarlungTerrainMeshGridSize; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < YarlungTerrainMeshGridSize; ++XIndex)
        {
            Vertices.Add(Builder.AppendVertex(YarlungPositionAtMeshGrid(HeightData, XIndex, YIndex)));
        }
    }

    const auto AppendTerrainTriangle = [&](int32 X0, int32 Y0, int32 X1, int32 Y1, int32 X2, int32 Y2)
    {
        const int32 I0 = Y0 * YarlungTerrainMeshGridSize + X0;
        const int32 I1 = Y1 * YarlungTerrainMeshGridSize + X1;
        const int32 I2 = Y2 * YarlungTerrainMeshGridSize + X2;
        const FVertexInstanceID VI0 = Builder.AppendInstance(Vertices[I0]);
        const FVertexInstanceID VI1 = Builder.AppendInstance(Vertices[I1]);
        const FVertexInstanceID VI2 = Builder.AppendInstance(Vertices[I2]);
        const FVertexInstanceID Instances[3] = { VI0, VI1, VI2 };
        const int32 Xs[3] = { X0, X1, X2 };
        const int32 Ys[3] = { Y0, Y1, Y2 };

        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            const FVector Position = YarlungPositionAtMeshGrid(HeightData, Xs[Corner], Ys[Corner]);
            const FVector Normal = YarlungSmoothNormalAtMeshGrid(HeightData, Xs[Corner], Ys[Corner]);
            const FLinearColor Color = YarlungColorAtPosition(Position.X, Position.Y, Position.Z);
            Builder.SetInstanceTangentSpace(Instances[Corner], Normal, FVector::XAxisVector, 1.0f);
            Builder.SetInstanceUV(
                Instances[Corner],
                FVector2D(
                    static_cast<float>(Xs[Corner]) / static_cast<float>(YarlungTerrainMeshGridSize - 1),
                    static_cast<float>(Ys[Corner]) / static_cast<float>(YarlungTerrainMeshGridSize - 1)),
                0);
            Builder.SetInstanceColor(
                Instances[Corner],
                FVector4f(Color.R, Color.G, Color.B, Color.A));
        }

        Builder.AppendTriangle(VI0, VI1, VI2, PolygonGroup);
    };

    for (int32 YIndex = 0; YIndex < YarlungTerrainMeshGridSize - 1; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < YarlungTerrainMeshGridSize - 1; ++XIndex)
        {
            AppendTerrainTriangle(XIndex, YIndex, XIndex + 1, YIndex, XIndex + 1, YIndex + 1);
            AppendTerrainTriangle(XIndex, YIndex, XIndex + 1, YIndex + 1, XIndex, YIndex + 1);
        }
    }

    StaticMesh->CommitMeshDescription(0);

    FMeshNaniteSettings NaniteSettings = StaticMesh->GetNaniteSettings();
    NaniteSettings.bEnabled = true;
    NaniteSettings.KeepPercentTriangles = 1.0f;
    NaniteSettings.FallbackPercentTriangles = 1.0f;
    StaticMesh->SetNaniteSettings(NaniteSettings);

    FMeshSectionInfo SectionInfo = StaticMesh->GetSectionInfoMap().Get(0, 0);
    SectionInfo.MaterialIndex = 0;
    SectionInfo.bEnableCollision = false;
    StaticMesh->GetSectionInfoMap().Set(0, 0, SectionInfo);
    StaticMesh->GetOriginalSectionInfoMap().Set(0, 0, SectionInfo);

    StaticMesh->Build();
    StaticMesh->PostEditChange();
    StaticMesh->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(StaticMesh);

    if (!UEditorLoadingAndSavingUtils::SavePackages({ MeshPackage }, false))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to save Yarlung mesh terrain asset: %s"), YarlungTerrainMeshPackagePath);
        return nullptr;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Nanite Yarlung mesh terrain: %s vertices=%d triangles=%d nanite=%s"),
        *StaticMesh->GetPathName(),
        YarlungTerrainMeshGridSize * YarlungTerrainMeshGridSize,
        (YarlungTerrainMeshGridSize - 1) * (YarlungTerrainMeshGridSize - 1) * 2,
        StaticMesh->IsNaniteEnabled() ? TEXT("true") : TEXT("false"));
    return StaticMesh;
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
    constexpr int32 HeightmapSize = 1009;
    constexpr float MinX = -337778.431f;
    constexpr float MaxX = 337778.431f;
    constexpr float MinY = -416981.551f;
    constexpr float MaxY = 416981.551f;
    constexpr float EncodedMinZ = 260000.0f;
    constexpr float EncodedMaxZ = 730000.0f;
    constexpr int32 SectionsPerComponent = 1;
    constexpr int32 SectionSizeQuads = 63;

    const FString HeightmapPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectContentDir(),
        TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16")));
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

    UStaticMesh* MeshTerrainAsset = BuildYarlungTerrainStaticMesh(HeightData);
    if (!MeshTerrainAsset)
    {
        return 1;
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
    Landscape->SetActorHiddenInGame(true);

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
        UE_LOG(LogTemp, Error, TEXT("Missing required Yarlung landscape material: /Game/Generated/Materials/M_YarlungLandscapeGround.M_YarlungLandscapeGround"));
        return 1;
    }
    Landscape->LandscapeMaterial = LandscapeMaterial;
    Landscape->UpdateAllComponentMaterialInstances(true);
    UE_LOG(LogTemp, Display, TEXT("Assigned Yarlung landscape material: %s"), *LandscapeMaterial->GetPathName());

    Landscape->PostEditChange();

    AYarlungMeshTerrainActor* MeshTerrain = World->SpawnActor<AYarlungMeshTerrainActor>(
        AYarlungMeshTerrainActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (MeshTerrain)
    {
        MeshTerrain->SetActorLabel(TEXT("YarlungMeshTerrainScenery"));
        if (UStaticMeshComponent* MeshComponent = MeshTerrain->FindComponentByClass<UStaticMeshComponent>())
        {
            MeshComponent->SetStaticMesh(MeshTerrainAsset);
        }
    }

    ACoasterRideActor* Ride = World->SpawnActor<ACoasterRideActor>(
        ACoasterRideActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (Ride)
    {
        Ride->SetActorLabel(TEXT("YarlungCoasterRide"));
    }

    AYarlungRiverActor* River = World->SpawnActor<AYarlungRiverActor>(
        AYarlungRiverActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (River)
    {
        River->SetActorLabel(TEXT("YarlungRiverScenery"));
    }

    AYarlungSceneryActor* Scenery = World->SpawnActor<AYarlungSceneryActor>(
        AYarlungSceneryActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    if (Scenery)
    {
        Scenery->SetActorLabel(TEXT("YarlungForestRockScenery"));
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
