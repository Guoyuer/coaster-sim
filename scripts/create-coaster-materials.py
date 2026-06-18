import unreal


PACKAGE_PATH = "/Game/Generated/Materials"
MATERIAL_NAME = "M_CoasterTint"
ASSET_PATH = f"{PACKAGE_PATH}/{MATERIAL_NAME}"


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def main():
    ensure_folder(PACKAGE_PATH)
    material = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if material is None:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = asset_tools.create_asset(
            MATERIAL_NAME,
            PACKAGE_PATH,
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )

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
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


main()
