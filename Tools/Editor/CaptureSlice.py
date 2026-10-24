"""Real rendered editor screenshots; temporary shot poses are never saved."""
import json
import math
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from QuietRoadLayout import road_y

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert les.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()
van = next(a for a in actors if "QuietRoad.Van" in [str(t) for t in a.tags])
original = van.get_actor_transform()
out = Path(unreal.Paths.project_saved_dir()) / "Screenshots" / "M22"
out.mkdir(parents=True, exist_ok=True)
shots = [
    ("CamperRoad", (6500, road_y(6500), 12), (5500, road_y(6500) - 1400, 630), (6500, road_y(6500), 160)),
    ("CamperDriverDoor", (6500, road_y(6500), 12), (6900, road_y(6500) - 1050, 410), (6500, road_y(6500), 160)),
    ("RelayForest", None, (22400, -2100, 1000), (24000, 300, 370)),
]
state = {"frame": 0, "index": 0, "handle": None}


def pose(shot):
    name, vanpos, cam, target = shot
    if vanpos:
        van.set_actor_location(unreal.Vector(*vanpos), False, False)
        van.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    direction = unreal.Vector(*target) - unreal.Vector(*cam)
    rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*cam), unreal.Vector(*target))
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        unreal.Vector(*cam), rotation)


pose(shots[0])


def tick(dt):
    state["frame"] += 1
    # Wait for exposure, shaders and temporal accumulation at each viewpoint.
    if state["frame"] == 180:
        name = shots[state["index"]][0]
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, str(out / (name + ".png")))
        unreal.log("[CAPTURE] " + name)
    if state["frame"] >= 230:
        state["index"] += 1
        if state["index"] == len(shots):
            van.set_actor_transform(original, False, False)
            unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log("[CAPTURE-RESULT] DONE")
            unreal.SystemLibrary.quit_editor()
            return
        state["frame"] = 0
        pose(shots[state["index"]])


state["handle"] = unreal.register_slate_post_tick_callback(tick)