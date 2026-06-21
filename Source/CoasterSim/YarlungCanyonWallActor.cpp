#include "YarlungCanyonWallActor.h"

#include "YarlungTerrainProfile.h"
#include "YarlungTrackCsv.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

namespace
{
float Hash01(float X, float Y)
{
    const float Value = FMath::Sin(X * 12.9898f + Y * 78.233f) * 43758.5453f;
    return FMath::Frac(Value);
}

float Smooth01(float Value)
{
    const float T = FMath::Clamp(Value, 0.0f, 1.0f);
    return T * T * (3.0f - 2.0f * T);
}

FVector SampleForward(const TArray<FYarlungCanyonWallTrackSample>& Samples, int32 Index)
{
    const int32 Prev = FMath::Max(0, Index - 1);
    const int32 Next = FMath::Min(Samples.Num() - 1, Index + 1);
    FVector Forward = Samples[Next].Position - Samples[Prev].Position;
    Forward.Z = 0.0f;
    return Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal();
}

FVector SampleRight(const TArray<FYarlungCanyonWallTrackSample>& Samples, int32 Index)
{
    const FVector Right = FVector::CrossProduct(FVector::UpVector, SampleForward(Samples, Index)).GetSafeNormal();
    return Right.IsNearlyZero() ? FVector::RightVector : Right;
}

void AddSingleSidedQuad(TArray<int32>& Triangles, int32 A, int32 B, int32 C, int32 D)
{
    Triangles.Append({ A, C, B, B, C, D });
}

FLinearColor ForestColor(float Along01, float Across01, float Noise)
{
    const FLinearColor ShadowForest(0.016f, 0.070f, 0.030f, 1.0f);
    const FLinearColor DampCanopy(0.036f, 0.145f, 0.058f, 1.0f);
    const FLinearColor MossRock(0.070f, 0.098f, 0.070f, 1.0f);
    FLinearColor Color = FMath::Lerp(ShadowForest, DampCanopy, Smooth01(0.26f + Noise * 0.62f));
    Color = FMath::Lerp(Color, MossRock, Smooth01((Across01 - 0.64f) / 0.36f) * 0.35f);
    const float RavineShade = FMath::Pow(FMath::Clamp(0.5f + 0.5f * FMath::Sin(Along01 * 37.0f + Across01 * 8.0f), 0.0f, 1.0f), 5.0f);
    return FMath::Lerp(Color, FLinearColor(0.010f, 0.030f, 0.020f, 1.0f), RavineShade * 0.18f);
}

FLinearColor WetCliffColor(float Along01, float Height01, float Noise)
{
    const FLinearColor WetRock(0.055f, 0.063f, 0.058f, 1.0f);
    const FLinearColor WeatheredRock(0.105f, 0.112f, 0.098f, 1.0f);
    const FLinearColor Lichen(0.058f, 0.100f, 0.062f, 1.0f);
    FLinearColor Color = FMath::Lerp(WetRock, WeatheredRock, Smooth01(Noise * 0.72f + Height01 * 0.18f));
    const float ForestStain = Smooth01((0.36f - Height01) / 0.24f) * (0.35f + 0.25f * Noise);
    Color = FMath::Lerp(Color, Lichen, ForestStain);
    const float Gully = FMath::Pow(FMath::Clamp(0.5f + 0.5f * FMath::Sin(Along01 * 31.0f - Height01 * 13.0f), 0.0f, 1.0f), 6.0f);
    return FMath::Lerp(Color, FLinearColor(0.018f, 0.027f, 0.024f, 1.0f), Gully * 0.34f);
}
}

AYarlungCanyonWallActor::AYarlungCanyonWallActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    ForestApronMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ForestApronMesh"));
    ForestApronMesh->SetupAttachment(SceneRoot);
    ForestApronMesh->bUseAsyncCooking = true;
    ForestApronMesh->SetCastShadow(true);
    ForestApronMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    WetCliffMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WetCliffMesh"));
    WetCliffMesh->SetupAttachment(SceneRoot);
    WetCliffMesh->bUseAsyncCooking = true;
    WetCliffMesh->SetCastShadow(true);
    WetCliffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AYarlungCanyonWallActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildCanyonWalls();
}

