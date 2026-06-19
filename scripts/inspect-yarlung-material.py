import unreal


"""Print the generated Yarlung landscape material graph for headless diagnosis."""

MATERIAL_PATH = "/Game/Generated/Materials/M_YarlungLandscapeGround.M_YarlungLandscapeGround"


def object_path(obj):
    return obj.get_path_name() if obj else "<none>"


def main():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material is None:
        raise RuntimeError(f"Missing material: {MATERIAL_PATH}")

    print(f"[YARLUNG-MATERIAL] material={object_path(material)}")
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    for expression in expressions:
        texture = None
        if hasattr(expression, "texture"):
            texture = expression.get_editor_property("texture")
        print(
            f"[YARLUNG-MATERIAL] expression={expression.get_name()} "
            f"class={expression.get_class().get_name()} texture={object_path(texture)}"
        )


main()
