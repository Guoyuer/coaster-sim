#include "YarlungCliffActor.h"

#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

namespace
{
constexpr int32 HeightmapSize = 1009;
constexpr float MinX = -337778.431f;
constexpr float MaxX = 337778.431f;
constexpr float MinY = -416981.551f;
constexpr float MaxY = 416981.551f;
constexpr float EncodedMinZ = 260000.0f;
constexpr float EncodedMaxZ = 730000.0f;

struct FTrackCsvRow
{
    FVector Position = FVector::ZeroVector;
    FString Section;
};

bool ParseTrackRow(const FString& Line, FTrackCsvRow& OutRow)
{
    TArray<FString> Columns;
    Line.ParseIntoArray(Columns, TEXT(","), true);
    if (Columns.Num() < 7)
    {
        return false;
    }

    OutRow.Position = FVector(
        FCString::Atof(*Columns[1]),
        FCString::Atof(*Columns[2]),
        FCString::Atof(*Columns[3]));
    OutRow.Section = Columns[5].TrimStartAndEnd();
    return true;
}

FVector HorizontalForward(const FVector& A, const FVector& B)
{
    FVector Forward = B - A;
    Forward.Z = 0.0f;
    if (Forward.IsNearlyZero())
    {
        return FVector::ForwardVector;
    }
    return Forward.GetSafeNormal();
}

FVector RightFromForward(const FVector& Forward)
{
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    return Right.IsNearlyZero() ? FVector::RightVector : Right;
}

float HeightValueToCm(uint16 Encoded)
{
    return FMath::Lerp(EncodedMinZ, EncodedMaxZ, static_cast<float>(Encoded) / 65535.0f);
}

}

AYarlungCliffActor::AYarlungCliffActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    CliffMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CliffMesh"));
    CliffMesh->SetupAttachment(SceneRoot);
    CliffMesh->bUseAsyncCooking = true;
    CliffMesh->SetCastShadow(true);
}

void AYarlungCliffActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildCliff();
}

void AYarlungCliffActor::BeginPlay()
{
    Super::BeginPlay();
    RebuildCliff();
}

void AYarlungCliffActor::RebuildCliff()
{
    TArray<FYarlungCliffPathSample> PathSamples;
    TArray<uint16> HeightData;
    if (!LoadOutboundPath(PathSamples) || !LoadHeightmap(HeightData))
    {
        CliffMesh->ClearAllMeshSections();
        return;
    }

    BuildCliffMesh(PathSamples, HeightData);
    ApplyMaterial();
}

bool AYarlungCliffActor::LoadOutboundPath(TArray<FYarlungCliffPathSample>& OutSamples) const
{
    const FString Path = FPaths::ProjectContentDir() / TrackCsvRelativePath;
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung track CSV for cliff mesh: %s"), *Path);
        return false;
    }

    TArray<FTrackCsvRow> Rows;
    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        FTrackCsvRow Row;
        if (ParseTrackRow(Lines[LineIndex], Row) && Row.Section.Equals(TEXT("Outbound"), ESearchCase::IgnoreCase))
        {
            Rows.Add(Row);
        }
    }

    if (Rows.Num() < 4)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few outbound track samples for cliff mesh: %d"), Rows.Num());
        return false;
    }

    OutSamples.Reset();
    constexpr float MaxStepCm = 600.0f;
    for (int32 Index = 0; Index < Rows.Num() - 1; ++Index)
    {
        const FVector Start = Rows[Index].Position;
        const FVector End = Rows[Index + 1].Position;
        const float SegmentLength = FVector::Dist2D(Start, End);
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(SegmentLength / MaxStepCm));
        const FVector Forward = HorizontalForward(Start, End);
        const FVector Right = RightFromForward(Forward);

        for (int32 Step = 0; Step < Steps; ++Step)
        {
            const float Alpha = static_cast<float>(Step) / static_cast<float>(Steps);
            FYarlungCliffPathSample Sample;
            Sample.Center = FMath::Lerp(Start, End, Alpha);
            Sample.Forward = Forward;
            Sample.Right = Right;
            OutSamples.Add(Sample);
        }
    }

    FYarlungCliffPathSample LastSample;
    LastSample.Center = Rows.Last().Position;
    LastSample.Forward = HorizontalForward(Rows[Rows.Num() - 2].Position, Rows.Last().Position);
    LastSample.Right = RightFromForward(LastSample.Forward);
    OutSamples.Add(LastSample);

    UE_LOG(LogTemp, Display, TEXT("Loaded Yarlung cliff path: %d outbound samples"), OutSamples.Num());
    return OutSamples.Num() >= 4;
}

