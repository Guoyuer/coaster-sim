import json
from pathlib import Path

import unreal


PACKAGE_PATH = "/Game/Generated/Materials"
TINT_MATERIAL_NAME = "M_CoasterTint"
MESH_TERRAIN_MATERIAL_NAME = "M_YarlungMeshTerrain"
WATER_RIVER_MATERIAL_INSTANCE_NAME = "MI_YarlungWaterRiver"
WATER_SURFACE_MATERIAL_NAME = "M_YarlungWaterSurface"
UE_WATER_RIVER_PARENT_PATH = "/Water/Materials/WaterSurface/Water_Material_River.Water_Material_River"
PROJECT_WATER_PARENT_PATH = f"{PACKAGE_PATH}/{WATER_SURFACE_MATERIAL_NAME}.{WATER_SURFACE_MATERIAL_NAME}"
SUCCESS_MARKER = "material-generation-ok.txt"


def yarlung_asset_config():
    path = Path(unreal.Paths.project_config_dir()) / "yarlung-assets.json"
    return json.loads(path.read_text(encoding="utf-8"))


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def create_material_asset(name, package_path):
    asset_path = f"{package_path}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        material = unreal.EditorAssetLibrary.load_asset(asset_path)
        if isinstance(material, unreal.Material):
            return material
        raise RuntimeError(f"Existing asset is not a Material: {asset_path}")

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        package_path,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError(f"Unable to create material: {asset_path}")
    return material


def create_material_instance_asset(name, package_path, parent_path):
    asset_path = f"{package_path}/{name}"
    parent = unreal.EditorAssetLibrary.load_asset(parent_path) if unreal.EditorAssetLibrary.does_asset_exist(parent_path) else None
    if parent is None:
        raise RuntimeError(f"Unable to load material instance parent: {parent_path}")

    instance = unreal.EditorAssetLibrary.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if instance is None:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,
            package_path,
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew(),
        )
    if instance is None:
        raise RuntimeError(f"Unable to create material instance: {asset_path}")

    instance.set_editor_property("parent", parent)
    return instance


def connect_material_property(material, expression, material_property, label):
    connected = unreal.MaterialEditingLibrary.connect_material_property(
        expression,
        "",
        material_property,
    )
    if not connected:
        raise RuntimeError(f"Unable to connect {label}")


def create_vector_parameter(material, name, value, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        x,
        y,
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", value)
    return expression


def create_scalar_parameter(material, name, value, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        x,
        y,
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", value)
    return expression


def create_constant3(material, color, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant3Vector,
        x,
        y,
    )
    expression.set_editor_property("constant", color)
    return expression


def create_constant(material, value, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        x,
        y,
    )
    expression.set_editor_property("r", value)
    return expression


def finalize_material(material):
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def create_tint_material():
    material = create_material_asset(TINT_MATERIAL_NAME, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    connect_material_property(
        material,
        create_vector_parameter(material, "BaseColor", unreal.LinearColor(0.5, 0.5, 0.5, 1.0), -500, -120),
        unreal.MaterialProperty.MP_BASE_COLOR,
        "tint BaseColor",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Roughness", 0.88, -500, 80),
        unreal.MaterialProperty.MP_ROUGHNESS,
        "tint Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", 0.10, -500, 240),
        unreal.MaterialProperty.MP_SPECULAR,
        "tint Specular",
    )
    # Metallic param (default 0 = dielectric, e.g. rock/vegetation). The coaster
    # steel sets this to ~1.0 per component so rails/structure read as real metal
    # instead of flat matte plastic. Shared material, so rocks leave it at 0.
    connect_material_property(
        material,
        create_scalar_parameter(material, "Metallic", 0.0, -500, 400),
        unreal.MaterialProperty.MP_METALLIC,
        "tint Metallic",
    )

    unreal.MaterialEditingLibrary.set_base_material_usage(
        material,
        unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,
        True,
    )
    finalize_material(material)


def set_optional_material_usage(material, usage_name):
    usage = getattr(unreal.MaterialUsage, usage_name, None)
    if usage is not None:
        unreal.MaterialEditingLibrary.set_base_material_usage(material, usage, True)


def set_instance_scalar(instance, name, value):
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        instance,
        name,
        value,
    )


def set_instance_vector(instance, name, value):
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance,
        name,
        value,
    )


