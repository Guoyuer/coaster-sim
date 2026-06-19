import unreal


"""Export the imported Yarlung macro albedo texture so screenshots can be compared to the UE asset."""

TEXTURE_PATH = "/Game/Generated/Materials/YarlungMacro/T_YarlungMacroAlbedo.T_YarlungMacroAlbedo"
EXPORT_PATH = "Saved/T_YarlungMacroAlbedo_export.tga"


def main():
    texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
    if texture is None:
        raise RuntimeError(f"Missing texture: {TEXTURE_PATH}")

    print(
        "[YARLUNG-TEXTURE] "
        f"path={texture.get_path_name()} "
        f"srgb={texture.get_editor_property('srgb')} "
        f"compression={texture.get_editor_property('compression_settings')} "
        f"lod_group={texture.get_editor_property('lod_group')}"
    )

    task = unreal.AssetExportTask()
    task.set_editor_property("object", texture)
    task.set_editor_property("filename", unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + EXPORT_PATH))
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("exporter", unreal.TextureExporterTGA())
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError(f"Unable to export texture to {EXPORT_PATH}")
    print(f"[YARLUNG-TEXTURE] exported={EXPORT_PATH}")


main()
