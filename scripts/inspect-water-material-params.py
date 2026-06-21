import unreal


MATERIAL_PATH = "/Water/Materials/WaterSurface/Water_Material_River.Water_Material_River"


def main():
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material is None:
        raise RuntimeError(f"Unable to load {MATERIAL_PATH}")

    print(f"[WATER-PARAMS] material={material.get_path_name()} class={material.get_class().get_name()}")
    for getter, label in (
        (unreal.MaterialEditingLibrary.get_scalar_parameter_names, "scalar"),
        (unreal.MaterialEditingLibrary.get_vector_parameter_names, "vector"),
        (unreal.MaterialEditingLibrary.get_texture_parameter_names, "texture"),
    ):
        try:
            names = getter(material)
        except Exception as exc:
            print(f"[WATER-PARAMS] {label}_error={exc}")
            continue
        print(f"[WATER-PARAMS] {label}_count={len(names)}")
        for name in names:
            print(f"[WATER-PARAMS] {label}={name}")


main()
