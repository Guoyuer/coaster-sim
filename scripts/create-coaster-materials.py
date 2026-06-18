import unreal


PACKAGE_PATH = "/Game/Generated/Materials"
PBR_PACKAGE_PATH = f"{PACKAGE_PATH}/PolyHaven/LeafyGrass"
MATERIAL_NAME = "M_CoasterTint"
LANDSCAPE_MATERIAL_NAME = "M_YarlungLandscapeGround"
PBR_SOURCE_DIR = "SourceAssets/PolyHaven/leafy_grass"
PBR_TEXTURES = {
    "T_LeafyGrass_Diffuse": ("leafy_grass_diff_4k.jpg", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    "T_LeafyGrass_Normal": ("leafy_grass_nor_dx_4k.jpg", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "T_LeafyGrass_Rough": ("leafy_grass_rough_4k.jpg", False, unreal.TextureCompressionSettings.TC_DEFAULT),
    "T_LeafyGrass_AO": ("leafy_grass_ao_4k.jpg", False, unreal.TextureCompressionSettings.TC_DEFAULT),
}


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def create_or_load_material(name, package_path):
    asset_path = f"{package_path}/{name}"
    material = unreal.EditorAssetLibrary.load_asset(asset_path)
    if material is None:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = asset_tools.create_asset(
            name,
            package_path,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
    return material


def recreate_material(name, package_path):
    asset_path = f"{package_path}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise RuntimeError(f"Unable to delete stale material: {asset_path}")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        name,
        package_path,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError(f"Unable to create material: {asset_path}")
    return material


def import_texture(name, filename, srgb, compression):
    asset_path = f"{PBR_PACKAGE_PATH}/{name}"
    source = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + f"{PBR_SOURCE_DIR}/{filename}"
    )

    if not unreal.Paths.file_exists(source):
        raise RuntimeError(f"Missing PBR texture source: {source}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", PBR_PACKAGE_PATH)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None:
        raise RuntimeError(f"Unable to load imported texture: {asset_path}")

    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def main():
    ensure_folder(PACKAGE_PATH)
    ensure_folder(PBR_PACKAGE_PATH)
    material = create_or_load_material(MATERIAL_NAME, PACKAGE_PATH)

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -500,
        -120,
    )
    base_color.set_editor_property("parameter_name", "BaseColor")
    base_color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        base_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -500,
        80,
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.88)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    specular = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -500,
        240,
    )
    specular.set_editor_property("parameter_name", "Specular")
    specular.set_editor_property("default_value", 0.10)
    unreal.MaterialEditingLibrary.connect_material_property(
        specular,
        "",
        unreal.MaterialProperty.MP_SPECULAR,
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

    _pbr_textures = {
        name: import_texture(name, *settings)
        for name, settings in PBR_TEXTURES.items()
    }

    landscape_material = recreate_material(LANDSCAPE_MATERIAL_NAME, PACKAGE_PATH)

    macro_forest_color = unreal.MaterialEditingLibrary.create_material_expression(
        landscape_material,
        unreal.MaterialExpressionConstant3Vector,
        -520,
        -220,
    )
    macro_forest_color.set_editor_property("constant", unreal.LinearColor(0.045, 0.32, 0.085, 1.0))

    connected = unreal.MaterialEditingLibrary.connect_material_property(
        macro_forest_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    if not connected:
        raise RuntimeError("Unable to connect Poly Haven leafy grass BaseColor")

    landscape_roughness = unreal.MaterialEditingLibrary.create_material_expression(
        landscape_material,
        unreal.MaterialExpressionConstant,
        -560,
        80,
    )
    landscape_roughness.set_editor_property("r", 0.92)
    unreal.MaterialEditingLibrary.connect_material_property(
        landscape_roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    landscape_specular = unreal.MaterialEditingLibrary.create_material_expression(
        landscape_material,
        unreal.MaterialExpressionConstant,
        -230,
        600,
    )
    landscape_specular.set_editor_property("r", 0.08)
    unreal.MaterialEditingLibrary.connect_material_property(
        landscape_specular,
        "",
        unreal.MaterialProperty.MP_SPECULAR,
    )

    unreal.MaterialEditingLibrary.layout_material_expressions(landscape_material)
    unreal.MaterialEditingLibrary.recompile_material(landscape_material)
    unreal.EditorAssetLibrary.save_loaded_asset(landscape_material)


main()
