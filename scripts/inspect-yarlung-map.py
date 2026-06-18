import unreal


MAP_PATH = "/Game/Generated/YarlungLandscape/YarlungLandscape_Level"
LANDSCAPE_MATERIAL_PATH = "/Game/Generated/Materials/M_YarlungLandscapeGround.M_YarlungLandscapeGround"
FORBIDDEN_RIDE_COMPONENTS = {"CanyonTerrainMesh", "SnowCaps"}


def object_path(obj):
    return obj.get_path_name() if obj else "<none>"


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"Unable to load map: {MAP_PATH}")

    material = unreal.EditorAssetLibrary.load_asset(LANDSCAPE_MATERIAL_PATH)
    print(f"[YARLUNG-INSPECT] material={object_path(material)}")
    if material:
        print(f"[YARLUNG-INSPECT] material_class={material.get_class().get_name()}")

    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    landscapes = [actor for actor in actors if actor.get_class().get_name().startswith("Landscape")]
    print(f"[YARLUNG-INSPECT] world={object_path(world)} actor_count={len(actors)} landscape_count={len(landscapes)}")
    if len(landscapes) != 1:
        raise RuntimeError(f"Expected exactly one Landscape actor, found {len(landscapes)}")

    for actor in landscapes:
        actor_material = actor.get_editor_property("landscape_material")
        print(f"[YARLUNG-INSPECT] landscape={actor.get_actor_label()} material={object_path(actor_material)}")
        if actor_material != material:
            raise RuntimeError(f"Landscape material mismatch: {object_path(actor_material)}")
        components = actor.get_components_by_class(unreal.LandscapeComponent)
        print(f"[YARLUNG-INSPECT] landscape_components={len(components)}")
        if not components:
            raise RuntimeError("Landscape has no components")
        for index, component in enumerate(components[:8]):
            component_material = component.get_material(0)
            override_material = component.get_editor_property("override_material")
            print(
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
        print(f"[YARLUNG-INSPECT] ride={actor.get_actor_label()} class={actor.get_class().get_name()}")
        for component in actor.get_components_by_class(unreal.MeshComponent):
            if component.get_name() in FORBIDDEN_RIDE_COMPONENTS:
                raise RuntimeError(f"Forbidden legacy component is still present: {component.get_name()}")
            material_names = [object_path(component.get_material(slot)) for slot in range(component.get_num_materials())]
            print(
                f"[YARLUNG-INSPECT] ride_component={component.get_name()} "
                f"class={component.get_class().get_name()} hidden={component.get_editor_property('hidden_in_game')} "
                f"materials={material_names}"
            )


main()
