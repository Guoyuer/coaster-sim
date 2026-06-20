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

float Saturate(float Value)
{
    return FMath::Clamp(Value, 0.0f, 1.0f);
}

float RiverEnergy(const TArray<FYarlungRiverSample>& Samples, int32 Index)
{
    const int32 Prev = FMath::Max(Index - 1, 0);
    const int32 Next = FMath::Min(Index + 1, Samples.Num() - 1);
    const FVector Delta = Samples[Next].Center - Samples[Prev].Center;
    const float HorizontalDistance = FMath::Max(FVector(Delta.X, Delta.Y, 0.0f).Size(), 1.0f);
    const float Slope = FMath::Abs(Delta.Z) / HorizontalDistance;

    const FVector PrevForward = SampleForward(Samples, Prev);
    const FVector NextForward = SampleForward(Samples, Next);
    const float Curvature = 1.0f - FMath::Clamp(FVector::DotProduct(PrevForward, NextForward), -1.0f, 1.0f);

    const float BrokenSurface = 0.5f + 0.5f * FMath::Sin(Samples[Index].Flow * 47.0f + Index * 0.37f);
    return Saturate(Slope * 70.0f + Curvature * 4.0f + BrokenSurface * 0.22f);
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

    const TArray<float> AcrossValues = { -1.0f, -0.88f, -0.72f, -0.52f, -0.30f, 0.0f, 0.30f, 0.52f, 0.72f, 0.88f, 1.0f };
    Vertices.Reserve(Samples.Num() * AcrossValues.Num());

    for (int32 Along = 0; Along < Samples.Num(); ++Along)
    {
        const FYarlungRiverSample& Sample = Samples[Along];
        const FVector Forward = SampleForward(Samples, Along);
        const FVector Right = SampleRight(Samples, Along);
        const float Energy = RiverEnergy(Samples, Along);
        for (int32 Across = 0; Across < AcrossValues.Num(); ++Across)
        {
            const float AcrossValue = AcrossValues[Across];
            const float EdgeWeight = FMath::Abs(AcrossValue);
            const float CenterWeight = 1.0f - FMath::Abs(AcrossValue);
            const float BankNoise = 0.018f * FMath::Sin(Sample.Flow * 67.0f + AcrossValue * 5.9f)
                + 0.011f * FMath::Sin(Sample.Flow * 151.0f - AcrossValue * 4.1f);
            const float BankedAcross = AcrossValue + FMath::Sign(AcrossValue) * BankNoise * Saturate((EdgeWeight - 0.55f) / 0.45f);
            const float Ripple = (3.0f + CenterWeight * 11.0f) * FMath::Sin(Sample.Flow * 92.0f + AcrossValue * 3.2f)
                + CenterWeight * 5.0f * FMath::Sin(Sample.Flow * 211.0f - AcrossValue * 6.4f)
                + Energy * (5.0f + CenterWeight * 14.0f) * FMath::Sin(Sample.Flow * 383.0f + AcrossValue * 11.0f);
            Vertices.Add(Sample.Center + Right * (BankedAcross * Sample.HalfWidthCm) + FVector(0.0f, 0.0f, Ripple));
            const float FlowChop = Energy * (0.05f * FMath::Cos(Sample.Flow * 383.0f + AcrossValue * 11.0f)
                + 0.035f * FMath::Cos(Sample.Flow * 211.0f - AcrossValue * 6.4f));
            const float CrossChop = Energy * (0.08f * FMath::Cos(Sample.Flow * 383.0f + AcrossValue * 11.0f)
                + 0.035f * FMath::Cos(Sample.Flow * 92.0f + AcrossValue * 3.2f));
            Normals.Add((FVector::UpVector - Forward * FlowChop - Right * CrossChop).GetSafeNormal());
            UVs.Add(FVector2D(Sample.Flow * 28.0f, AcrossValue * 0.5f + 0.5f));
            const float BankFade = Saturate((EdgeWeight - 0.72f) / 0.28f);
            const float Alpha = FMath::Lerp(0.76f, 0.24f, BankFade);
            const float MilkyWater = Saturate(Energy * (0.62f + CenterWeight * 0.42f));
            Colors.Add(FLinearColor(
                FMath::Lerp(0.12f + CenterWeight * 0.24f, 0.62f, MilkyWater),
                FMath::Lerp(0.42f + CenterWeight * 0.22f, 0.80f, MilkyWater),
                FMath::Lerp(0.46f + CenterWeight * 0.20f, 0.72f, MilkyWater),
                FMath::Max(Alpha, 0.42f + MilkyWater * 0.20f)));
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

    const TArray<float> FoamLanes = { -0.82f, -0.62f, -0.36f, -0.12f, 0.18f, 0.46f, 0.72f };
    for (int32 Lane = 0; Lane < FoamLanes.Num(); ++Lane)
    {
        for (int32 Along = 0; Along < Samples.Num(); ++Along)
        {
            const FYarlungRiverSample& Sample = Samples[Along];
            const FVector Forward = SampleForward(Samples, Along);
            const FVector Right = SampleRight(Samples, Along);
            const float Energy = RiverEnergy(Samples, Along);
            const float LaneNoise = FMath::Sin(Sample.Flow * 92.0f + Lane * 1.7f)
                + 0.55f * FMath::Sin(Sample.Flow * 233.0f - Lane * 0.8f);
            const float Lateral = (FoamLanes[Lane] + LaneNoise * 0.030f) * Sample.HalfWidthCm;
            const float EdgeWeight = FMath::Abs(FoamLanes[Lane]);
            const float HalfWidth = FMath::Lerp(72.0f, 210.0f, EdgeWeight)
                + Energy * FMath::Lerp(36.0f, 190.0f, EdgeWeight)
                + 44.0f * FMath::Sin(Sample.Flow * 57.0f + Lane * 0.9f);
            const FVector Center = Sample.Center + Right * Lateral + FVector(0.0f, 0.0f, 34.0f);
            Vertices.Add(Center - Right * HalfWidth);
            Vertices.Add(Center + Right * HalfWidth);
            Normals.Add(FVector::UpVector);
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(Sample.Flow * 34.0f, 0.0f));
            UVs.Add(FVector2D(Sample.Flow * 34.0f, 1.0f));
            const float Alpha = Saturate(FMath::Lerp(0.46f, 0.86f, EdgeWeight) + Energy * 0.28f);
            Colors.Add(FLinearColor(0.70f, 0.82f, 0.77f, Alpha * 0.62f));
            Colors.Add(FLinearColor(0.93f, 0.98f, 0.91f, Alpha));
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

    if (!WaterMaterial || !FoamMaterial)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Missing required Yarlung river materials: water=%s foam=%s"),
            WaterMaterial ? TEXT("ok") : TEXT("missing"),
            FoamMaterial ? TEXT("ok") : TEXT("missing"));
        return;
    }

    WaterMesh->SetMaterial(0, WaterMaterial);
    FoamMesh->SetMaterial(0, FoamMaterial);

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
