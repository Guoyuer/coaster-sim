#pragma once

#include "CoreMinimal.h"
#include "CoasterRideCapture.h"
#include "GameFramework/Actor.h"
#include "CoasterRideActor.generated.h"

class UCameraComponent;
class UCoasterTrackComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;

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
    void StartRideFromCommandLine(float DefaultTrackRatio, float DefaultSpeedMps);

protected:
    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<UCoasterTrackComponent> TrackSpline;

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> TrainRoot;

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

    UPROPERTY(VisibleAnywhere, Category = "Lighting")
    TObjectPtr<UVolumetricCloudComponent> VolumetricClouds;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> LeftRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> RightRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> CenterSpine;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> LeftGuardRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> RightGuardRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> Ties;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> TrackBraces;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> Supports;

    UPROPERTY(EditAnywhere, Category = "Track", meta = (ClampMin = "0.0"))
    float RailGaugeCm = 170.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float LiftTargetSpeedMps = 12.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float LaunchTargetSpeedMps = 34.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float BrakeTargetSpeedMps = 8.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float PoweredCruiseTargetSpeedMps = 28.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float PoweredTurnaroundTargetSpeedMps = 12.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float PoweredDriveMaxAccelMps2 = 4.2f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float PoweredBrakeMaxAccelMps2 = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Ride", meta = (ClampMin = "0.0"))
    float NumericalStallFloorMps = 0.2f;

    // Resistance / limit knobs for tuning ride feel (were hardcoded in AdvanceRide).
    UPROPERTY(EditAnywhere, Category = "Ride|Resistance", meta = (ClampMin = "0.0"))
    float AeroDragCoefficient = 0.000015f;

    UPROPERTY(EditAnywhere, Category = "Ride|Resistance", meta = (ClampMin = "0.0"))
    float RollingResistanceCms2 = 18.0f;

    UPROPERTY(EditAnywhere, Category = "Ride|Resistance", meta = (ClampMin = "0.0"))
    float MaxSpeedMps = 56.0f;

private:
    void RebuildSpline();
    void RebuildVisuals();
    void AdvanceRide(float DeltaSeconds);
    float ComputeAdvancedTrackRatio(float TrackRatio, float SpeedMps, float StartSeconds) const;
    void PositionRideForCommandLineSeconds(float StartSeconds);
    void UpdateFirstPersonCamera();
    void SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const;

    FCoasterTelemetry Telemetry;
    FVector LastVelocityCms = FVector::ZeroVector;
    float CurrentDistanceCm = 0.0f;
    float CurrentSpeedCms = 0.0f;
    float TrackLengthCm = 1.0f;

    CoasterRideCapture::FState BatchCapture;
};
