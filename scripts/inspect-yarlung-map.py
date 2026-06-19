import unreal
from pathlib import Path


MAP_PATH = "/Game/Generated/YarlungLandscape/YarlungLandscape_Level"
LANDSCAPE_MATERIAL_PATH = "/Game/Generated/Materials/M_YarlungLandscapeGround.M_YarlungLandscapeGround"
CLIFF_MATERIAL_PATH = "/Game/Generated/Materials/M_YarlungCliffRock.M_YarlungCliffRock"
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

    material = unreal.EditorAssetLibrary.load_asset(LANDSCAPE_MATERIAL_PATH)
    cliff_material = unreal.EditorAssetLibrary.load_asset(CLIFF_MATERIAL_PATH)
    emit(f"[YARLUNG-INSPECT] material={object_path(material)}")
    if material:
        emit(f"[YARLUNG-INSPECT] material_class={material.get_class().get_name()}")
    emit(f"[YARLUNG-INSPECT] cliff_material={object_path(cliff_material)}")

    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    landscapes = [actor for actor in actors if actor.get_class().get_name().startswith("Landscape")]
    emit(f"[YARLUNG-INSPECT] world={object_path(world)} actor_count={len(actors)} landscape_count={len(landscapes)}")
    if len(landscapes) != 1:
        raise RuntimeError(f"Expected exactly one Landscape actor, found {len(landscapes)}")

    for actor in landscapes:
        actor_material = actor.get_editor_property("landscape_material")
        emit(f"[YARLUNG-INSPECT] landscape={actor.get_actor_label()} material={object_path(actor_material)}")
        if actor_material != material:
            raise RuntimeError(f"Landscape material mismatch: {object_path(actor_material)}")
        components = actor.get_components_by_class(unreal.LandscapeComponent)
        emit(f"[YARLUNG-INSPECT] landscape_components={len(components)}")
        if not components:
            raise RuntimeError("Landscape has no components")
        for index, component in enumerate(components[:8]):
            component_material = component.get_material(0)
            override_material = component.get_editor_property("override_material")
            emit(
                f"[YARLUNG-INSPECT] component[{index}] material={object_path(component_material)} "
                f"override={object_path(override_material)}"
            )
            if component_material != material:
                raise RuntimeError(f"Landscape component {index} material mismatch: {object_path(component_material)}")
            if override_material:
                raise RuntimeError(f"Landscape component {index} has unexpected override: {object_path(override_material)}")

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

    cliff_actors = [actor for actor in actors if actor.get_class().get_name().startswith("YarlungCliffActor")]
    if len(cliff_actors) != 1:
        raise RuntimeError(f"Expected exactly one YarlungCliffActor, found {len(cliff_actors)}")
    for actor in cliff_actors:
        emit(f"[YARLUNG-INSPECT] cliff={actor.get_actor_label()} class={actor.get_class().get_name()}")
        for component in actor.get_components_by_class(unreal.MeshComponent):
            material_names = [object_path(component.get_material(slot)) for slot in range(component.get_num_materials())]
            emit(
                f"[YARLUNG-INSPECT] cliff_component={component.get_name()} "
                f"class={component.get_class().get_name()} hidden={component.get_editor_property('hidden_in_game')} "
                f"materials={material_names}"
            )

    write_report()


main()
