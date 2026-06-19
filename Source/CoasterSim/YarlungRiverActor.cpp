#include "YarlungRiverActor.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"

namespace
{
void AddDoubleSidedQuad(TArray<int32>& Triangles, int32 A, int32 B, int32 C, int32 D)
{
    Triangles.Append({ A, C, B, B, C, D, A, B, C, B, D, C });
}

FVector SampleForward(const TArray<FYarlungRiverSample>& Samples, int32 Index)
{
    const int32 Prev = FMath::Max(Index - 1, 0);
    const int32 Next = FMath::Min(Index + 1, Samples.Num() - 1);
    FVector Forward = Samples[Next].Center - Samples[Prev].Center;
    Forward.Z = 0.0f;
    if (Forward.IsNearlyZero())
    {
        return FVector::ForwardVector;
    }
    return Forward.GetSafeNormal();
}

FVector SampleRight(const TArray<FYarlungRiverSample>& Samples, int32 Index)
{
    return FVector::CrossProduct(FVector::UpVector, SampleForward(Samples, Index)).GetSafeNormal();
}

bool ParseRiverRow(const FString& Line, FYarlungRiverSample& OutSample)
{
    TArray<FString> Columns;
    Line.ParseIntoArray(Columns, TEXT(","), true);
    if (Columns.Num() < 6)
    {
        return false;
    }

    OutSample.Center.X = FCString::Atof(*Columns[1]);
    OutSample.Center.Y = FCString::Atof(*Columns[2]);
    OutSample.Center.Z = FCString::Atof(*Columns[3]);
    OutSample.HalfWidthCm = FCString::Atof(*Columns[4]);
    OutSample.Flow = FCString::Atof(*Columns[5]);
    return true;
}
}

AYarlungRiverActor::AYarlungRiverActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    WaterMesh->SetupAttachment(SceneRoot);
    WaterMesh->bUseAsyncCooking = true;
    WaterMesh->SetCastShadow(false);

    FoamMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FoamMesh"));
    FoamMesh->SetupAttachment(SceneRoot);
    FoamMesh->bUseAsyncCooking = true;
    FoamMesh->SetCastShadow(false);
}

void AYarlungRiverActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildRiver();
}

void AYarlungRiverActor::BeginPlay()
{
    Super::BeginPlay();
    RebuildRiver();
}

void AYarlungRiverActor::RebuildRiver()
{
    TArray<FYarlungRiverSample> Samples;
    if (!LoadRiverSamples(Samples))
    {
        WaterMesh->ClearAllMeshSections();
        FoamMesh->ClearAllMeshSections();
        return;
    }

    BuildWaterMesh(Samples);
    BuildFoamMesh(Samples);
    ApplyMaterials();
}

bool AYarlungRiverActor::LoadRiverSamples(TArray<FYarlungRiverSample>& OutSamples) const
{
    const FString Path = FPaths::ProjectContentDir() / RiverCsvRelativePath;
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("Unable to read Yarlung river CSV: %s"), *Path);
        return false;
    }

    OutSamples.Reset();
    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        FYarlungRiverSample Sample;
        if (ParseRiverRow(Lines[LineIndex], Sample))
        {
            OutSamples.Add(Sample);
        }
    }

    if (OutSamples.Num() < 4)
    {
        UE_LOG(LogTemp, Error, TEXT("Yarlung river CSV has too few valid samples: %d"), OutSamples.Num());
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT("Loaded Yarlung river CSV: %d samples"), OutSamples.Num());
    return true;
}

void AYarlungRiverActor::BuildWaterMesh(const TArray<FYarlungRiverSample>& Samples)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const TArray<float> AcrossValues = { -1.0f, -0.58f, 0.0f, 0.58f, 1.0f };
    Vertices.Reserve(Samples.Num() * AcrossValues.Num());

    for (int32 Along = 0; Along < Samples.Num(); ++Along)
    {
        const FYarlungRiverSample& Sample = Samples[Along];
        const FVector Forward = SampleForward(Samples, Along);
        const FVector Right = SampleRight(Samples, Along);
        for (int32 Across = 0; Across < AcrossValues.Num(); ++Across)
        {
            const float AcrossValue = AcrossValues[Across];
            const float CenterWeight = 1.0f - FMath::Abs(AcrossValue);
            const float Ripple = 8.0f * FMath::Sin(Sample.Flow * 72.0f + AcrossValue * 3.2f);
            Vertices.Add(Sample.Center + Right * (AcrossValue * Sample.HalfWidthCm) + FVector(0.0f, 0.0f, Ripple));
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(Sample.Flow * 28.0f, AcrossValue * 0.5f + 0.5f));
            Colors.Add(FLinearColor(
                0.18f + CenterWeight * 0.12f,
                0.46f + CenterWeight * 0.18f,
                0.50f + CenterWeight * 0.20f,
                0.74f));
            Tangents.Add(FProcMeshTangent(Forward, false));
        }
    }

    for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
    {
        for (int32 Across = 0; Across < AcrossValues.Num() - 1; ++Across)
        {
            const int32 A = Along * AcrossValues.Num() + Across;
            const int32 B = (Along + 1) * AcrossValues.Num() + Across;
            const int32 C = Along * AcrossValues.Num() + Across + 1;
            const int32 D = (Along + 1) * AcrossValues.Num() + Across + 1;
            AddDoubleSidedQuad(Triangles, A, B, C, D);
        }
    }

    WaterMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

