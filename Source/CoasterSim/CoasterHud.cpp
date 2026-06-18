#include "CoasterHud.h"

#include "CoasterRideActor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

void ACoasterHud::DrawHUD()
{
    Super::DrawHUD();

    const ACoasterRideActor* Ride = nullptr;
    for (TActorIterator<ACoasterRideActor> It(GetWorld()); It; ++It)
    {
        Ride = *It;
        break;
    }

    if (!Ride || !Canvas)
    {
        return;
    }

    const FCoasterTelemetry& Telemetry = Ride->GetTelemetry();
    const FString Lines = FString::Printf(
        TEXT("COASTER SIM\nSection: %s\nSpeed: %.1f m/s  %.0f km/h\nHeight: %.1f m\nTrack: %.1f m\nVertical G: %.2f\nLateral G: %.2f\nLongitudinal G: %.2f"),
        *Telemetry.SectionName.ToString(),
        Telemetry.SpeedMps,
        Telemetry.SpeedMps * 3.6f,
        Telemetry.HeightMeters,
        Telemetry.TrackDistanceMeters,
        Telemetry.VerticalG,
        Telemetry.LateralG,
        Telemetry.LongitudinalG);

    FCanvasTextItem TextItem(FVector2D(28.0f, 28.0f), FText::FromString(Lines), GEngine->GetMediumFont(), FLinearColor(0.9f, 0.94f, 0.96f, 0.78f));
    TextItem.EnableShadow(FLinearColor::Black);
    TextItem.Scale = FVector2D(0.82f, 0.82f);
    Canvas->DrawItem(TextItem);
}