def create_yarlung_water_material_instance():
    parent_path = UE_WATER_RIVER_PARENT_PATH
    if not unreal.EditorAssetLibrary.does_asset_exist(parent_path):
        parent_path = PROJECT_WATER_PARENT_PATH
        if not unreal.EditorAssetLibrary.does_asset_exist(parent_path):
            raise RuntimeError(f"Unable to load UE Water parent or project water parent: {UE_WATER_RIVER_PARENT_PATH} / {PROJECT_WATER_PARENT_PATH}")
        print(f"[WATER-MATERIAL] UE Water plugin parent unavailable; using project parent {PROJECT_WATER_PARENT_PATH}")

    instance = create_material_instance_asset(
        WATER_RIVER_MATERIAL_INSTANCE_NAME,
        PACKAGE_PATH,
        parent_path,
    )

    # Keep the UE Water shader/render path, but bias it toward glacial Yarlung
    # water: cold emerald body color, high roughness, and readable whitewater.
    vector_values = {
        "Water Albedo": unreal.LinearColor(0.055, 0.42, 0.36, 1.0),
        "Scattering": unreal.LinearColor(0.76, 1.08, 0.92, 1.0),
        "Absorption": unreal.LinearColor(0.82, 0.18, 0.08, 1.0),
        "Foam Scattering": unreal.LinearColor(0.92, 1.00, 0.90, 1.0),
        "Foam Emissive": unreal.LinearColor(0.22, 0.30, 0.22, 1.0),
    }
    scalar_values = {
        "Opacity": 0.86,
        "Water Opacity": 0.86,
        "Water Roughness": 0.86,
        "Water Specular": 0.18,
        "Refraction": 0.035,
        "Refraction Far": 0.015,
        "Water Fresnel Roughness": 0.94,
        "Water Fresnel Specular": 0.14,
        "Default Near Normal Strength": 1.85,
        "Default Distant Normal Strength": 1.05,
        "Default Distant Normal StrengthB": 0.86,
        "River Normal Flatness": 0.12,
        "River Flowmap Speed": 2.35,
        "River Flowmap Detection Velocity": 0.08,
        "River Foam Scale": 3.80,
        "Foam Opacity": 1.0,
        "Foam Roughness": 0.96,
        "FoamContrast": 5.40,
        "Foam Boost": 4.80,
        "Foam MacroScale": 0.32,
        "Front Foam Scale": 2.10,
        "SimFoam Contrast": 4.20,
        "MaxFlowVelocity": 1180.0,
    }
    for name, value in vector_values.items():
        set_instance_vector(instance, name, value)
    for name, value in scalar_values.items():
        set_instance_scalar(instance, name, value)

    unreal.EditorAssetLibrary.save_loaded_asset(instance)
    print(f"[WATER-MATERIAL] saved /Game/Generated/Materials/{WATER_RIVER_MATERIAL_INSTANCE_NAME}")
    return instance


def create_yarlung_water_surface_material():
    material = create_material_asset(WATER_SURFACE_MATERIAL_NAME, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", True)

    connect_material_property(
        material,
        create_vector_parameter(material, "BaseColor", unreal.LinearColor(0.055, 0.42, 0.36, 1.0), -500, -220),
        unreal.MaterialProperty.MP_BASE_COLOR,
        "yarlung water surface BaseColor",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Roughness", 0.92, -500, -40),
        unreal.MaterialProperty.MP_ROUGHNESS,
        "yarlung water surface Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", 0.12, -500, 120),
        unreal.MaterialProperty.MP_SPECULAR,
        "yarlung water surface Specular",
    )
    connect_material_property(
        material,
        create_vector_parameter(material, "EmissiveTint", unreal.LinearColor(0.018, 0.055, 0.047, 1.0), -500, 280),
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        "yarlung water surface EmissiveTint",
    )
    set_optional_material_usage(material, "MATUSAGE_STATIC_MESH")
    set_optional_material_usage(material, "MATUSAGE_SPLINE_MESHES")
    finalize_material(material)
    print(f"[WATER-SURFACE] saved /Game/Generated/Materials/{WATER_SURFACE_MATERIAL_NAME}")
    return material


def create_mesh_terrain_material():
    material = create_material_asset(MESH_TERRAIN_MATERIAL_NAME, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", False)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -940,
        -180,
    )
    albedo_gain = create_scalar_parameter(material, "TerrainAlbedoGain", 1.35, -640, 40)
    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -380,
        -160,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "", base_color, "A"):
        raise RuntimeError("Unable to connect mesh terrain base color A")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(albedo_gain, "", base_color, "B"):
        raise RuntimeError("Unable to connect mesh terrain base color B")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        base_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    ):
        raise RuntimeError("Unable to connect mesh terrain rock BaseColor")

    connect_material_property(
        material,
        create_scalar_parameter(material, "TerrainRoughness", 0.84, -640, 220),
        unreal.MaterialProperty.MP_ROUGHNESS,
        "mesh terrain Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "TerrainAmbientOcclusion", 0.92, -640, 400),
        unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
        "mesh terrain Ambient Occlusion",
    )

    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", 0.05, -640, 760),
        unreal.MaterialProperty.MP_SPECULAR,
        "mesh terrain Specular",
    )
    set_optional_material_usage(material, "MATUSAGE_STATIC_MESH")
    set_optional_material_usage(material, "MATUSAGE_NANITE")
    finalize_material(material)


def enable_imported_material_usages():
    """Imported asset base materials can miss Nanite / instanced-static-mesh
    usage flags. When our scatter places those assets on HierarchicalInstancedStaticMesh
    components (and Nanite cliffs) in a headless -game run, the engine has no editor
    auto-fix and swaps in the default grey material. Enable the flags on every Fab base
    material and Megaplant material so scanned PBR renders on scattered + Nanite instances."""
    material_dirs = yarlung_asset_config()["scenery"].get("material_usage_roots", [])
    usage_names = ["MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_NANITE"]
    for material_dir in material_dirs:
        if not unreal.EditorAssetLibrary.does_directory_exist(material_dir):
            print(f"[IMPORTED-USAGE] no {material_dir} directory; skipping")
            continue
        for path in unreal.EditorAssetLibrary.list_assets(material_dir, recursive=True, include_folder=False):
            asset = unreal.EditorAssetLibrary.load_asset(path)
            if not isinstance(asset, unreal.Material):
                continue
            for name in usage_names:
                usage = getattr(unreal.MaterialUsage, name, None)
                if usage is not None:
                    unreal.MaterialEditingLibrary.set_base_material_usage(asset, usage, True)
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
            print(f"[IMPORTED-USAGE] enabled instancing/nanite usage: {path}")


def main():
    ensure_folder(PACKAGE_PATH)
    enable_imported_material_usages()
    create_tint_material()
    create_yarlung_water_surface_material()
    create_yarlung_water_material_instance()
    create_mesh_terrain_material()
    marker_path = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_saved_dir() + SUCCESS_MARKER
    )
    with open(marker_path, "w", encoding="utf-8") as marker:
        marker.write("ok\n")


main()
