#include "YarlungRiverSurfaceBuilder.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "YarlungAssetConfig.h"
#include "YarlungDeterministicNoise.h"
#include "YarlungGeneratedPaths.h"
#include "YarlungRiverField.h"
#include "YarlungTerrainProfile.h"
#include "Engine/StaticMesh.h"
#include "FileHelpers.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "UObject/Linker.h"

namespace YarlungRiverSurfaceBuilder
{
UStaticMesh* BuildStaticMesh(const FYarlungRiverField& RiverField)
{
    const FYarlungWaterConfig& WaterConfig = YarlungAssets::Config().Water;
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr,
        *WaterConfig.SurfaceMaterialPath);
    if (!Material)
    {
        UE_LOG(LogTemp, Fatal, TEXT("Missing required river surface material: %s"), *WaterConfig.SurfaceMaterialPath);
    }

    const TArray<FYarlungRiverRow>& Rows = RiverField.GetRows();
    if (Rows.Num() < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few river rows to build river surface: %d"), Rows.Num());
        return nullptr;
    }

    UPackage* MeshPackage = CreatePackage(YarlungGeneratedPaths::RiverSurfaceMeshPackagePath);
    MeshPackage->FullyLoad();
    MeshPackage->Modify();
    if (UObject* Existing = StaticFindObject(
            UStaticMesh::StaticClass(),
            MeshPackage,
            YarlungGeneratedPaths::RiverSurfaceMeshAssetName))
    {
        ResetLoaders(MeshPackage);
        Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_AllowPackageLinkerMismatch);
    }

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
        MeshPackage,
        FName(YarlungGeneratedPaths::RiverSurfaceMeshAssetName),
        RF_Public | RF_Standalone);
    StaticMesh->InitResources();
    StaticMesh->SetLightingGuid();
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("YarlungRiverSurface"), TEXT("YarlungRiverSurface")));
    StaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);

    FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
    SourceModel.BuildSettings.bRecomputeNormals = false;
    SourceModel.BuildSettings.bRecomputeTangents = false;
    SourceModel.BuildSettings.bRemoveDegenerates = false;
    SourceModel.BuildSettings.bUseFullPrecisionUVs = true;

    FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);
    FStaticMeshAttributes(*MeshDescription).Register();
    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(MeshDescription);
    Builder.SetNumUVLayers(1);

    constexpr int32 CrossSamples = 33;
    const int32 RowCount = Rows.Num();
    TArray<FVertexID> VertexIds;
    TArray<FVector> VertexPositions;
    TArray<FVector> VertexNormals;
    TArray<FVector> VertexTangents;
    TArray<FVector2D> VertexUvs;
    TArray<FVector4f> VertexColors;
    VertexIds.SetNumUninitialized(RowCount * CrossSamples);
    VertexPositions.SetNumUninitialized(RowCount * CrossSamples);
    VertexNormals.SetNumUninitialized(RowCount * CrossSamples);
    VertexTangents.SetNumUninitialized(RowCount * CrossSamples);
    VertexUvs.SetNumUninitialized(RowCount * CrossSamples);
    VertexColors.SetNumUninitialized(RowCount * CrossSamples);
    float MinSurfaceZ = TNumericLimits<float>::Max();
    float MaxSurfaceZ = -TNumericLimits<float>::Max();
    int32 FoamVertexCount = 0;

    const auto VertexIndex = [](int32 RowIndex, int32 CrossIndex)
    {
        return RowIndex * CrossSamples + CrossIndex;
    };

    for (int32 Index = 0; Index < RowCount; ++Index)
    {
        const FVector Center = Rows[Index].PositionCm;
        const FVector Next = Rows[FMath::Min(Index + 1, RowCount - 1)].PositionCm;
        const FVector Prev = Rows[FMath::Max(Index - 1, 0)].PositionCm;
        FVector2D Tangent = FVector2D(Next.X - Prev.X, Next.Y - Prev.Y).GetSafeNormal();
        if (Tangent.IsNearlyZero())
        {
            Tangent = FVector2D(1.0f, 0.0f);
        }
        const FVector2D Normal(-Tangent.Y, Tangent.X);
        const float HalfWidth = FYarlungRiverField::VisibleRibbonHalfWidthCm(Rows[Index].HalfWidthCm);
        const float Flow = FMath::Clamp(Rows[Index].Flow, 0.0f, 1.0f);
        const float SegmentDropCm = FMath::Abs(Next.Z - Prev.Z);
        const float SegmentLengthCm = FMath::Max(1.0f, FVector2D(Next.X - Prev.X, Next.Y - Prev.Y).Size());
        const float SlopeRapid = YarlungTerrain::Smooth01((SegmentDropCm / SegmentLengthCm - 0.018f) / 0.055f);
        const float LeftBankScale = FMath::Clamp(
            0.95f
                + 0.16f * YarlungDeterministicNoise::Signed(Flow * 93000.0f + 17.0f, Flow * 51000.0f)
                + 0.08f * YarlungDeterministicNoise::Signed(Flow * 311000.0f, Flow * 177000.0f + 41.0f),
            0.78f,
            1.16f);
        const float RightBankScale = FMath::Clamp(
            0.96f
                + 0.15f * YarlungDeterministicNoise::Signed(Flow * 87000.0f + 83.0f, Flow * 61000.0f + 29.0f)
                + 0.08f * YarlungDeterministicNoise::Signed(Flow * 293000.0f + 19.0f, Flow * 193000.0f),
            0.78f,
            1.15f);
        const float CenterWanderCm = HalfWidth * 0.055f * YarlungDeterministicNoise::Signed(Flow * 71000.0f, Flow * 139000.0f + 11.0f);

        for (int32 CrossIndex = 0; CrossIndex < CrossSamples; ++CrossIndex)
        {
            const float Across01 = static_cast<float>(CrossIndex) / static_cast<float>(CrossSamples - 1);
            const float AcrossSigned = 1.0f - Across01 * 2.0f;
            const float EdgeFoam = YarlungTerrain::Smooth01((FMath::Abs(AcrossSigned) - 0.78f) / 0.18f);
            const float CenterRapid = (1.0f - YarlungTerrain::Smooth01(FMath::Abs(AcrossSigned) / 0.34f))
                * (0.45f + 0.55f * SlopeRapid);
            const float PatchNoise = FMath::Clamp(
                YarlungDeterministicNoise::Value01(Flow * 220000.0f + AcrossSigned * 31000.0f, Flow * 113000.0f - AcrossSigned * 47000.0f) * 0.62f
                    + YarlungDeterministicNoise::Value01(Flow * 790000.0f - AcrossSigned * 19000.0f, Flow * 470000.0f + AcrossSigned * 29000.0f) * 0.38f,
                0.0f,
                1.0f);
            const float PatchGate = YarlungTerrain::Smooth01((PatchNoise - 0.48f) / 0.28f);
            const float BoilSource = FMath::Clamp(
                0.58f * YarlungDeterministicNoise::Value01(Flow * 620000.0f + AcrossSigned * 81000.0f, Flow * 270000.0f - AcrossSigned * 191000.0f)
                    + 0.42f * YarlungDeterministicNoise::Value01(Flow * 1310000.0f - AcrossSigned * 126000.0f, Flow * 880000.0f + AcrossSigned * 73000.0f),
                0.0f,
                1.0f);
            const float BoilPatch = YarlungTerrain::Smooth01((BoilSource - 0.52f) / 0.24f)
                * (0.55f + 0.45f * PatchGate);
            const float BrokenRapid = FMath::Pow(
                FMath::Clamp(
                    0.42f
                        * YarlungDeterministicNoise::Value01(Flow * 1710000.0f + AcrossSigned * 263000.0f, Flow * 690000.0f)
                        + 0.34f * YarlungDeterministicNoise::Value01(Flow * 930000.0f, AcrossSigned * 377000.0f + Flow * 110000.0f)
                        + 0.24f * PatchGate,
                    0.0f,
                    1.0f),
                2.4f);
            const float CrossWake = FMath::Pow(1.0f - FMath::Abs(AcrossSigned), 2.2f);
            const float MidChannel = FMath::Pow(1.0f - FMath::Abs(AcrossSigned), 1.35f);
            const float Foam = FMath::Clamp(
                EdgeFoam * (0.008f + 0.055f * BrokenRapid)
                    + CenterRapid * (BrokenRapid * 0.09f + BoilPatch * 0.12f)
                    + MidChannel * BoilPatch * 0.035f,
                0.0f,
                0.24f);
            const float RawRippleZ = (5.0f + 8.0f * CrossWake) * FMath::Sin(Flow * 287.0f + AcrossSigned * 23.0f + PatchNoise * 5.0f)
                + 5.0f * FMath::Sin(Flow * 431.0f - AcrossSigned * 47.0f + BrokenRapid * 2.0f)
                + 8.0f * YarlungDeterministicNoise::Signed(Flow * 360000.0f, AcrossSigned * 64000.0f)
                + 4.0f * BoilPatch;
            const float RippleZ = FMath::Clamp(RawRippleZ, -10.0f, 18.0f);
            const float SurfaceZ = Center.Z + FYarlungRiverField::DefaultWaterSurfaceLiftCm + RippleZ;
            MinSurfaceZ = FMath::Min(MinSurfaceZ, SurfaceZ);
            MaxSurfaceZ = FMath::Max(MaxSurfaceZ, SurfaceZ);

            const float SideWidthFactor = AcrossSigned >= 0.0f ? LeftBankScale : RightBankScale;
            const float EdgeIrregularityCm = HalfWidth
                * 0.045f
                * YarlungDeterministicNoise::Signed(Flow * 410000.0f + AcrossSigned * 57000.0f, Flow * 233000.0f - AcrossSigned * 99000.0f)
                * FMath::Pow(FMath::Abs(AcrossSigned), 1.7f);
            const float AcrossOffsetCm = CenterWanderCm + AcrossSigned * HalfWidth * SideWidthFactor + EdgeIrregularityCm;
            const FVector Position(
                Center.X + Normal.X * AcrossOffsetCm,
                Center.Y + Normal.Y * AcrossOffsetCm,
                SurfaceZ);
            const int32 Id = VertexIndex(Index, CrossIndex);
            VertexPositions[Id] = Position;
            VertexIds[Id] = Builder.AppendVertex(Position);
            VertexUvs[Id] = FVector2D(Across01 + PatchNoise * 0.035f, Flow * 8.5f + BoilPatch * 0.22f);

            const FLinearColor DeepWater(0.001f, 0.017f, 0.023f, 1.0f);
            const FLinearColor GlacialGreen(0.004f, 0.055f, 0.052f, 1.0f);
            const FLinearColor AeratedFoam(0.24f, 0.33f, 0.30f, 1.0f);
            const float ChannelTint = 0.028f + 0.040f * PatchNoise + 0.034f * BoilPatch + 0.055f * (1.0f - CrossWake);
            const FLinearColor WaterColor = FMath::Lerp(DeepWater, GlacialGreen, FMath::Clamp(ChannelTint, 0.0f, 0.28f));
            const FLinearColor FinalColor = FMath::Lerp(WaterColor, AeratedFoam, Foam);
            VertexColors[Id] = FVector4f(FinalColor.R, FinalColor.G, FinalColor.B, 1.0f);
            if (Foam > 0.12f)
            {
                ++FoamVertexCount;
            }
        }
    }

    for (int32 Index = 0; Index < RowCount; ++Index)
    {
        for (int32 CrossIndex = 0; CrossIndex < CrossSamples; ++CrossIndex)
        {
            const int32 PrevRow = FMath::Max(Index - 1, 0);
            const int32 NextRow = FMath::Min(Index + 1, RowCount - 1);
            const int32 PrevCross = FMath::Max(CrossIndex - 1, 0);
            const int32 NextCross = FMath::Min(CrossIndex + 1, CrossSamples - 1);

            const FVector Along = VertexPositions[VertexIndex(NextRow, CrossIndex)] - VertexPositions[VertexIndex(PrevRow, CrossIndex)];
            const FVector Across = VertexPositions[VertexIndex(Index, NextCross)] - VertexPositions[VertexIndex(Index, PrevCross)];
            FVector NormalAlong = Along;
            FVector NormalAcross = Across;
            NormalAlong.Z *= 1.2f;
            NormalAcross.Z *= 1.2f;
            FVector SurfaceNormal = FVector::CrossProduct(NormalAlong, NormalAcross).GetSafeNormal();
            if (SurfaceNormal.Z < 0.0f)
            {
                SurfaceNormal *= -1.0f;
            }
            if (SurfaceNormal.IsNearlyZero())
            {
                SurfaceNormal = FVector::UpVector;
            }

            FVector SurfaceTangent = (Along - SurfaceNormal * FVector::DotProduct(Along, SurfaceNormal)).GetSafeNormal();
            if (SurfaceTangent.IsNearlyZero())
            {
                SurfaceTangent = FVector::XAxisVector;
            }

            const int32 Id = VertexIndex(Index, CrossIndex);
            VertexNormals[Id] = SurfaceNormal;
            VertexTangents[Id] = SurfaceTangent;
        }
    }

    FPolygonGroupID PolygonGroup = Builder.AppendPolygonGroup(TEXT("YarlungRiverSurface"));
    const auto AppendTri = [&](int32 A, int32 B, int32 C)
    {
        const FVertexInstanceID IA = Builder.AppendInstance(VertexIds[A]);
        const FVertexInstanceID IB = Builder.AppendInstance(VertexIds[B]);
        const FVertexInstanceID IC = Builder.AppendInstance(VertexIds[C]);
        const FVertexInstanceID Instances[3] = { IA, IB, IC };
        const FVector2D Uvs[3] = { VertexUvs[A], VertexUvs[B], VertexUvs[C] };
        const FVector4f Colors[3] = { VertexColors[A], VertexColors[B], VertexColors[C] };
        const int32 VertexIndices[3] = { A, B, C };
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Builder.SetInstanceTangentSpace(
                Instances[Corner],
                VertexNormals[VertexIndices[Corner]],
                VertexTangents[VertexIndices[Corner]],
                1.0f);
            Builder.SetInstanceUV(Instances[Corner], Uvs[Corner], 0);
            Builder.SetInstanceColor(Instances[Corner], Colors[Corner]);
        }
        Builder.AppendTriangle(IA, IB, IC, PolygonGroup);
    };

    for (int32 Index = 0; Index + 1 < RowCount; ++Index)
    {
        for (int32 CrossIndex = 0; CrossIndex + 1 < CrossSamples; ++CrossIndex)
        {
            const int32 A = VertexIndex(Index, CrossIndex);
            const int32 B = VertexIndex(Index + 1, CrossIndex);
            const int32 C = VertexIndex(Index + 1, CrossIndex + 1);
            const int32 D = VertexIndex(Index, CrossIndex + 1);
            AppendTri(A, B, C);
            AppendTri(A, C, D);
        }
    }

    StaticMesh->CommitMeshDescription(0);

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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Unable to save Yarlung river surface asset: %s"),
            YarlungGeneratedPaths::RiverSurfaceMeshPackagePath);
        return nullptr;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Yarlung river surface ribbon: %s rows=%d cross_lanes=%d foam_vertices=%d surface_z_cm=%.0f..%.0f"),
        *StaticMesh->GetPathName(),
        RowCount,
        CrossSamples,
        FoamVertexCount,
        MinSurfaceZ,
        MaxSurfaceZ);
    return StaticMesh;
}
}

#endif
