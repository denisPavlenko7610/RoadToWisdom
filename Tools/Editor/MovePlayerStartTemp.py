import unreal
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not LES.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype"):
    raise SystemExit
for a in EAS.get_all_level_actors():
    if a.get_actor_label() == "QR_PlayerStart":
        a.set_actor_location_and_rotation(
            unreal.Vector(23300, 0, 100), unreal.Rotator(pitch=0, yaw=-90, roll=0), False, False)
        unreal.log_warning("[TMP] player start moved to relay")
if not LES.save_current_level():
    unreal.log_warning("[TMP] save failed")