void AYarlungCanyonWallActor::BeginPlay()
{
    Super::BeginPlay();
    RebuildCanyonWalls();

    if (FParse::Param(FCommandLine::Get(), TEXT("YarlungHideCanyonWalls")))
    {
        SetActorHiddenInGame(true);
        ForestApronMesh->SetVisibility(false, true);
        WetCliffMesh->SetVisibility(false, true);
    }
}

void AYarlungCanyonWallActor::RebuildCanyonWalls()
{
    TArray<FYarlungCanyonWallTrackSample> TrackSamples;
    TArray<uint16> HeightData;
    if (!LoadOutboundTrack(TrackSamples) || !LoadHeightmap(HeightData))
    {
        ForestApronMesh->ClearAllMeshSections();
        WetCliffMesh->ClearAllMeshSections();
        return;
    }

    BuildForestApron(TrackSamples, HeightData);
    BuildWetCliff(TrackSamples, HeightData);
    ApplyMaterials();
}

bool AYarlungCanyonWallActor::LoadOutboundTrack(TArray<FYarlungCanyonWallTrackSample>& OutSamples) const
{
    const FString Path = FPaths::ProjectContentDir() / TrackCsvRelativePath;
    TArray<FYarlungTrackRow> Rows;
    FString Error;
    if (!YarlungTrackCsv::Load(Path, Rows, &Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung canyon-wall track CSV: %s"), *Error);
        return false;
    }

    OutSamples.Reset();
    for (int32 Index = 0; Index < Rows.Num(); Index += 2)
    {
        const FYarlungTrackRow& Row = Rows[Index];
        if (!Row.Section.Equals(TEXT("Station"), ESearchCase::IgnoreCase))
        {
            FYarlungCanyonWallTrackSample Sample;
            Sample.Position = Row.PositionCm;
            Sample.TerrainZCm = Row.TerrainZCm;
            OutSamples.Add(Sample);
        }
    }

    if (OutSamples.Num() < 8)
    {
        UE_LOG(LogTemp, Error, TEXT("Too few route samples for Yarlung canyon walls: %d"), OutSamples.Num());
        return false;
    }

    return true;
}

bool AYarlungCanyonWallActor::LoadHeightmap(TArray<uint16>& OutHeightData) const
{
    const FString Path = FPaths::ProjectContentDir() / HeightmapRelativePath;
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung heightmap for canyon walls: %s"), *Path);
        return false;
    }

    const int32 HeightmapSize = YarlungTerrain::Config().GridSize;
    const int32 ExpectedByteCount = HeightmapSize * HeightmapSize * sizeof(uint16);
    if (RawBytes.Num() != ExpectedByteCount)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung canyon-wall heightmap has %d bytes; expected %d"), RawBytes.Num(), ExpectedByteCount);
        return false;
    }

    OutHeightData.SetNumUninitialized(HeightmapSize * HeightmapSize);
    FMemory::Memcpy(OutHeightData.GetData(), RawBytes.GetData(), RawBytes.Num());
    return true;
}

