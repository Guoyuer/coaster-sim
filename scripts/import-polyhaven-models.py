import unreal


MODEL_ASSETS = [
    {
        "name": "boulder_01_1k",
        "source": "SourceAssets/PolyHaven/boulder_01/boulder_01_1k.fbx",
        "destination": "/Game/Generated/Models/Boulder01",
    },
    {
        "name": "rock_face_01_1k",
        "source": "SourceAssets/PolyHaven/rock_face_01/rock_face_01_1k.fbx",
        "destination": "/Game/Generated/Models/RockFace01",
    },
    {
        "name": "rock_face_02_1k",
        "source": "SourceAssets/PolyHaven/rock_face_02/rock_face_02_1k.fbx",
        "destination": "/Game/Generated/Models/RockFace02",
    },
    {
        "name": "shrub_03_1k",
        "source": "SourceAssets/PolyHaven/shrub_03/shrub_03_1k.fbx",
        "destination": "/Game/Generated/Models/Shrub03",
    },
    {
        "name": "shrub_04_1k",
        "source": "SourceAssets/PolyHaven/shrub_04/shrub_04_1k.fbx",
        "destination": "/Game/Generated/Models/Shrub04",
    },
]


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def import_static_mesh(asset):
    source_relative_path = asset["source"]
    destination_path = asset["destination"]
    asset_name = asset["name"]
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

    imported_asset = unreal.EditorAssetLibrary.load_asset(f"{destination_path}/{asset_name}")
    if imported_asset is None:
        raise RuntimeError(f"Unable to load imported mesh {asset_name} in {destination_path}")
    unreal.EditorAssetLibrary.save_loaded_asset(imported_asset)
    print(f"[POLYHAVEN-IMPORT] imported={imported_asset.get_path_name()}")


def main():
    for asset in MODEL_ASSETS:
        ensure_folder(asset["destination"])
        import_static_mesh(asset)


main()
