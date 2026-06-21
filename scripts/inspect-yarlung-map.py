import unreal
from pathlib import Path


MAP_PATH = "/Game/Generated/YarlungLandscape/YarlungLandscape_Level"
MESH_TERRAIN_MATERIAL_PATH = "/Game/Generated/Materials/M_YarlungMeshTerrain.M_YarlungMeshTerrain"
CORRIDOR_TERRAIN_STATIC_MESH_PATH = "/Game/Generated/YarlungLandscape/SM_YarlungCorridorTerrain.SM_YarlungCorridorTerrain"
FORBIDDEN_RIDE_COMPONENTS = {
    "CanyonTerrainMesh",
    "SnowCaps",
    "RiverSurface",
    "Rapids",
    "MistBands",
    "RiverRibbonMesh",
    "FoamRibbonMesh",
}


REPORT_LINES = []


def emit(message):
    REPORT_LINES.append(message)
    unreal.log(message)


def write_report():
    report_path = Path(unreal.Paths.project_saved_dir()) / "Diagnostics" / "yarlung-map-inspect.txt"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(REPORT_LINES) + "\n", encoding="utf-8")


def object_path(obj):
    return obj.get_path_name() if obj else "<none>"


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"Unable to load map: {MAP_PATH}")

    mesh_terrain_material = unreal.EditorAssetLibrary.load_asset(MESH_TERRAIN_MATERIAL_PATH)
    corridor_terrain_static_mesh = unreal.EditorAssetLibrary.load_asset(CORRIDOR_TERRAIN_STATIC_MESH_PATH)
    emit(f"[YARLUNG-INSPECT] mesh_terrain_material={object_path(mesh_terrain_material)}")
    emit(f"[YARLUNG-INSPECT] corridor_terrain_static_mesh={object_path(corridor_terrain_static_mesh)}")
    if not corridor_terrain_static_mesh:
        raise RuntimeError(f"Missing Yarlung corridor terrain StaticMesh: {CORRIDOR_TERRAIN_STATIC_MESH_PATH}")
    corridor_nanite_status = "<unavailable>"
    if hasattr(corridor_terrain_static_mesh, "is_nanite_enabled"):
        corridor_nanite_status = corridor_terrain_static_mesh.is_nanite_enabled()
    emit(f"[YARLUNG-INSPECT] corridor_terrain_static_mesh_nanite={corridor_nanite_status}")

    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    landscapes = [actor for actor in actors if actor.get_class().get_name().startswith("Landscape")]
    emit(f"[YARLUNG-INSPECT] world={object_path(world)} actor_count={len(actors)} landscape_count={len(landscapes)}")
    if landscapes:
        raise RuntimeError(f"Runtime map should not include square source Landscape actors by default, found {len(landscapes)}")

    mesh_terrain_actors = [actor for actor in actors if actor.get_class().get_name().startswith("YarlungMeshTerrainActor")]
    if len(mesh_terrain_actors) != 1:
        raise RuntimeError(f"Expected exactly one runtime corridor YarlungMeshTerrainActor, found {len(mesh_terrain_actors)}")
    for actor in mesh_terrain_actors:
        emit(f"[YARLUNG-INSPECT] mesh_terrain={actor.get_actor_label()} class={actor.get_class().get_name()}")
        if actor.get_actor_label() != "YarlungCorridorTerrainScenery":
            raise RuntimeError(f"Unexpected runtime terrain actor label: {actor.get_actor_label()}")
        mesh_components = [
            component
            for component in actor.get_components_by_class(unreal.StaticMeshComponent)
            if component.get_name() == "YarlungMeshTerrain"
        ]
        if len(mesh_components) != 1:
            raise RuntimeError(f"Expected one YarlungMeshTerrain component, found {len(mesh_components)}")
        for component in mesh_components:
            static_mesh = component.get_editor_property("static_mesh")
            if static_mesh != corridor_terrain_static_mesh:
                raise RuntimeError(f"Yarlung corridor terrain component uses wrong mesh: {object_path(static_mesh)}")
            material_names = [object_path(component.get_material(slot)) for slot in range(component.get_num_materials())]
            emit(
                f"[YARLUNG-INSPECT] mesh_terrain_component={component.get_name()} "
                f"class={component.get_class().get_name()} hidden={component.get_editor_property('hidden_in_game')} "
                f"static_mesh={object_path(static_mesh)} materials={material_names}"
            )

    ride_actors = [actor for actor in actors if actor.get_class().get_name().startswith("CoasterRideActor")]
    if len(ride_actors) != 1:
        raise RuntimeError(f"Expected exactly one CoasterRideActor, found {len(ride_actors)}")
    for actor in ride_actors:
        emit(f"[YARLUNG-INSPECT] ride={actor.get_actor_label()} class={actor.get_class().get_name()}")
        for component in actor.get_components_by_class(unreal.MeshComponent):
            if component.get_name() in FORBIDDEN_RIDE_COMPONENTS:
                raise RuntimeError(f"Forbidden legacy component is still present: {component.get_name()}")
            material_names = [object_path(component.get_material(slot)) for slot in range(component.get_num_materials())]
            instance_suffix = ""
            if hasattr(component, "get_instance_count"):
                instance_suffix = f" instances={component.get_instance_count()}"
            emit(
                f"[YARLUNG-INSPECT] ride_component={component.get_name()} "
                f"class={component.get_class().get_name()} hidden={component.get_editor_property('hidden_in_game')} "
                f"materials={material_names}{instance_suffix}"
            )
        for component in actor.get_components_by_class(unreal.ExponentialHeightFogComponent):
            location = component.get_editor_property("relative_location")
            emit(
                f"[YARLUNG-INSPECT] fog_component={component.get_name()} "
                f"relative_z_cm={location.z:.1f}"
            )

    river_actors = [actor for actor in actors if actor.get_class().get_name().startswith("YarlungRiverActor")]
    if len(river_actors) != 1:
        raise RuntimeError(f"Expected exactly one YarlungRiverActor, found {len(river_actors)}")
    for actor in river_actors:
        emit(f"[YARLUNG-INSPECT] river={actor.get_actor_label()} class={actor.get_class().get_name()}")
        for component in actor.get_components_by_class(unreal.MeshComponent):
            material_names = [object_path(component.get_material(slot)) for slot in range(component.get_num_materials())]
            emit(
                f"[YARLUNG-INSPECT] river_component={component.get_name()} "
                f"class={component.get_class().get_name()} hidden={component.get_editor_property('hidden_in_game')} "
                f"materials={material_names}"
            )

    scenery_actors = [actor for actor in actors if actor.get_class().get_name().startswith("YarlungSceneryActor")]
    if len(scenery_actors) != 1:
        raise RuntimeError(f"Expected exactly one YarlungSceneryActor, found {len(scenery_actors)}")
    for actor in scenery_actors:
        emit(f"[YARLUNG-INSPECT] scenery={actor.get_actor_label()} class={actor.get_class().get_name()}")
        total_instances = 0
        rock_instances = 0
        cliff_face_instances = 0
        forest_shrub_instances = 0
        for component in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            material_names = [object_path(component.get_material(slot)) for slot in range(component.get_num_materials())]
            instance_count = component.get_instance_count()
            total_instances += instance_count
            if component.get_name() == "RockOutcrops":
                rock_instances = instance_count
            if component.get_name().startswith("CliffRockFaces"):
                cliff_face_instances += instance_count
            if component.get_name().startswith("ForestShrubs"):
                forest_shrub_instances += instance_count
            emit(
                f"[YARLUNG-INSPECT] scenery_component={component.get_name()} "
                f"class={component.get_class().get_name()} hidden={component.get_editor_property('hidden_in_game')} "
                f"materials={material_names} instances={instance_count}"
            )
        if rock_instances < 500:
            raise RuntimeError(f"Yarlung rock scatter is too sparse: {rock_instances} instances")
        if cliff_face_instances < 400:
            raise RuntimeError(f"Yarlung cliff-face asset scatter is too sparse: {cliff_face_instances} instances")
        if forest_shrub_instances < 1000:
            raise RuntimeError(f"Yarlung forest shrub asset scatter is too sparse: {forest_shrub_instances} instances")

    cliff_actors = [actor for actor in actors if actor.get_class().get_name().startswith("YarlungCliffActor")]
    if cliff_actors:
        raise RuntimeError(f"Unexpected YarlungCliffActor still present: {len(cliff_actors)}")

    write_report()


main()
