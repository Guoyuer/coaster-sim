#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoasterRideActor.generated.h"

class UCameraComponent;
class UInstancedStaticMeshComponent;
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

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> LeftRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> RightRail;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> Ties;

    UPROPERTY(VisibleAnywhere, Category = "Track Visuals")
    TObjectPtr<UInstancedStaticMeshComponent> Supports;

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
    void AdvanceRide(float DeltaSeconds);
    void SampleFrame(float DistanceCm, FVector& OutLocation, FRotator& OutRotation, FVector& OutForward, FVector& OutRight, FVector& OutUp) const;
    FName GetSectionName(float TrackRatio) const;

    FCoasterTelemetry Telemetry;
    FVector LastVelocityCms = FVector::ZeroVector;
    float CurrentDistanceCm = 0.0f;
    float CurrentSpeedCms = 0.0f;
    float TrackLengthCm = 1.0f;
};
