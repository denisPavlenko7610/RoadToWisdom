"""Read-only audit of the open-world map: landscape, partition, roads, POIs."""
import unreal
from collections import Counter

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAP = "/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld"

if not LES.load_level(MAP):
    unreal.log_warning("[AUDIT] LOAD FAILED")
    raise SystemExit

actors = EAS.get_all_level_actors()
out = ["actors=%d" % len(actors)]
classes = Counter(a.get_class().get_name() for a in actors)
out.append("classes=%s" % dict(classes.most_common(15)))

for a in actors:
    cn = a.get_class().get_name()
    label = a.get_actor_label()
    if "Landscape" in cn:
        loc = a.get_actor_location(); scale = a.get_actor_scale3d()
        out.append("LANDSCAPE %s loc=(%.0f,%.0f,%.0f) scale=(%.1f,%.1f,%.1f)" % (label, loc.x, loc.y, loc.z, scale.x, scale.y, scale.z))
    elif cn in ("WorldSettings",):
        ws = a
        gm = ws.get_editor_property("default_game_mode")
        out.append("GAMEMODE=%s" % (gm.get_name() if gm else "None"))
    elif "Spline" in cn:
        out.append("SPLINE %s" % label)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
try:
    wp = world.get_editor_property("world_partition")
    if wp:
        out.append("WORLDPARTITION enabled")
        try:
            runtime = wp.get_editor_property("runtime_hash")
            out.append("WP runtime hash exists")
        except Exception:
            pass
    else:
        out.append("WORLDPARTITION none")
except Exception as e:
    out.append("WORLDPARTITION err %s" % e)

unreal.log_warning("[AUDIT] ||| " + " ||| ".join(out))
