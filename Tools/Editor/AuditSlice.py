"""Read the saved scene before making content changes. Never saves the map."""
import json
import os
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert les.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report = []
for actor in actors:
    entry = dict(label=actor.get_actor_label(), cls=actor.get_class().get_path_name(),
                 transform=str(actor.get_actor_transform()))
    if "QuietRoad.Van" in [str(t) for t in actor.tags]:
        mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        entry["skeletal_mesh"] = mesh.skeletal_mesh_asset.get_path_name()
        entry["physics_asset"] = str(mesh.skeletal_mesh_asset.get_editor_property("physics_asset"))
        entry["materials"] = [str(m.material_slot_name) for m in mesh.skeletal_mesh_asset.materials]
        entry["bounds"] = str(actor.get_actor_bounds(False))
        entry["bones"] = {str(mesh.get_bone_name(i)): str(
            mesh.get_socket_transform(mesh.get_bone_name(i), unreal.RelativeTransformSpace.RTS_COMPONENT))
            for i in range(mesh.get_num_bones())}
        entry["cameras"] = [{"name": c.get_name(), "transform": str(c.get_relative_transform())}
                            for c in actor.get_components_by_class(unreal.CameraComponent)]
    if "DecorCluster" in actor.get_class().get_name():
        entry["hism"] = {p: actor.get_editor_property(p).get_instance_count()
                         for p in ("TreeTrunks", "TreeCrowns", "Bushes", "Rocks")}
    if actor.get_class().get_name()=='RoadToWisdomCamperBody':
        mesh=actor.get_editor_property('Body')
        entry['body_bounds']=str(mesh.get_local_bounds())
        entry['body_materials']=[str(mesh.get_material(i)) for i in range(mesh.get_num_materials())]
        for i in range(mesh.get_num_materials()):
            mat=mesh.get_material(i)
            unreal.log('[MAT] '+str(mat)+' '+str(unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mat,'Tint')))
    report.append(entry)
path = os.path.join(unreal.Paths.project_saved_dir(), "AuditSlice.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(report, f, indent=2)
unreal.log("[AUDIT] " + path)