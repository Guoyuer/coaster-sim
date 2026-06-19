#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoasterRideActor.generated.h"

class UCameraComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class USplineComponent;

USTRUCT(BlueprintType)
struct FCoasterTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    float SpeedMps = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    float HeightMeters = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    float TrackDistanceMeters = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    float VerticalG = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    float LateralG = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    float LongitudinalG = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Coaster")
    FName SectionName = TEXT("Station");
};

struct FEnvironmentRiverSample
{
    FVector Center = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    FVector Right = FVector::RightVector;
    float Ratio = 0.0f;
};

UCLASS()
class COASTERSIM_API ACoasterRideActor : public AActor
{
    GENERATED_BODY()

public:
    ACoasterRideActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    const FCoasterTelemetry& GetTelemetry() const { return Telemetry; }
    void StartRideAt(float TrackRatio, float SpeedMps);

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USplineComponent> TrackSpline;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> TrainRoot;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UStaticMeshComponent> TrainBody;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UCameraComponent> RideCamera;

    UPROPERTY(VisibleAnywhere, Category = "Lighting")
    TObjectPtr<USkyLightComponent> SkyLight;

    UPROPERTY(VisibleAnywhere, Category = "Lighting")
    TObjectPtr<UDirectionalLightComponent> SunLight;

    UPROPERTY(VisibleAnywhere, Category = "Lighting")
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    UPROPERTY(VisibleAnywhere, Category = "Lighting")
    TObjectPtr<UExponentialHeightFogComponent> ValleyFog;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> LeftRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> RightRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> Ties;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> Supports;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UInstancedStaticMeshComponent> RiverSurface;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UInstancedStaticMeshComponent> Rapids;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UInstancedStaticMeshComponent> MistBands;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UInstancedStaticMeshComponent> BoulderOutcrops;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UProceduralMeshComponent> RiverRibbonMesh;

    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UProceduralMeshComponent> FoamRibbonMesh;

    UPROPERTY(EditAnywhere, Category = "Track")
    TArray<FVector> ControlPoints;

    UPROPERTY(EditAnywhere, Category = "Track", meta = (ClampMin = "0.0"))
    float RailGaugeCm = 170.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float LiftTargetSpeedMps = 7.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float LaunchTargetSpeedMps = 34.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float BrakeTargetSpeedMps = 8.0f;

private:
    void EnsureDefaultTrack();
    void RebuildSpline();
    void RebuildVisuals();
    void RebuildEnvironment();
    void ClearEnvironmentVisuals();
    void BuildBoulderOutcrops();
    void BuildRiverEffects();
    void BuildRiverSamples(TArray<FEnvironmentRiverSample>& OutSamples, int32 SampleCount) const;
    void BuildRiverRibbon(const TArray<FEnvironmentRiverSample>& Samples, float RiverHalfWidth);
    void BuildFoamRibbon(const TArray<FEnvironmentRiverSample>& Samples, float RiverHalfWidth);
    void BuildRapids(const TArray<FEnvironmentRiverSample>& Samples, float RiverHalfWidth);
    void ApplyVisualMaterials();
    void AdvanceRide(float DeltaSeconds);
    void UpdateFirstPersonCamera();
    void SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const;
    FName GetSectionName(float TrackRatio) const;

    FCoasterTelemetry Telemetry;
    FVector LastVelocityCms = FVector::ZeroVector;
    float CurrentDistanceCm = 0.0f;
    float CurrentSpeedCms = 0.0f;
    float TrackLengthCm = 1.0f;

};