float AYarlungCanyonWallActor::SampleHeightCm(const TArray<uint16>& HeightData, float X, float Y) const
{
    const YarlungTerrain::FConfig& Tc = YarlungTerrain::Config();
    const int32 HeightmapSize = Tc.GridSize;
    const float GridX = FMath::Clamp((X - Tc.MinXCm) / (Tc.MaxXCm - Tc.MinXCm) * (HeightmapSize - 1), 0.0f, static_cast<float>(HeightmapSize - 1));
    const float GridY = FMath::Clamp((Y - Tc.MinYCm) / (Tc.MaxYCm - Tc.MinYCm) * (HeightmapSize - 1), 0.0f, static_cast<float>(HeightmapSize - 1));
    const int32 X0 = FMath::FloorToInt(GridX);
    const int32 Y0 = FMath::FloorToInt(GridY);
    const int32 X1 = FMath::Min(HeightmapSize - 1, X0 + 1);
    const int32 Y1 = FMath::Min(HeightmapSize - 1, Y0 + 1);
    const float Tx = GridX - X0;
    const float Ty = GridY - Y0;

    const auto At = [&HeightData, HeightmapSize](int32 SampleX, int32 SampleY)
    {
        return YarlungTerrain::HeightValueToCm(HeightData[SampleY * HeightmapSize + SampleX]);
    };

    const float A = FMath::Lerp(At(X0, Y0), At(X1, Y0), Tx);
    const float B = FMath::Lerp(At(X0, Y1), At(X1, Y1), Tx);
    return FMath::Lerp(A, B, Ty);
}

void AYarlungCanyonWallActor::BuildForestApron(const TArray<FYarlungCanyonWallTrackSample>& TrackSamples, const TArray<uint16>& HeightData)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const TArray<float> AcrossValues = { 0.0f, 0.12f, 0.26f, 0.42f, 0.60f, 0.78f, 1.0f };
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? 1.0f : -1.0f;
        const int32 SideBase = Vertices.Num();
        for (int32 Along = 0; Along < TrackSamples.Num(); ++Along)
        {
            const FVector Forward = SampleForward(TrackSamples, Along);
            const FVector Right = SampleRight(TrackSamples, Along);
            const float Along01 = static_cast<float>(Along) / static_cast<float>(FMath::Max(1, TrackSamples.Num() - 1));
            for (int32 Across = 0; Across < AcrossValues.Num(); ++Across)
            {
                const float Across01 = AcrossValues[Across];
                const float Noise = Hash01(Along * 0.73f + Side * 11.0f, Across * 3.1f);
                const float LateralCm = FMath::Lerp(9000.0f, 76000.0f, Across01)
                    + FMath::Sin(Along01 * 41.0f + Across01 * 7.0f + Side * 0.8f) * FMath::Lerp(1400.0f, 6200.0f, Across01);
                const FVector2D TrackXY(TrackSamples[Along].Position.X, TrackSamples[Along].Position.Y);
                const FVector2D RightXY(Right.X, Right.Y);
                const FVector2D ForwardXY(Forward.X, Forward.Y);
                const FVector2D XY = TrackXY + RightXY * Side * LateralCm + ForwardXY * (Noise - 0.5f) * 1800.0f;
                const float BaseHeight = SampleHeightCm(HeightData, XY.X, XY.Y);
                const float CanopyLift = FMath::Lerp(1200.0f, 7400.0f, Across01)
                    + 1900.0f * Smooth01(FMath::Sin(Along01 * 19.0f + Side * 1.7f) * 0.5f + 0.5f);
                Vertices.Add(FVector(XY.X, XY.Y, BaseHeight + CanopyLift));
                Normals.Add((FVector::UpVector - Right * Side * 0.18f).GetSafeNormal());
                UVs.Add(FVector2D(Along01 * 12.0f, Across01));
                Colors.Add(ForestColor(Along01, Across01, Noise));
                Tangents.Add(FProcMeshTangent(Forward, false));
            }
        }

        const int32 Stride = AcrossValues.Num();
        for (int32 Along = 0; Along < TrackSamples.Num() - 1; ++Along)
        {
            for (int32 Across = 0; Across < AcrossValues.Num() - 1; ++Across)
            {
                const int32 A = SideBase + Along * Stride + Across;
                const int32 B = SideBase + (Along + 1) * Stride + Across;
                const int32 C = SideBase + Along * Stride + Across + 1;
                const int32 D = SideBase + (Along + 1) * Stride + Across + 1;
                AddSingleSidedQuad(Triangles, A, B, C, D);
            }
        }
    }

    ForestApronMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

