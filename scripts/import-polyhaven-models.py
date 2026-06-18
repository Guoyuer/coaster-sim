import unreal


MODEL_PACKAGE_PATH = "/Game/Generated/Models/Boulder01"
BOULDER_SOURCE = "SourceAssets/PolyHaven/boulder_01/boulder_01_1k.fbx"


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def import_static_mesh(source_relative_path, destination_path):
    source = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir() + source_relative_path)
    if not unreal.Paths.file_exists(source):
        raise RuntimeError(f"Missing model source: {source}")

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", True)
    options.static_mesh_import_data.set_editor_property("auto_generate_collision", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", destination_path)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset = unreal.EditorAssetLibrary.load_asset(f"{destination_path}/boulder_01_1k")
    if asset is None:
        raise RuntimeError(f"Unable to load imported boulder mesh in {destination_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    print(f"[POLYHAVEN-IMPORT] imported={asset.get_path_name()}")


def main():
    ensure_folder(MODEL_PACKAGE_PATH)
    import_static_mesh(BOULDER_SOURCE, MODEL_PACKAGE_PATH)


main()