void AYarlungRiverActor::BuildFoamMesh(const TArray<FYarlungRiverSample>& Samples)
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const TArray<float> FoamLanes = { -0.72f, -0.34f, 0.22f, 0.66f };
    for (int32 Lane = 0; Lane < FoamLanes.Num(); ++Lane)
    {
        for (int32 Along = 0; Along < Samples.Num(); ++Along)
        {
            const FYarlungRiverSample& Sample = Samples[Along];
            const FVector Forward = SampleForward(Samples, Along);
            const FVector Right = SampleRight(Samples, Along);
            const float LaneNoise = FMath::Sin(Sample.Flow * 92.0f + Lane * 1.7f);
            const float Lateral = (FoamLanes[Lane] + LaneNoise * 0.035f) * Sample.HalfWidthCm;
            const float HalfWidth = 115.0f + 38.0f * FMath::Sin(Sample.Flow * 57.0f + Lane * 0.9f);
            const FVector Center = Sample.Center + Right * Lateral + FVector(0.0f, 0.0f, 28.0f);
            Vertices.Add(Center - Right * HalfWidth);
            Vertices.Add(Center + Right * HalfWidth);
            Normals.Add(FVector::UpVector);
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(Sample.Flow * 34.0f, 0.0f));
            UVs.Add(FVector2D(Sample.Flow * 34.0f, 1.0f));
            Colors.Add(FLinearColor(0.74f, 0.84f, 0.78f, 0.68f));
            Colors.Add(FLinearColor(0.92f, 0.98f, 0.90f, 0.76f));
            Tangents.Add(FProcMeshTangent(Forward, false));
            Tangents.Add(FProcMeshTangent(Forward, false));
        }

        const int32 LaneBase = Lane * Samples.Num() * 2;
        for (int32 Along = 0; Along < Samples.Num() - 1; ++Along)
        {
            const int32 A = LaneBase + Along * 2;
            const int32 B = LaneBase + (Along + 1) * 2;
            const int32 C = LaneBase + Along * 2 + 1;
            const int32 D = LaneBase + (Along + 1) * 2 + 1;
            AddDoubleSidedQuad(Triangles, A, B, C, D);
        }
    }

    FoamMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

void AYarlungRiverActor::ApplyMaterials()
{
    UMaterialInterface* WaterMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_YarlungRiverWater.M_YarlungRiverWater"));
    UMaterialInterface* FoamMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_YarlungRiverFoam.M_YarlungRiverFoam"));
    UMaterialInterface* FallbackMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint"));

    if (!WaterMaterial)
    {
        WaterMaterial = FallbackMaterial;
    }
    if (!FoamMaterial)
    {
        FoamMaterial = FallbackMaterial;
    }

    if (WaterMaterial)
    {
        WaterMesh->SetMaterial(0, WaterMaterial);
    }
    if (FoamMaterial)
    {
        FoamMesh->SetMaterial(0, FoamMaterial);
    }

    if (UMaterialInstanceDynamic* WaterDynamic = WaterMesh->CreateAndSetMaterialInstanceDynamic(0))
    {
        WaterDynamic->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.16f, 0.50f, 0.54f, 0.72f));
        WaterDynamic->SetScalarParameterValue(TEXT("Opacity"), 0.72f);
        WaterDynamic->SetScalarParameterValue(TEXT("Roughness"), 0.18f);
        WaterDynamic->SetScalarParameterValue(TEXT("Specular"), 0.75f);
    }

    if (UMaterialInstanceDynamic* FoamDynamic = FoamMesh->CreateAndSetMaterialInstanceDynamic(0))
    {
        FoamDynamic->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.82f, 0.92f, 0.84f, 0.78f));
        FoamDynamic->SetScalarParameterValue(TEXT("Opacity"), 0.82f);
        FoamDynamic->SetScalarParameterValue(TEXT("Roughness"), 0.62f);
        FoamDynamic->SetScalarParameterValue(TEXT("Specular"), 0.20f);
    }
}
