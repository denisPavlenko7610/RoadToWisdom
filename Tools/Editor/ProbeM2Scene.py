import unreal

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not LES.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype"):
    unreal.log_warning("[PROBE4] MAP LOAD FAILED")
    raise SystemExit

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out = []
for a in EAS.get_all_level_actors():
    if a.get_class().get_name() == "RoadToWisdomDecorCluster":
        for prop in ("TreeTrunks", "TreeCrowns", "Bushes", "Rocks"):
            comp = a.get_editor_property(prop)
            serialized = comp.get_editor_property("per_instance_sm_data")
            first = serialized[0].transform if serialized else None
            out.append(f"{prop}: serialized={len(serialized)} count={comp.get_instance_count()} first={first}")

unreal.log_warning("[PROBE4] ||| " + " ||| ".join(out))
