import json
from pathlib import Path

import unreal


PACKAGE_PATH = "/Game/Generated/Materials"
TINT_MATERIAL_NAME = "M_CoasterTint"
MESH_TERRAIN_MATERIAL_NAME = "M_YarlungMeshTerrain"
WATER_SURFACE_MATERIAL_NAME = "M_YarlungWaterSurface"
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


def create_yarlung_water_surface_material():
    material = create_material_asset(WATER_SURFACE_MATERIAL_NAME, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", True)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -860,
        -180,
    )
    vertex_color_gain = create_scalar_parameter(material, "VertexColorBlend", 0.72, -620, -20)
    base_color_param = create_vector_parameter(
        material,
        "BaseColor",
        unreal.LinearColor(0.008, 0.105, 0.125, 1.0),
        -640,
        -300,
    )
    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        -120,
        -200,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(base_color_param, "", base_color, "A"):
        raise RuntimeError("Unable to connect yarlung water base color")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "", base_color, "B"):
        raise RuntimeError("Unable to connect yarlung water vertex color")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color_gain, "", base_color, "Alpha"):
        raise RuntimeError("Unable to connect yarlung water vertex-color blend")
    connect_material_property(
        material,
        base_color,
        unreal.MaterialProperty.MP_BASE_COLOR,
        "yarlung water surface vertex BaseColor",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Roughness", 0.24, -500, -40),
        unreal.MaterialProperty.MP_ROUGHNESS,
        "yarlung water surface Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", 0.78, -500, 120),
        unreal.MaterialProperty.MP_SPECULAR,
        "yarlung water surface Specular",
    )
    connect_material_property(
        material,
        create_vector_parameter(material, "EmissiveTint", unreal.LinearColor(0.001, 0.004, 0.004, 1.0), -500, 280),
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
    albedo_gain = create_scalar_parameter(material, "TerrainAlbedoGain", 0.78, -640, 40)
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
    usage_names = ["MATUSAGE_INSTANCED_STATIC_MESHES", "MATUSAGE_NANITE"]
    touched_materials = set()

    def resolve_base_material(asset):
        current = asset
        seen = set()
        while isinstance(current, unreal.MaterialInstance):
            path = current.get_path_name()
            if path in seen:
                raise RuntimeError(f"Material instance parent cycle while enabling usage: {path}")
            seen.add(path)
            try:
                current = current.get_editor_property("parent")
            except Exception:
                raise RuntimeError(f"Unable to read material instance parent: {path}")
            if not current:
                raise RuntimeError(f"Material instance has no parent: {path}")
        return current if isinstance(current, unreal.Material) else None

    def configured_material_interfaces():
        for component in yarlung_asset_config()["scenery"]["components"]:
            mesh_path = component.get("mesh")
            name = component.get("name", "<unnamed>")
            if not mesh_path:
                raise RuntimeError(f"Configured scenery component has no mesh: {name}")
            mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
            if not mesh:
                raise RuntimeError(f"Configured scenery mesh does not exist for {name}: {mesh_path}")
            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError(f"Configured scenery mesh is not a StaticMesh for {name}: {mesh_path}")

            static_materials = mesh.get_editor_property("static_materials")
            if not static_materials:
                raise RuntimeError(f"Configured scenery mesh has no material slots for {name}: {mesh_path}")
            for slot_index, static_material in enumerate(static_materials):
                material = static_material.get_editor_property("material_interface")
                if not material:
                    raise RuntimeError(f"Configured scenery mesh has empty material slot {slot_index} for {name}: {mesh_path}")
                yield material

    for asset in configured_material_interfaces():
        material = resolve_base_material(asset)
        if not material:
            raise RuntimeError(f"Unable to resolve base material for configured material: {asset.get_path_name()}")
        material_path = material.get_path_name()
        if material_path in touched_materials:
            continue
        for name in usage_names:
            usage = getattr(unreal.MaterialUsage, name, None)
            if usage is not None:
                unreal.MaterialEditingLibrary.set_base_material_usage(material, usage, True)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        touched_materials.add(material_path)
        print(f"[IMPORTED-USAGE] enabled instancing/nanite usage: {material_path}")


def main():
    ensure_folder(PACKAGE_PATH)
    enable_imported_material_usages()
    create_tint_material()
    create_yarlung_water_surface_material()
    create_mesh_terrain_material()
    marker_path = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_saved_dir() + SUCCESS_MARKER
    )
    with open(marker_path, "w", encoding="utf-8") as marker:
        marker.write("ok\n")


main()
