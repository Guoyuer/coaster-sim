import json
from pathlib import Path

import unreal


def load_config():
    path = Path(unreal.Paths.project_config_dir()) / "yarlung-assets.json"
    print(f"[ASSET-CONFIG] {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def describe_asset(path):
    if not path:
        print("[ASSET] empty path")
        return
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        print(f"[ASSET] MISSING {path}")
        return
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        print(f"[ASSET] MISSING {path}")
        return

    cls = asset.get_class().get_name()
    info = f"[ASSET] {cls:<24} {path}"
    if isinstance(asset, unreal.StaticMesh):
        bounds = asset.get_bounds().box_extent
        nanite = asset.get_editor_property("nanite_settings").enabled
        info += f" extent_cm=({bounds.x:.0f},{bounds.y:.0f},{bounds.z:.0f}) nanite={nanite}"
    print(info)


def inspect_scenery_assets(config):
    for component in config["scenery"]["components"]:
        print(
            "[SCENERY-COMPONENT] "
            f"name={component['name']} kind={component.get('kind', '')} "
            f"count={component.get('count', 0)} seed={component.get('seed', 0)}"
        )
        describe_asset(component.get("mesh", ""))


def inspect_water_materials(config):
    water = config["water"]
    for label, key in (
        ("river", "river_material"),
        ("surface", "surface_material"),
    ):
        material_path = water.get(key, "")
        if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
            print(f"[WATER-PARAMS] {label}=MISSING {material_path}")
            continue
        material = unreal.EditorAssetLibrary.load_asset(material_path)
        if material is None:
            print(f"[WATER-PARAMS] {label}=MISSING {material_path}")
            continue

        print(f"[WATER-PARAMS] {label}={material.get_path_name()} class={material.get_class().get_name()}")
        for getter, param_label in (
            (unreal.MaterialEditingLibrary.get_scalar_parameter_names, "scalar"),
            (unreal.MaterialEditingLibrary.get_vector_parameter_names, "vector"),
            (unreal.MaterialEditingLibrary.get_texture_parameter_names, "texture"),
        ):
            try:
                names = getter(material)
            except Exception as exc:
                print(f"[WATER-PARAMS] {label}.{param_label}_error={exc}")
                continue
            print(f"[WATER-PARAMS] {label}.{param_label}_count={len(names)}")
            for name in names:
                print(f"[WATER-PARAMS] {label}.{param_label}={name}")


def inspect_terrain_surface_assets(config):
    terrain = config.get("terrain")
    if not isinstance(terrain, dict):
        raise RuntimeError("Missing terrain config in Config/yarlung-assets.json")
    surface = terrain.get("surface")
    if not isinstance(surface, dict):
        raise RuntimeError("Missing terrain.surface config in Config/yarlung-assets.json")

    print(
        "[TERRAIN-SURFACE] "
        f"tiling={surface.get('tiling')} detail_strength={surface.get('detail_strength')} "
        f"roughness_strength={surface.get('roughness_strength')} ao_strength={surface.get('ao_strength')}"
    )
    for label in ("base_color", "normal", "orm"):
        texture_path = surface.get(label, "")
        if not texture_path:
            raise RuntimeError(f"Missing terrain.surface.{label} in Config/yarlung-assets.json")
        if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
            raise RuntimeError(f"Missing terrain surface texture {label}: {texture_path}")
        texture = unreal.EditorAssetLibrary.load_asset(texture_path)
        if not isinstance(texture, unreal.Texture):
            raise RuntimeError(f"Configured terrain surface asset is not a Texture for {label}: {texture_path}")
        print(f"[TERRAIN-SURFACE] {label}={texture.get_path_name()} class={texture.get_class().get_name()}")


def list_local_tree_meshes():
    roots = [
        "/Game/PN_interactiveSpruceForest/Meshes",
        "/Game/PN_interactiveSpruceForest/ExampleContent/Winter/Meshes",
    ]
    for root in roots:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        for path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            obj = unreal.EditorAssetLibrary.load_asset(path)
            if isinstance(obj, (unreal.StaticMesh, unreal.SkeletalMesh)):
                info = f"[TREEMESH] {obj.get_class().get_name():<14} {path}"
                if isinstance(obj, unreal.StaticMesh):
                    bounds = obj.get_bounds().box_extent
                    nanite = obj.get_editor_property("nanite_settings").enabled
                    info += f" extent_cm=({bounds.x:.0f},{bounds.y:.0f},{bounds.z:.0f}) nanite={nanite}"
                print(info)


def list_new_asset_candidates():
    roots = [
        "/Game/Fab/Alaskan_Cliff_Rock_1_Free",
        "/Game/Fab/Megascans/3D/Nordic_Forest_Tree_Fallen_Medium_tkerbglda",
    ]
    for root in roots:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        for path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            obj = unreal.EditorAssetLibrary.load_asset(path)
            if isinstance(obj, (unreal.StaticMesh, unreal.MaterialInterface)):
                print(f"[NEW-ASSET] {obj.get_class().get_name():<24} {path}")


def main():
    config = load_config()
    inspect_scenery_assets(config)
    inspect_water_materials(config)
    inspect_terrain_surface_assets(config)
    list_local_tree_meshes()
    list_new_asset_candidates()


main()