bool AYarlungCliffActor::LoadHeightmap(TArray<uint16>& OutHeightData) const
{
    const FString Path = FPaths::ProjectContentDir() / HeightmapRelativePath;
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung heightmap for cliff mesh: %s"), *Path);
        return false;
    }

    const int32 ExpectedByteCount = HeightmapSize * HeightmapSize * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung heightmap has %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return false;
    }

    OutHeightData.SetNumUninitialized(HeightmapSize * HeightmapSize);
    FMemory::Memcpy(OutHeightData.GetData(), RawBytes.GetData(), RawBytes.Num());
    return true;
}

float AYarlungCliffActor::SampleHeightCm(const TArray<uint16>& HeightData, float X, float Y) const
{
    const float U = FMath::Clamp((X - MinX) / (MaxX - MinX), 0.0f, 1.0f) * static_cast<float>(HeightmapSize - 1);
    const float V = FMath::Clamp((Y - MinY) / (MaxY - MinY), 0.0f, 1.0f) * static_cast<float>(HeightmapSize - 1);
    const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, HeightmapSize - 1);
    const int32 Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, HeightmapSize - 1);
    const int32 X1 = FMath::Min(X0 + 1, HeightmapSize - 1);
    const int32 Y1 = FMath::Min(Y0 + 1, HeightmapSize - 1);
    const float AlphaX = U - static_cast<float>(X0);
    const float AlphaY = V - static_cast<float>(Y0);

    const float H00 = HeightValueToCm(HeightData[Y0 * HeightmapSize + X0]);
    const float H10 = HeightValueToCm(HeightData[Y0 * HeightmapSize + X1]);
    const float H01 = HeightValueToCm(HeightData[Y1 * HeightmapSize + X0]);
    const float H11 = HeightValueToCm(HeightData[Y1 * HeightmapSize + X1]);
    return FMath::Lerp(FMath::Lerp(H00, H10, AlphaX), FMath::Lerp(H01, H11, AlphaX), AlphaY);
}

