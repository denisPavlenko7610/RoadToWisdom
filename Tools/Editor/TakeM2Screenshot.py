import unreal

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")

loc = unreal.Vector(13000, -3000, 1400)
rot = unreal.Rotator(pitch=-20, yaw=35, roll=0)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)

state = {"ticks": 0, "handle": None}
SHOT_TICKS = {5: "M2_A.png", 25: "M2_B.png", 60: "M2_C.png", 120: "M2_Reference.png"}

def on_tick(delta_seconds):
    state["ticks"] += 1
    name = SHOT_TICKS.pop(state["ticks"], None)
    if name:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, name)
        unreal.log_warning(f"[SCREENSHOT] requested {name} at tick {state['ticks']}")
    if not SHOT_TICKS and state["handle"]:
        unreal.unregister_slate_post_tick_callback(state["handle"])
        state["handle"] = None

state["handle"] = unreal.register_slate_post_tick_callback(on_tick)
