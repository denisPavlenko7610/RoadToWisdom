import unreal

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not LES.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype"):
    raise SystemExit

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
if ws is None:
    unreal.log_warning("[GM] WorldSettings not found")
    raise SystemExit

gm = unreal.load_class(None, "/Script/RoadToWisdom.RoadToWisdomQuietRoadGameMode")
ws.set_editor_property("default_game_mode", gm)
unreal.log_warning(f"[GM] set: {ws.get_editor_property('default_game_mode')}")

if not LES.save_current_level():
    unreal.log_warning("[GM] save failed")
unreal.log_warning("[GM] DONE")
