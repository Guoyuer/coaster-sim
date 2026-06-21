import unreal


PACKAGE_PATH = "/Game/Generated/Materials"
AERIAL_GRASS_ROCK_PACKAGE_PATH = f"{PACKAGE_PATH}/PolyHaven/AerialGrassRock"
TINT_MATERIAL_NAME = "M_CoasterTint"
RIVER_WATER_MATERIAL_NAME = "M_YarlungRiverWater"
RIVER_FOAM_MATERIAL_NAME = "M_YarlungRiverFoam"
MESH_TERRAIN_MATERIAL_NAME = "M_YarlungMeshTerrain"
SUCCESS_MARKER = "material-generation-ok.txt"
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


def connect_detail_texture(material, texture, x, y, coordinates, material_property, label):
    """Sample a PBR detail texture and wire it straight to a material output."""
    sample = create_texture_sample(material, texture, x, y, coordinates)
    connect_material_property(material, sample, material_property, label)
    return sample


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


def create_texture_sample(material, texture, x, y, coordinates=None):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        x,
        y,
    )
    expression.set_editor_property("texture", texture)
    if coordinates is not None:
        connected = False
        for input_name in ("Coordinates", "UVs"):
            connected = unreal.MaterialEditingLibrary.connect_material_expressions(
                coordinates,
                "",
                expression,
                input_name,
            )
            if connected:
                break
        if not connected:
            raise RuntimeError(f"Unable to connect texture coordinates for {texture.get_name()}")
    return expression


def create_landscape_coords(material, x, y, mapping_scale=1009.0):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionLandscapeLayerCoords,
        x,
        y,
    )
    expression.set_editor_property("mapping_type", unreal.TerrainCoordMappingType.TCMT_XY)
    expression.set_editor_property("custom_uv_type", unreal.LandscapeCustomizedCoordType.LCCT_NONE)
    expression.set_editor_property("mapping_scale", mapping_scale)
    expression.set_editor_property("mapping_rotation", 0.0)
    expression.set_editor_property("mapping_pan_u", 0.0)
    expression.set_editor_property("mapping_pan_v", 0.0)
    return expression


def create_texture_coordinate(material, x, y, u_tiling, v_tiling):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        x,
        y,
    )
    expression.set_editor_property("coordinate_index", 0)
    expression.set_editor_property("u_tiling", u_tiling)
    expression.set_editor_property("v_tiling", v_tiling)
    return expression


def create_lerp(material, a_expression, a_output, b_expression, b_output, alpha_expression, alpha_output, x, y, label):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        x,
        y,
    )
    connections = [
        (a_expression, a_output, "A"),
        (b_expression, b_output, "B"),
        (alpha_expression, alpha_output, "Alpha"),
    ]
    for source, output_name, input_name in connections:
        if not unreal.MaterialEditingLibrary.connect_material_expressions(source, output_name, expression, input_name):
            raise RuntimeError(f"Unable to connect {label} {input_name}")
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
    # Metallic param (default 0 = dielectric, e.g. rock/vegetation). The coaster
    # steel sets this to ~1.0 per component so rails/structure read as real metal
    # instead of flat matte plastic. Shared material, so rocks leave it at 0.
    connect_material_property(
        material,
        create_scalar_parameter(material, "Metallic", 0.0, -500, 400),
        unreal.MaterialProperty.MP_METALLIC,
        "tint Metallic",
    )

    unreal.MaterialEditingLibrary.set_material_usage(
        material,
        unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,
    )
    finalize_material(material)


def set_optional_material_usage(material, usage_name):
    usage = getattr(unreal.MaterialUsage, usage_name, None)
    if usage is not None:
        unreal.MaterialEditingLibrary.set_material_usage(material, usage)


def create_translucent_parameter_material(name, base_color, opacity, roughness, specular):
    material = create_material_asset(name, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", False)

    connect_material_property(
        material,
        create_vector_parameter(material, "BaseColor", base_color, -620, -240),
        unreal.MaterialProperty.MP_BASE_COLOR,
        f"{name} BaseColor",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Opacity", opacity, -620, -40),
        unreal.MaterialProperty.MP_OPACITY,
        f"{name} Opacity",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Roughness", roughness, -620, 160),
        unreal.MaterialProperty.MP_ROUGHNESS,
        f"{name} Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", specular, -620, 340),
        unreal.MaterialProperty.MP_SPECULAR,
        f"{name} Specular",
    )
    set_optional_material_usage(material, "MATUSAGE_PROCEDURAL_MESHES")
    finalize_material(material)


