import unreal

PATHS = [
    "/Game/Fab/Megascans/3D/Nordic_Forest_Cliff_Large_xibsff1/Raw/xibsff1_tier_0/StaticMeshes/xibsff1_tier_0",
    "/Game/Fab/Megascans/3D/Quarry_Cliff_uchwaffda/High/uchwaffda_tier_1/StaticMeshes/uchwaffda_tier_1",
    "/Game/Fab/Megascans/3D/Beach_Boulder_uisjbbis/Medium/uisjbbis_tier_2/StaticMeshes/uisjbbis_tier_2",
    "/Game/Fab/Megascans/Plants/Dragon_Tree_siumT/Medium/siumT_tier_2/StaticMeshes/SM_siumT_VarA",
    "/Game/Fab/Megascans/Plants/Dragon_Tree_siumT/Medium/siumT_tier_2/StaticMeshes/SM_siumT_VarB",
    "/Game/Fab/Megascans/Plants/Dragon_Tree_siumT/Medium/siumT_tier_2/StaticMeshes/SM_siumT_VarC",
    "/Game/Fab/Megascans/Plants/Dragon_Tree_siumT/Medium/siumT_tier_2/StaticMeshes/SM_siumT_VarD",
    "/Game/Fab/Megascans/Plants/Dragon_Tree_siumT/Medium/siumT_tier_2/StaticMeshes/SM_siumT_VarE",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Branch_Norway_Spruce_01",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Branch_Norway_Spruce_05",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Branch_Norway_Spruce_Top_01",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Decoration_Norway_Spruce_01",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Instances/Twig_Norway_Spruce_01",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A",
    "/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D",
    "/Game/Megaplant_Library/Tree_Aleppo_Pine/Tree_Aleppo_Pine_01/Tree_Aleppo_Pine_01_A",
]


def describe(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        print(f"[INSPECT] MISSING {path}")
        return
    cls = asset.get_class().get_name()
    info = f"[INSPECT] {cls:<18} {path}"
    if isinstance(asset, unreal.StaticMesh):
        b = asset.get_bounds().box_extent
        nanite = asset.get_editor_property("nanite_settings").enabled
        info += f"  extent_cm=({b.x:.0f},{b.y:.0f},{b.z:.0f}) nanite={nanite}"
    print(info)


# List Megaplant tree top-level to discover real asset names
for root in ["/Game/Megaplant_Library/Tree_Norway_Spruce", "/Game/Megaplant_Library/Tree_Aleppo_Pine"]:
    if unreal.EditorAssetLibrary.does_directory_exist(root):
        for a in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            obj = unreal.EditorAssetLibrary.load_asset(a)
            if isinstance(obj, (unreal.StaticMesh, unreal.SkeletalMesh)):
                print(f"[TREEMESH] {obj.get_class().get_name():<14} {a}")

for p in PATHS:
    describe(p)
