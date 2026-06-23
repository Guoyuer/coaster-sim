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


def required_terrain_surface_config(terrain, key):
    surface = terrain.get(key)
    if not isinstance(surface, dict):
        raise RuntimeError(f"Missing terrain.{key} config in Config/yarlung-assets.json")
    for texture_key in ("base_color", "normal", "orm"):
        if not surface.get(texture_key):
            raise RuntimeError(f"Missing terrain.{key}.{texture_key} in Config/yarlung-assets.json")
    return surface


def terrain_surface_config():
    config = yarlung_asset_config()
    terrain = config.get("terrain")
    if not isinstance(terrain, dict):
        raise RuntimeError("Missing terrain config in Config/yarlung-assets.json")
    return required_terrain_surface_config(terrain, "surface"), required_terrain_surface_config(terrain, "rock_surface")


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


def create_texture_coordinate(material, tiling, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        x,
        y,
    )
    expression.set_editor_property("u_tiling", tiling)
    expression.set_editor_property("v_tiling", tiling)
    return expression


def load_required_texture(path, label):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError(f"Missing required {label} texture: {path}")
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(texture, unreal.Texture):
        raise RuntimeError(f"Configured {label} asset is not a Texture: {path}")
    return texture


def create_texture_sample(material, texture, x, y, coordinates=None, sampler_type=None):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSample,
        x,
        y,
    )
    expression.set_editor_property("texture", texture)
    if sampler_type is not None:
        expression.set_editor_property("sampler_type", sampler_type)
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
            raise RuntimeError(f"Unable to connect texture coordinates for {texture.get_path_name()}")
    return expression


def create_multiply(material, a_expression, a_output, b_expression, b_output, x, y, label):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionMultiply,
        x,
        y,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(a_expression, a_output, expression, "A"):
        raise RuntimeError(f"Unable to connect {label} A")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(b_expression, b_output, expression, "B"):
        raise RuntimeError(f"Unable to connect {label} B")
    return expression


def create_lerp(material, a_expression, a_output, b_expression, b_output, alpha_expression, alpha_output, x, y, label):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionLinearInterpolate,
        x,
        y,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(a_expression, a_output, expression, "A"):
        raise RuntimeError(f"Unable to connect {label} A")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(b_expression, b_output, expression, "B"):
        raise RuntimeError(f"Unable to connect {label} B")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(alpha_expression, alpha_output, expression, "Alpha"):
        raise RuntimeError(f"Unable to connect {label} Alpha")
    return expression


def create_add(material, a_expression, a_output, b_expression, b_output, x, y, label):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionAdd,
        x,
        y,
    )
    if not unreal.MaterialEditingLibrary.connect_material_expressions(a_expression, a_output, expression, "A"):
        raise RuntimeError(f"Unable to connect {label} A")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(b_expression, b_output, expression, "B"):
        raise RuntimeError(f"Unable to connect {label} B")
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
    # Single Layer Water is the engine's purpose-built river/lake shader: it gives
    # real depth-based transparency, refraction and underwater scattering in one
    # opaque pass. The old Default-Lit + opaque setup had no transparency or depth
    # absorption at all, so the dark low-roughness surface read as glossy plastic.
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -860,
        -180,
    )
    vertex_color_gain = create_scalar_parameter(material, "VertexColorBlend", 0.78, -620, -20)
    base_color_param = create_vector_parameter(
        material,
        "BaseColor",
        unreal.LinearColor(0.002, 0.038, 0.046, 1.0),
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
        create_scalar_parameter(material, "Roughness", 0.06, -500, -40),
        unreal.MaterialProperty.MP_ROUGHNESS,
        "yarlung water surface Roughness",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Specular", 0.58, -500, 120),
        unreal.MaterialProperty.MP_SPECULAR,
        "yarlung water surface Specular",
    )
    connect_material_property(
        material,
        create_scalar_parameter(material, "Opacity", 1.0, -500, 280),
        unreal.MaterialProperty.MP_OPACITY,
        "yarlung water surface Opacity",
    )
    fresnel = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionFresnel,
        -500,
        460,
    )
    fresnel_tint = create_vector_parameter(
        material,
        "GrazingHighlight",
        unreal.LinearColor(0.008, 0.024, 0.030, 1.0),
        -500,
        640,
    )
    fresnel_highlight = create_multiply(
        material,
        fresnel,
        "",
        fresnel_tint,
        "",
        -120,
        540,
        "yarlung water grazing highlight",
    )
    subtle_emissive = create_vector_parameter(
        material,
        "SubtleWaterLift",
        unreal.LinearColor(0.0002, 0.0008, 0.0009, 1.0),
        -500,
        820,
    )
    connect_material_property(
        material,
        create_add(
            material,
            fresnel_highlight,
            "",
            subtle_emissive,
            "",
            120,
            660,
            "yarlung water emissive highlight",
        ),
        unreal.MaterialProperty.MP_EMISSIVE_COLOR,
        "yarlung water surface EmissiveColor",
    )
    # Single Layer Water absorption + scattering drive the glacial colour and how
    # fast the channel goes opaque with depth. Coefficients are per-cm extinction
    # (red absorbs fastest, so deep mid-channel reads teal while the shallow ~0.6m
    # shoreline stays clear). Exposed as parameters so they are tunable on a
    # material instance without recompiling.
    water_output = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionSingleLayerWaterMaterialOutput,
        320,
        420,
    )
    absorption = create_vector_parameter(
        material,
        "WaterAbsorption",
        unreal.LinearColor(0.0042, 0.0019, 0.0016, 1.0),
        -120,
        360,
    )
    scattering = create_vector_parameter(
        material,
        "WaterScattering",
        unreal.LinearColor(0.0011, 0.0021, 0.0020, 1.0),
        -120,
        520,
    )
    def connect_water_coefficient(source, input_names, label):
        for input_name in input_names:
            if unreal.MaterialEditingLibrary.connect_material_expressions(
                source, "", water_output, input_name
            ):
                return
        raise RuntimeError(f"Unable to connect yarlung water {label}")

    # Input display names differ slightly between engine versions, so try the
    # spaced and unspaced variants.
    connect_water_coefficient(
        absorption,
        ("Absorption Coefficients", "AbsorptionCoefficients"),
        "absorption coefficients",
    )
    connect_water_coefficient(
        scattering,
        ("Scattering Coefficients", "ScatteringCoefficients"),
        "scattering coefficients",
    )
    set_optional_material_usage(material, "MATUSAGE_STATIC_MESH")
    set_optional_material_usage(material, "MATUSAGE_SPLINE_MESHES")
    finalize_material(material)
    print(f"[WATER-SURFACE] saved /Game/Generated/Materials/{WATER_SURFACE_MATERIAL_NAME}")
    return material


