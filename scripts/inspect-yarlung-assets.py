import json
from pathlib import Path

import unreal


CONFIG_PATH = Path(unreal.Paths.project_config_dir()) / "yarlung-assets.json"


def load_config():
    return json.loads(CONFIG_PATH.read_text(encoding="utf-8"))


def describe_asset(path):
    if not path:
        print("[ASSET] empty path")
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
        ("fallback_river", "fallback_river_material"),
        ("surface", "surface_material"),
    ):
        material_path = water.get(key, "")
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


def list_local_tree_meshes():
    for root in ["/Game/Megaplant_Library/Tree_Norway_Spruce", "/Game/Megaplant_Library/Tree_Aleppo_Pine"]:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        for path in unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False):
            obj = unreal.EditorAssetLibrary.load_asset(path)
            if isinstance(obj, (unreal.StaticMesh, unreal.SkeletalMesh)):
                print(f"[TREEMESH] {obj.get_class().get_name():<14} {path}")


def main():
    config = load_config()
    inspect_scenery_assets(config)
    inspect_water_materials(config)
    list_local_tree_meshes()


main()
