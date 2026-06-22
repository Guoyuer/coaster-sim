#pragma once

#include "CoreMinimal.h"

class FYarlungRiverField;
class UStaticMesh;

namespace YarlungRiverSurfaceBuilder
{
#if WITH_EDITOR
UStaticMesh* BuildStaticMesh(const FYarlungRiverField& RiverField);
#endif
}
