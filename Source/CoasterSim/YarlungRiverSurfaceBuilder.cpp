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

namespace
{
struct FRiverSurfaceRow
{
    FVector PositionCm = FVector::ZeroVector;
    float HalfWidthCm = 0.0f;
    float Flow = 0.0f;
};

float SmoothSignedFlowNoise(float Flow, float Phase)
{
    const float TwoPi = UE_TWO_PI;
    return 0.56f * FMath::Sin(Flow * TwoPi * 1.7f + Phase)
        + 0.31f * FMath::Sin(Flow * TwoPi * 3.9f + Phase * 1.73f)
        + 0.13f * FMath::Sin(Flow * TwoPi * 8.1f + Phase * 2.41f);
}

TArray<FRiverSurfaceRow> BuildResampledSurfaceRows(const TArray<FYarlungRiverRow>& SourceRows)
{
    constexpr float MaxSegmentLengthCm = 600.0f;

    TArray<FRiverSurfaceRow> SurfaceRows;
    if (SourceRows.IsEmpty())
    {
        return SurfaceRows;
    }

    for (int32 Index = 0; Index + 1 < SourceRows.Num(); ++Index)
    {
        const FYarlungRiverRow& A = SourceRows[Index];
        const FYarlungRiverRow& B = SourceRows[Index + 1];
        const FVector Segment = B.PositionCm - A.PositionCm;
        const int32 StepCount = FMath::Max(1, FMath::CeilToInt(FVector2D(Segment.X, Segment.Y).Size() / MaxSegmentLengthCm));

        const FVector P0 = SourceRows[FMath::Max(Index - 1, 0)].PositionCm;
        const FVector P1 = A.PositionCm;
        const FVector P2 = B.PositionCm;
        const FVector P3 = SourceRows[FMath::Min(Index + 2, SourceRows.Num() - 1)].PositionCm;
        const FVector ArriveTangent = (P2 - P0) * 0.5f;
        const FVector LeaveTangent = (P3 - P1) * 0.5f;

        for (int32 Step = 0; Step < StepCount; ++Step)
        {
            const float T = static_cast<float>(Step) / static_cast<float>(StepCount);
            FRiverSurfaceRow Row;
            Row.PositionCm = FMath::CubicInterp(P1, ArriveTangent, P2, LeaveTangent, T);
            Row.HalfWidthCm = FMath::Lerp(A.HalfWidthCm, B.HalfWidthCm, T);
            Row.Flow = FMath::Lerp(A.Flow, B.Flow, T);
            SurfaceRows.Add(Row);
        }
    }

    FRiverSurfaceRow LastRow;
    LastRow.PositionCm = SourceRows.Last().PositionCm;
    LastRow.HalfWidthCm = SourceRows.Last().HalfWidthCm;
    LastRow.Flow = SourceRows.Last().Flow;
    SurfaceRows.Add(LastRow);
    return SurfaceRows;
}
}

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

    const TArray<FYarlungRiverRow>& SourceRows = RiverField.GetRows();
    if (SourceRows.Num() < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few river rows to build river surface: %d"), SourceRows.Num());
        return nullptr;
    }

    const TArray<FRiverSurfaceRow> Rows = BuildResampledSurfaceRows(SourceRows);
    if (Rows.Num() < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few resampled river surface rows: %d"), Rows.Num());
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

    constexpr int32 CrossSamples = 45;
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
        const float LeftBankScale = FMath::Clamp(0.97f + 0.07f * SmoothSignedFlowNoise(Flow, 0.6f), 0.88f, 1.08f);
        const float RightBankScale = FMath::Clamp(0.98f + 0.07f * SmoothSignedFlowNoise(Flow, 2.2f), 0.88f, 1.08f);
        const float CenterWanderCm = HalfWidth * 0.022f * SmoothSignedFlowNoise(Flow, 4.1f);

        for (int32 CrossIndex = 0; CrossIndex < CrossSamples; ++CrossIndex)
        {
            const float Across01 = static_cast<float>(CrossIndex) / static_cast<float>(CrossSamples - 1);
            const float AcrossSigned = 1.0f - Across01 * 2.0f;
            const float EdgeFoam = YarlungTerrain::Smooth01((FMath::Abs(AcrossSigned) - 0.54f) / 0.28f);
            const float CenterRapid = (1.0f - YarlungTerrain::Smooth01(FMath::Abs(AcrossSigned) / 0.34f))
                * (0.62f + 0.38f * SlopeRapid);
            const float LongStreakSource = FMath::Clamp(
                0.46f * (0.5f + 0.5f * FMath::Sin(Flow * UE_TWO_PI * 42.0f + AcrossSigned * 9.0f))
                    + 0.34f * (0.5f + 0.5f * FMath::Sin(Flow * UE_TWO_PI * 83.0f - AcrossSigned * 17.0f))
                    + 0.20f * YarlungDeterministicNoise::Value01(Flow * 1510000.0f, AcrossSigned * 167000.0f),
                0.0f,
                1.0f);
            const float LongStreak = YarlungTerrain::Smooth01((LongStreakSource - 0.48f) / 0.20f)
                * FMath::Pow(1.0f - FMath::Abs(AcrossSigned), 0.72f);
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
                EdgeFoam * (0.075f + 0.22f * BrokenRapid + 0.18f * LongStreak)
                    + CenterRapid * (BrokenRapid * 0.18f + BoilPatch * 0.20f + LongStreak * 0.24f)
                    + MidChannel * BoilPatch * 0.070f,
                0.0f,
                0.68f);
            const float RawRippleZ = (7.0f + 15.0f * CrossWake) * FMath::Sin(Flow * 287.0f + AcrossSigned * 23.0f + PatchNoise * 5.0f)
                + 11.0f * FMath::Sin(Flow * 431.0f - AcrossSigned * 47.0f + BrokenRapid * 2.0f)
                + 14.0f * YarlungDeterministicNoise::Signed(Flow * 360000.0f, AcrossSigned * 64000.0f)
                + 13.0f * LongStreak
                + 8.0f * BoilPatch;
            const float RippleZ = FMath::Clamp(RawRippleZ, -18.0f, 42.0f);
            const float SurfaceZ = Center.Z + FYarlungRiverField::DefaultWaterSurfaceLiftCm + RippleZ;
            MinSurfaceZ = FMath::Min(MinSurfaceZ, SurfaceZ);
            MaxSurfaceZ = FMath::Max(MaxSurfaceZ, SurfaceZ);

            const float SideWidthFactor = AcrossSigned >= 0.0f ? LeftBankScale : RightBankScale;
            const float EdgeIrregularityCm = HalfWidth
                * 0.012f
                * SmoothSignedFlowNoise(Flow, AcrossSigned >= 0.0f ? 5.3f : 7.1f)
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

            const FLinearColor DeepWater(0.002f, 0.026f, 0.034f, 1.0f);
            const FLinearColor GlacialGreen(0.015f, 0.135f, 0.125f, 1.0f);
            const FLinearColor AeratedFoam(0.52f, 0.68f, 0.62f, 1.0f);
            const float ChannelTint = 0.055f + 0.065f * PatchNoise + 0.050f * BoilPatch + 0.080f * (1.0f - CrossWake) + 0.060f * LongStreak;
            const FLinearColor WaterColor = FMath::Lerp(DeepWater, GlacialGreen, FMath::Clamp(ChannelTint, 0.0f, 0.28f));
            const FLinearColor FinalColor = FMath::Lerp(WaterColor, AeratedFoam, Foam);
            VertexColors[Id] = FVector4f(FinalColor.R, FinalColor.G, FinalColor.B, 0.86f + Foam * 0.13f);
            if (Foam > 0.18f)
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
            NormalAlong.Z *= 2.0f;
            NormalAcross.Z *= 2.0f;
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
        TEXT("Built Yarlung river surface ribbon: %s source_rows=%d surface_rows=%d cross_lanes=%d foam_vertices=%d surface_z_cm=%.0f..%.0f"),
        *StaticMesh->GetPathName(),
        SourceRows.Num(),
        RowCount,
        CrossSamples,
        FoamVertexCount,
        MinSurfaceZ,
        MaxSurfaceZ);
    return StaticMesh;
}
}

#endif