def create_mesh_terrain_material():
    surface, rock_surface = terrain_surface_config()
    surface_base_color = load_required_texture(surface["base_color"], "terrain base color")
    surface_normal = load_required_texture(surface["normal"], "terrain normal")
    surface_orm = load_required_texture(surface["orm"], "terrain ORM")
    tiling = float(surface.get("tiling", 44.0))
    detail_strength = float(surface.get("detail_strength", 0.36))
    roughness_strength = float(surface.get("roughness_strength", 0.42))
    ao_strength = float(surface.get("ao_strength", 0.30))
    rock_base_color = load_required_texture(rock_surface["base_color"], "terrain rock base color")
    rock_normal = load_required_texture(rock_surface["normal"], "terrain rock normal")
    rock_orm = load_required_texture(rock_surface["orm"], "terrain rock ORM")
    rock_tiling = float(rock_surface.get("tiling", 28.0))
    rock_blend_strength = float(rock_surface.get("blend_strength", 0.86))

    material = create_material_asset(MESH_TERRAIN_MATERIAL_NAME, PACKAGE_PATH)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("two_sided", False)

    terrain_uv = create_texture_coordinate(material, tiling, -1180, 160)
    rock_uv = create_texture_coordinate(material, rock_tiling, -1180, 500)
    surface_base = create_texture_sample(material, surface_base_color, -900, 120, terrain_uv)
    surface_normals = create_texture_sample(
        material,
        surface_normal,
        -900,
        380,
        terrain_uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    surface_masks = create_texture_sample(material, surface_orm, -900, 700, terrain_uv)
    rock_base = create_texture_sample(material, rock_base_color, -900, 980, rock_uv)
    rock_normals = create_texture_sample(
        material,
        rock_normal,
        -900,
        1240,
        rock_uv,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    rock_masks = create_texture_sample(material, rock_orm, -900, 1560, rock_uv)

    vertex_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVertexColor,
        -1180,
        -220,
    )
    wet_rock_mask = create_multiply(
        material,
        vertex_color,
        "R",
        create_scalar_parameter(material, "TerrainWetRockCoverageStrength", 0.96, -620, -360),
        "",
        -620,
        -360,
        "mesh terrain wet-rock coverage mask",
    )
    forest_floor_mask = create_multiply(
        material,
        vertex_color,
        "G",
        create_scalar_parameter(material, "TerrainForestFloorCoverageStrength", 0.88, -620, -180),
        "",
        -620,
        -180,
        "mesh terrain forest-floor coverage mask",
    )
    scree_mask = create_multiply(
        material,
        vertex_color,
        "B",
        create_scalar_parameter(material, "TerrainScreeCoverageStrength", rock_blend_strength, -620, 0),
        "",
        -620,
        0,
        "mesh terrain scree coverage mask",
    )

    forest_floor_color = create_constant3(
        material,
        unreal.LinearColor(0.050, 0.170, 0.070, 1.0),
        -900,
        -440,
    )
    weathered_rock_color = create_constant3(
        material,
        unreal.LinearColor(0.118, 0.126, 0.116, 1.0),
        -900,
        -300,
    )
    scree_color = create_constant3(
        material,
        unreal.LinearColor(0.150, 0.142, 0.122, 1.0),
        -900,
        -160,
    )
    wet_rock_color = create_constant3(
        material,
        unreal.LinearColor(0.055, 0.075, 0.070, 1.0),
        -900,
        -20,
    )
    macro_forest_base = create_lerp(
        material,
        weathered_rock_color,
        "",
        forest_floor_color,
        "",
        forest_floor_mask,
        "",
        -320,
        -420,
        "mesh terrain forest-floor macro color",
    )
    macro_scree_base = create_lerp(
        material,
        macro_forest_base,
        "",
        scree_color,
        "",
        scree_mask,
        "",
        -80,
        -340,
        "mesh terrain scree macro color",
    )
    macro_base_color = create_lerp(
        material,
        macro_scree_base,
        "",
        wet_rock_color,
        "",
        wet_rock_mask,
        "",
        160,
        -260,
        "mesh terrain wet-rock shore macro color",
    )
    blended_surface_base = create_lerp(
        material,
        surface_base,
        "",
        rock_base,
        "",
        scree_mask,
        "",
        -320,
        340,
        "mesh terrain forest/scree surface color",
    )
    wet_surface_base = create_lerp(
        material,
        blended_surface_base,
        "",
        rock_base,
        "",
        wet_rock_mask,
        "",
        -80,
        250,
        "mesh terrain wet-rock surface color",
    )
    blended_surface_normals = create_lerp(
        material,
        surface_normals,
        "",
        rock_normals,
        "",
        scree_mask,
        "",
        -320,
        560,
        "mesh terrain forest/scree surface normal",
    )
    wet_surface_normals = create_lerp(
        material,
        blended_surface_normals,
        "",
        rock_normals,
        "",
        wet_rock_mask,
        "",
        -80,
        520,
        "mesh terrain wet-rock surface normal",
    )
    blended_surface_roughness = create_lerp(
        material,
        surface_masks,
        "G",
        rock_masks,
        "G",
        scree_mask,
        "",
        -320,
        780,
        "mesh terrain forest/scree surface roughness",
    )
    wet_surface_roughness = create_lerp(
        material,
        blended_surface_roughness,
        "",
        create_scalar_parameter(material, "TerrainWetRockRoughness", 0.66, -620, 240),
        "",
        wet_rock_mask,
        "",
        -80,
        780,
        "mesh terrain wet-rock roughness",
    )
    blended_surface_ao = create_lerp(
        material,
        surface_masks,
        "R",
        rock_masks,
        "R",
        scree_mask,
        "",
        -320,
        940,
        "mesh terrain forest/scree surface ao",
    )
    wet_surface_ao = create_lerp(
        material,
        blended_surface_ao,
        "",
        rock_masks,
        "R",
        wet_rock_mask,
        "",
        -80,
        1000,
        "mesh terrain wet-rock ambient occlusion",
    )
    albedo_gain = create_scalar_parameter(material, "TerrainAlbedoGain", 1.28, -900, -40)
    macro_base_color_gain = create_multiply(
        material,
        macro_base_color,
        "",
        albedo_gain,
        "",
        380,
        -240,
        "mesh terrain macro base color gain",
    )
    textured_base_color = create_multiply(
        material,
        macro_base_color_gain,
        "",
        wet_surface_base,
        "",
        380,
        -40,
        "mesh terrain surface detail color",
    )
    final_base_color = create_lerp(
        material,
        macro_base_color_gain,
        "",
        textured_base_color,
        "",
        create_scalar_parameter(material, "TerrainSurfaceDetailStrength", detail_strength * 0.42, -620, 100),
        "",
        620,
        -140,
        "mesh terrain macro/detail base color",
    )
    if not unreal.MaterialEditingLibrary.connect_material_property(
        final_base_color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    ):
        raise RuntimeError("Unable to connect mesh terrain rock BaseColor")
    if not unreal.MaterialEditingLibrary.connect_material_property(
        wet_surface_normals,
        "",
        unreal.MaterialProperty.MP_NORMAL,
    ):
        raise RuntimeError("Unable to connect mesh terrain Normal")

    terrain_roughness = create_lerp(
        material,
        create_scalar_parameter(material, "TerrainRoughness", 0.84, -620, 280),
        "",
        wet_surface_roughness,
        "",
        create_scalar_parameter(material, "TerrainSurfaceRoughnessStrength", roughness_strength, -620, 460),
        "",
        -80,
        340,
        "mesh terrain roughness",
    )

    connect_material_property(
        material,
        terrain_roughness,
        unreal.MaterialProperty.MP_ROUGHNESS,
        "mesh terrain Roughness",
    )
    terrain_ao = create_lerp(
        material,
        create_scalar_parameter(material, "TerrainAmbientOcclusion", 0.92, -620, 580),
        "",
        wet_surface_ao,
        "",
        create_scalar_parameter(material, "TerrainSurfaceAmbientOcclusionStrength", ao_strength, -620, 760),
        "",
        -80,
        620,
        "mesh terrain ambient occlusion",
    )
    connect_material_property(
        material,
        terrain_ao,
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
