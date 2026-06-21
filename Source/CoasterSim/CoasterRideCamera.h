#pragma once

#include "CoreMinimal.h"

class UCameraComponent;

namespace CoasterRideCamera
{
void Configure(UCameraComponent* RideCamera);
void ApplyRigTransform(UCameraComponent* RideCamera);
}
