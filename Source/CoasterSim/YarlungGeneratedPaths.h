#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace YarlungGeneratedPaths
{
inline constexpr const TCHAR* CorridorDirRelative = TEXT("Generated/YarlungLandscape");
inline constexpr const TCHAR* TrackCsvRelative = TEXT("Generated/YarlungLandscape/YarlungTrack.csv");
inline constexpr const TCHAR* RiverCsvRelative = TEXT("Generated/YarlungLandscape/YarlungRiver.csv");
inline constexpr const TCHAR* HeightmapRelative = TEXT("Generated/YarlungLandscape/YarlungTsangpo_1009.r16");

inline constexpr const TCHAR* CoasterTintMaterialObjectPath = TEXT("/Game/Generated/Materials/M_CoasterTint.M_CoasterTint");
inline constexpr const TCHAR* MeshTerrainMaterialObjectPath = TEXT("/Game/Generated/Materials/M_YarlungMeshTerrain.M_YarlungMeshTerrain");

inline constexpr const TCHAR* CorridorTerrainMeshPackagePath = TEXT("/Game/Generated/YarlungLandscape/SM_YarlungCorridorTerrain");
inline constexpr const TCHAR* CorridorTerrainMeshAssetName = TEXT("SM_YarlungCorridorTerrain");
inline constexpr const TCHAR* CorridorTerrainMeshObjectPath = TEXT("/Game/Generated/YarlungLandscape/SM_YarlungCorridorTerrain.SM_YarlungCorridorTerrain");

inline constexpr const TCHAR* RiverSurfaceMeshPackagePath = TEXT("/Game/Generated/YarlungLandscape/SM_YarlungRiverSurface");
inline constexpr const TCHAR* RiverSurfaceMeshAssetName = TEXT("SM_YarlungRiverSurface");

inline constexpr const TCHAR* CorridorMapPackagePath = TEXT("/Game/Generated/YarlungLandscape/YarlungLandscape_Level");

inline FString ProjectContentFile(const TCHAR* RelativePath)
{
    return FPaths::ProjectContentDir() / RelativePath;
}
}
