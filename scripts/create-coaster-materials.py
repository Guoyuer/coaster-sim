import unreal


PACKAGE_PATH = "/Game/Generated/Materials"
LEAFY_GRASS_PACKAGE_PATH = f"{PACKAGE_PATH}/PolyHaven/LeafyGrass"
AERIAL_GRASS_ROCK_PACKAGE_PATH = f"{PACKAGE_PATH}/PolyHaven/AerialGrassRock"
TINT_MATERIAL_NAME = "M_CoasterTint"
LANDSCAPE_MATERIAL_NAME = "M_YarlungLandscapeGround"
LEAFY_GRASS_SOURCE_DIR = "SourceAssets/PolyHaven/leafy_grass"
LEAFY_GRASS_TEXTURES = {
    "T_LeafyGrass_Diffuse": ("leafy_grass_diff_4k.jpg", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    "T_LeafyGrass_Normal": ("leafy_grass_nor_dx_4k.jpg", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "T_LeafyGrass_Rough": ("leafy_grass_rough_4k.jpg", False, unreal.TextureCompressionSettings.TC_DEFAULT),
    "T_LeafyGrass_AO": ("leafy_grass_ao_4k.jpg", False, unreal.TextureCompressionSettings.TC_DEFAULT),
}
AERIAL_GRASS_ROCK_SOURCE_DIR = "SourceAssets/PolyHaven/aerial_grass_rock"
AERIAL_GRASS_ROCK_TEXTURES = {
    "T_AerialGrassRock_Diffuse": ("aerial_grass_rock_diff_2k.jpg", True, unreal.TextureCompressionSettings.TC_DEFAULT),
    "T_AerialGrassRock_Normal": ("aerial_grass_rock_nor_gl_2k.jpg", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "T_AerialGrassRock_Rough": ("aerial_grass_rock_rough_2k.jpg", False, unreal.TextureCompressionSettings.TC_DEFAULT),
    "T_AerialGrassRock_AO": ("aerial_grass_rock_ao_2k.jpg", False, unreal.TextureCompressionSettings.TC_DEFAULT),
}


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def create_material_asset(name, package_path, replace_existing=False):
    asset_path = f"{package_path}/{name}"
    if replace_existing and unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise RuntimeError(f"Unable to delete stale material: {asset_path}")

    material = unreal.EditorAssetLibrary.load_asset(asset_path)
    if material:
        return material

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


def create_texture_sample(material, texture, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        x,
        y,
    )
    expression.set_editor_property("texture", texture)
    return expression


def finalize_material(material):
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def import_texture(package_path, source_dir, name, filename, srgb, compression):
    asset_path = f"{package_path}/{name}"
    source = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + f"{source_dir}/{filename}"
    )

    if not unreal.Paths.file_exists(source):
        raise RuntimeError(f"Missing PBR texture source: {source}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", package_path)
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


def import_textures(package_path, source_dir, texture_settings):
    textures = {}
    for name, settings in texture_settings.items():
        textures[name] = import_texture(package_path, source_dir, name, *settings)
    return textures


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

    unreal.MaterialEditingLibrary.set_material_usage(
        material,
        unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,
    )
    finalize_material(material)


def create_landscape_material(textures):
    material = create_material_asset(LANDSCAPE_MATERIAL_NAME, PACKAGE_PATH, replace_existing=True)

    connect_material_property(
        material,
        create_constant3(material, unreal.LinearColor(0.038, 0.26, 0.082, 1.0), -760, -260),
        unreal.MaterialProperty.MP_BASE_COLOR,
        "landscape BaseColor",
    )
    connect_material_property(
        material,
        create_texture_sample(material, textures["T_AerialGrassRock_Normal"], -760, -20),
        unreal.MaterialProperty.MP_NORMAL,
        "landscape Normal",
    )
    roughness = create_texture_sample(material, textures["T_AerialGrassRock_Rough"], -760, 220)
    connected = unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "R",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )
    if not connected:
        raise RuntimeError("Unable to connect landscape Roughness")
    ambient_occlusion = create_texture_sample(material, textures["T_AerialGrassRock_AO"], -760, 460)
    connected = unreal.MaterialEditingLibrary.connect_material_property(
        ambient_occlusion,
        "R",
        unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,
    )
    if not connected:
        raise RuntimeError("Unable to connect landscape Ambient Occlusion")
    connect_material_property(
        material,
        create_constant(material, 0.06, -260, 620),
        unreal.MaterialProperty.MP_SPECULAR,
        "landscape Specular",
    )

    finalize_material(material)


def main():
    ensure_folder(PACKAGE_PATH)
    ensure_folder(LEAFY_GRASS_PACKAGE_PATH)
    ensure_folder(AERIAL_GRASS_ROCK_PACKAGE_PATH)
    create_tint_material()
    import_textures(LEAFY_GRASS_PACKAGE_PATH, LEAFY_GRASS_SOURCE_DIR, LEAFY_GRASS_TEXTURES)
    aerial_grass_rock_textures = import_textures(
        AERIAL_GRASS_ROCK_PACKAGE_PATH,
        AERIAL_GRASS_ROCK_SOURCE_DIR,
        AERIAL_GRASS_ROCK_TEXTURES,
    )
    create_landscape_material(aerial_grass_rock_textures)


main()