void AYarlungCliffActor::BuildCliffMesh(const TArray<FYarlungCliffPathSample>& PathSamples, const TArray<uint16>& HeightData)
{
    static const TArray<float> AcrossOffsetsCm = {
        9000.0f, 12000.0f, 16000.0f, 21000.0f, 27000.0f,
        34000.0f, 42000.0f, 51000.0f, 61000.0f, 72000.0f, 84000.0f
    };
    constexpr float TrackSidePullCm = 5200.0f;
    constexpr float SurfaceLiftCm = 850.0f;
    const int32 AlongCount = PathSamples.Num();
    const int32 AcrossCount = AcrossOffsetsCm.Num();

    TArray<FVector> Vertices;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    TArray<float> SmoothedHeights;
    Vertices.SetNumUninitialized(AlongCount * AcrossCount);
    UVs.SetNumUninitialized(AlongCount * AcrossCount);
    Colors.SetNumUninitialized(AlongCount * AcrossCount);
    Tangents.SetNumUninitialized(AlongCount * AcrossCount);
    SmoothedHeights.SetNumUninitialized(AlongCount * AcrossCount);

    for (int32 Along = 0; Along < AlongCount; ++Along)
    {
        const FYarlungCliffPathSample& Sample = PathSamples[Along];
        for (int32 Across = 0; Across < AcrossCount; ++Across)
        {
            const int32 VertexIndex = Along * AcrossCount + Across;
            const float Offset = AcrossOffsetsCm[Across];
            const FVector XY = Sample.Center + Sample.Right * Offset;
            const float RockBreakup = 95.0f * FMath::Sin(XY.X * 0.00031f + XY.Y * 0.00017f)
                + 45.0f * FMath::Sin(XY.X * 0.00073f - XY.Y * 0.00029f);
            SmoothedHeights[VertexIndex] = SampleHeightCm(HeightData, XY.X, XY.Y) + RockBreakup + SurfaceLiftCm;
        }
    }

    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        TArray<float> NextHeights = SmoothedHeights;
        for (int32 Along = 1; Along < AlongCount - 1; ++Along)
        {
            for (int32 Across = 1; Across < AcrossCount - 1; ++Across)
            {
                const int32 VertexIndex = Along * AcrossCount + Across;
                const float NeighborAverage = (
                    SmoothedHeights[(Along - 1) * AcrossCount + Across]
                    + SmoothedHeights[(Along + 1) * AcrossCount + Across]
                    + SmoothedHeights[Along * AcrossCount + Across - 1]
                    + SmoothedHeights[Along * AcrossCount + Across + 1]) * 0.25f;
                NextHeights[VertexIndex] = FMath::Lerp(SmoothedHeights[VertexIndex], NeighborAverage, 0.48f);
            }
        }
        SmoothedHeights = MoveTemp(NextHeights);
    }

    for (int32 Along = 0; Along < AlongCount; ++Along)
    {
        const FYarlungCliffPathSample& Sample = PathSamples[Along];
        for (int32 Across = 0; Across < AcrossCount; ++Across)
        {
            const int32 VertexIndex = Along * AcrossCount + Across;
            const float Offset = AcrossOffsetsCm[Across];
            FVector Position = Sample.Center + Sample.Right * (Offset - TrackSidePullCm);
            Position.Z = SmoothedHeights[VertexIndex];
            Vertices[VertexIndex] = Position;
            UVs[VertexIndex] = FVector2D(static_cast<float>(Along) * 0.12f, Offset * 0.00011f);
            const float AcrossWeight = static_cast<float>(Across) / static_cast<float>(AcrossCount - 1);
            const float Strata = 0.5f + 0.5f * FMath::Sin(Position.Z * 0.0018f + static_cast<float>(Along) * 0.13f);
            const float Wet = FMath::Clamp(1.0f - AcrossWeight * 0.72f, 0.0f, 1.0f);
            Colors[VertexIndex] = FLinearColor(
                0.23f + AcrossWeight * 0.10f + Strata * 0.035f,
                0.30f + AcrossWeight * 0.10f + Wet * 0.05f,
                0.27f + AcrossWeight * 0.08f + Wet * 0.04f,
                1.0f);
            Tangents[VertexIndex] = FProcMeshTangent(Sample.Forward, false);
        }
    }

    TArray<int32> Triangles;
    Triangles.Reserve((AlongCount - 1) * (AcrossCount - 1) * 6);
    for (int32 Along = 0; Along < AlongCount - 1; ++Along)
    {
        for (int32 Across = 0; Across < AcrossCount - 1; ++Across)
        {
            const int32 A = Along * AcrossCount + Across;
            const int32 B = (Along + 1) * AcrossCount + Across;
            const int32 C = Along * AcrossCount + Across + 1;
            const int32 D = (Along + 1) * AcrossCount + Across + 1;
            Triangles.Append({ A, B, C, C, B, D });
        }
    }

    TArray<FVector> Normals;
    Normals.SetNumUninitialized(Vertices.Num());
    for (int32 Along = 0; Along < AlongCount; ++Along)
    {
        const FVector FacingTrackNormal = (-PathSamples[Along].Right * 0.86f + FVector::UpVector * 0.34f).GetSafeNormal();
        for (int32 Across = 0; Across < AcrossCount; ++Across)
        {
            Normals[Along * AcrossCount + Across] = FacingTrackNormal;
        }
    }

    CliffMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    UE_LOG(LogTemp, Display, TEXT("Built Yarlung cliff mesh: %d vertices, %d triangles"), Vertices.Num(), Triangles.Num() / 3);
}

void AYarlungCliffActor::ApplyMaterial()
{
    UMaterialInterface* CliffMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_YarlungCliffRock.M_YarlungCliffRock"));
    if (!CliffMaterial)
    {
        CliffMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_YarlungLandscapeGround.M_YarlungLandscapeGround"));
    }

    if (CliffMaterial)
    {
        CliffMesh->SetMaterial(0, CliffMaterial);
    }
}