def create_translucent_vertex_color_material(name, opacity, roughness, specular):
    material = create_material_asset(name, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", False)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -620,
        -160,
    )
    if not unreal.MaterialEditingLibrary.connect_material_property(
        vertex_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    ):
        raise RuntimeError(f"Unable to connect {name} vertex BaseColor")
    opacity_scale = create_scalar_parameter(material, "Opacity", opacity, -620, 20)
    scaled_opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -360,
        -20,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(vertex_color, "A", scaled_opacity, "A"):
        raise RuntimeError(f"Unable to connect {name} vertex alpha")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(opacity_scale, "", scaled_opacity, "B"):
        raise RuntimeError(f"Unable to connect {name} opacity scale")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        scaled_opacity,
        "",
        unreal.MaterialProperty.MP_OPACITY,
    ):
        raise RuntimeError(f"Unable to connect {name} vertex Opacity")
    connect_material_property(
        material,
        create_scalar_parameter(material, "Roughness", roughness, -620, 160),
        unreal.MaterialProperty.MP_ROUGHNESS,
        f"{name} Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", specular, -620, 340),
        unreal.MaterialProperty.MP_SPECULAR,
        f"{name} Specular",
    )
    set_optional_material_usage(material, "MATUSAGE_PROCEDURAL_MESHES")
    finalize_material(material)


def create_opaque_vertex_color_material(name, roughness, specular):
    material = create_material_asset(name, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", True)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -620,
        -160,
    )
    if not unreal.MaterialEditingLibrary.connect_material_property(
        vertex_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    ):
        raise RuntimeError(f"Unable to connect {name} vertex BaseColor")
    connect_material_property(
        material,
        create_scalar_parameter(material, "Roughness", roughness, -620, 160),
        unreal.MaterialProperty.MP_ROUGHNESS,
        f"{name} Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", specular, -620, 340),
        unreal.MaterialProperty.MP_SPECULAR,
        f"{name} Specular",
    )
    set_optional_material_usage(material, "MATUSAGE_STATIC_MESH")
    set_optional_material_usage(material, "MATUSAGE_NANITE")
    finalize_material(material)


def create_river_materials():
    create_translucent_vertex_color_material(
        RIVER_WATER_MATERIAL_NAME,
        0.14,
        0.18,
        0.75,
    )
    create_translucent_vertex_color_material(
        RIVER_FOAM_MATERIAL_NAME,
        0.24,
        0.62,
        0.20,
    )


def create_mesh_terrain_material(rock_textures):
    material = create_material_asset(MESH_TERRAIN_MATERIAL_NAME, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", False)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -940,
        -180,
    )
    detail_coordinates = create_texture_coordinate(material, -940, 240, 18.0, 18.0)
    rock_diffuse = create_texture_sample(
        material,
        rock_textures["T_AerialGrassRock_Diffuse"],
        -640,
        -220,
        detail_coordinates,
    )
    rock_mix = create_lerp(
        material,
        vertex_color,
        "",
        rock_diffuse,
        "",
        create_constant(material, 0.08, -640, -20),
        "",
        -360,
        -160,
        "mesh terrain rock diffuse mix",
    )
    # Keep broad vertex-color landform bands readable; the scanned albedo is only
    # a surface breakup layer because it photographs too pale on whole mountains.
    brightness = create_scalar_parameter(material, "TerrainBrightness", 0.48, -440, 40)
    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        -200,
        -160,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(rock_mix, "", base_color, "A"):
        raise RuntimeError("Unable to connect mesh terrain brightness A")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(brightness, "", base_color, "B"):
        raise RuntimeError("Unable to connect mesh terrain brightness B")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        base_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    ):
        raise RuntimeError("Unable to connect mesh terrain rock BaseColor")

    connect_detail_texture(
        material, rock_textures["T_AerialGrassRock_Normal"], -640, 120, detail_coordinates,
        unreal.MaterialProperty.MP_NORMAL, "mesh terrain Normal")
    connect_detail_texture(
        material, rock_textures["T_AerialGrassRock_Rough"], -640, 360, detail_coordinates,
        unreal.MaterialProperty.MP_ROUGHNESS, "mesh terrain Roughness")
    connect_detail_texture(
        material, rock_textures["T_AerialGrassRock_AO"], -640, 580, detail_coordinates,
        unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, "mesh terrain Ambient Occlusion")

    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", 0.05, -640, 760),
        unreal.MaterialProperty.MP_SPECULAR,
        "mesh terrain Specular",
    )
    set_optional_material_usage(material, "MATUSAGE_STATIC_MESH")
    set_optional_material_usage(material, "MATUSAGE_NANITE")
    finalize_material(material)


def main():
    ensure_folder(PACKAGE_PATH)
    ensure_folder(AERIAL_GRASS_ROCK_PACKAGE_PATH)
    create_tint_material()
    create_river_materials()
    aerial_grass_rock_textures = import_textures(
        AERIAL_GRASS_ROCK_PACKAGE_PATH,
        AERIAL_GRASS_ROCK_SOURCE_DIR,
        AERIAL_GRASS_ROCK_TEXTURES,
    )
    create_mesh_terrain_material(aerial_grass_rock_textures)
    marker_path = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_saved_dir() + SUCCESS_MARKER
    )
    with open(marker_path, "w", encoding="utf-8") as marker:
        marker.write("ok\n")


main()