void AYarlungCanyonWallActor::BuildWetCliff(const TArray<FYarlungCanyonWallTrackSample>& TrackSamples, const TArray<uint16>& HeightData)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const TArray<float> HeightValues = { 0.0f, 0.10f, 0.22f, 0.36f, 0.52f, 0.70f, 0.86f, 1.0f };
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? 1.0f : -1.0f;
        const int32 SideBase = Vertices.Num();
        for (int32 Along = 0; Along < TrackSamples.Num(); ++Along)
        {
            const FVector Forward = SampleForward(TrackSamples, Along);
            const FVector Right = SampleRight(TrackSamples, Along);
            const float Along01 = static_cast<float>(Along) / static_cast<float>(FMath::Max(1, TrackSamples.Num() - 1));
            const float Buttress = 0.5f + 0.5f * FMath::Sin(Along01 * 17.0f + Side * 1.2f);
            for (int32 H = 0; H < HeightValues.Num(); ++H)
            {
                const float Height01 = HeightValues[H];
                const float Noise = Hash01(Along * 1.17f + Side * 19.0f, H * 5.3f);
                const float LateralCm = FMath::Lerp(52000.0f, 118000.0f, Height01)
                    + Buttress * FMath::Lerp(6200.0f, -6200.0f, Height01)
                    + FMath::Sin(Along01 * 29.0f - Height01 * 8.0f + Side * 0.4f) * 5200.0f;
                const FVector2D TrackXY(TrackSamples[Along].Position.X, TrackSamples[Along].Position.Y);
                const FVector2D RightXY(Right.X, Right.Y);
                const FVector2D XY = TrackXY + RightXY * Side * LateralCm;
                const float BaseHeight = SampleHeightCm(HeightData, XY.X, XY.Y);
                const float VerticalCm = FMath::Lerp(4200.0f, 52000.0f, Height01)
                    + Buttress * FMath::Lerp(2600.0f, 12400.0f, Height01)
                    - FMath::Pow(FMath::Clamp(0.5f + 0.5f * FMath::Sin(Along01 * 31.0f + Side * 0.6f), 0.0f, 1.0f), 5.0f) * 8200.0f * Height01;
                Vertices.Add(FVector(XY.X, XY.Y, BaseHeight + VerticalCm));
                Normals.Add((FVector::UpVector * 0.55f - Right * Side * 0.72f).GetSafeNormal());
                UVs.Add(FVector2D(Along01 * 10.0f, Height01));
                Colors.Add(WetCliffColor(Along01, Height01, Noise));
                Tangents.Add(FProcMeshTangent(Forward, false));
            }
        }

        const int32 Stride = HeightValues.Num();
        for (int32 Along = 0; Along < TrackSamples.Num() - 1; ++Along)
        {
            for (int32 H = 0; H < HeightValues.Num() - 1; ++H)
            {
                const int32 A = SideBase + Along * Stride + H;
                const int32 B = SideBase + (Along + 1) * Stride + H;
                const int32 C = SideBase + Along * Stride + H + 1;
                const int32 D = SideBase + (Along + 1) * Stride + H + 1;
                AddSingleSidedQuad(Triangles, A, B, C, D);
            }
        }
    }

    WetCliffMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

void AYarlungCanyonWallActor::ApplyMaterials()
{
    UMaterialInterface* WallMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_YarlungCanyonWall.M_YarlungCanyonWall"));
    if (!WallMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing required Yarlung canyon wall material: /Game/Generated/Materials/M_YarlungCanyonWall.M_YarlungCanyonWall"));
        return;
    }

    ForestApronMesh->SetMaterial(0, WallMaterial);
    WetCliffMesh->SetMaterial(0, WallMaterial);
    UE_LOG(
        LogTemp,
        Display,
        TEXT("Built Yarlung authored canyon walls: forest_vertices=%d cliff_vertices=%d"),
        ForestApronMesh->GetProcMeshSection(0) ? ForestApronMesh->GetProcMeshSection(0)->ProcVertexBuffer.Num() : 0,
        WetCliffMesh->GetProcMeshSection(0) ? WetCliffMesh->GetProcMeshSection(0)->ProcVertexBuffer.Num() : 0);
}
