"""Route acceptance drive: the Utility Van follows the real Main road spline.

Runs in an editor Simulate session (real Chaos physics, no human PIE). The van is
spawned at the first Main-route point; a simple waypoint autopilot steers it along
the road polyline from Saved/WorldGeneration/World.json. Acceptance checks:
progress along the route, no sustained stall under throttle, lateral offset within
the road bed, sane average speed. Result: Saved/WorldGeneration/VanRouteMain.json.
"""
import json
import math
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
WORLD_JSON = ROOT / "Saved/WorldGeneration/World.json"
OUT = ROOT / "Saved/WorldGeneration/VanRouteMain.json"

VAN_CLASS_PATH = "/Game/Vehicles/UtilityVan/Blueprints/BP_UtilityVan_Pawn.BP_UtilityVan_Pawn"
MAX_SECONDS = 480.0        # wall-clock budget for the whole run
WAYPOINT_RADIUS_M = 22.0   # advance to the next route point inside this radius
SAMPLE_EVERY = 0.5         # seconds between samples
STALL_WINDOW_S = 8.0       # moving slower than this while throttle is applied...
STALL_SPEED_CM = 40.0      # ...for this long counts as stuck
OFFROAD_LIMIT_M = 7.5      # half_width 4.5 m + shoulder allowance


def route_points():
    data = json.loads(WORLD_JSON.read_text())
    road = next(r for r in data["roads"] if r["name"] == "Main")
    # world.json stores metres; UE works in centimetres
    pts = [(p[0] * 100.0, p[1] * 100.0, p[2] * 100.0) for p in road["points"]]
    length = sum(math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1))
    return pts, length / 100.0


def lateral_offset_cm(pos, pts, idx):
    """Distance in cm from pos to the polyline segment near waypoint idx."""
    best = float("inf")
    for i in range(max(0, idx - 6), min(len(pts) - 1, idx + 12)):
        ax, ay = pts[i][0], pts[i][1]
        bx, by = pts[i + 1][0], pts[i + 1][1]
        dx, dy = bx - ax, by - ay
        dd = dx * dx + dy * dy
        t = 0.0 if dd == 0 else max(0.0, min(1.0, ((pos.x - ax) * dx + (pos.y - ay) * dy) / dd))
        best = min(best, math.hypot(pos.x - ax - t * dx, pos.y - ay - t * dy))
    return best


def spawn_van(pts):
    """Spawn into the editor world BEFORE Simulate: Simulate ticks the same world,
    so the placed van gets physics without needing a player controller."""
    van_bp = unreal.load_object(None, VAN_CLASS_PATH)
    van_class = van_bp.generated_class() if van_bp else None
    if not van_class:
        raise RuntimeError("BP_UtilityVan_Pawn class not found")
    loc = unreal.Vector(*pts[0])
    to_next = unreal.Vector(*pts[min(3, len(pts) - 1)]) - loc
    rot = unreal.MathLibrary.find_look_at_rotation(loc, loc + to_next)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    van = eas.spawn_actor_from_class(van_class, loc, rot)
    if not van:
        raise RuntimeError("van spawn failed")
    van.set_actor_label("QRW_RouteVan_TEMP")
    return van


def main():
    pts, total_km = route_points()
    unreal.log_warning("[ROUTE] Main route: %d points, %.0f m" % (len(pts), total_km))

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.load_level("/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld"):
        raise RuntimeError("map load failed")

    state = {
        "handle": None, "phase": "wait_world", "start_wall": time.monotonic(),
        "pts": pts, "wp": 1, "samples": [], "last_sample": 0.0,
        "throttle_since": None, "finished": False,
    }
    state["van"] = spawn_van(state["pts"])
    # Real PIE via the official subsystem API: Chaos drive works under a possessed
    # pawn (Simulate does not tick vehicle input without a controller).
    les.editor_request_begin_play()

    def tick(_dt):
        if state["finished"]:
            return
        if time.monotonic() - state["start_wall"] > MAX_SECONDS:
            finish("timeout")

        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if state["phase"] == "wait_world":
            if not world:
                return
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            if not controller:
                return
            # PIE duplicates the world: drive the PIE copy found by tag, not the
            # editor-world original we spawned before TogglePIE.
            tagged = unreal.GameplayStatics.get_all_actors_with_tag(world, "QRW_RouteVan_TEMP")
            if not tagged:
                return  # PIE copy not ready yet
            state["van_pie"] = tagged[0]
            controller.possess(state["van_pie"])
            movement = state["van_pie"].get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)
            if not movement:
                finish("no_movement_component")
                return
            state["movement"] = movement
            state["phase"] = "drive"
            unreal.log_warning("[ROUTE] van spawned, driving")

        van = state["van_pie"]
        movement = state["movement"]
        pos = van.get_actor_location()
        pts = state["pts"]

        # advance waypoints
        while state["wp"] < len(pts) - 1 and math.hypot(pos.x - pts[state["wp"]][0], pos.y - pts[state["wp"]][1]) < WAYPOINT_RADIUS_M * 100:
            state["wp"] += 1

        # steering toward the lookahead point
        look = pts[min(state["wp"] + 3, len(pts) - 1)]
        target = unreal.Vector(look[0], look[1], pos.z)
        to_target = target - pos
        to_target.normalize()
        forward = van.get_actor_forward_vector()
        cross = forward.x * to_target.y - forward.y * to_target.x
        dot = max(0.2, forward.x * to_target.x + forward.y * to_target.y)
        steer = max(-1.0, min(1.0, 2.2 * math.copysign(1.0, cross) * min(1.0, abs(cross) / dot * 1.4)))
        van.do_steering(steer)

        # curvature-aware speed target
        ahead = pts[min(state["wp"] + 14, len(pts) - 1)] if len(pts) > state["wp"] + 14 else pts[-1]
        curve = abs(math.hypot(ahead[0] - look[0], ahead[1] - look[1]))
        speed = movement.get_forward_speed()
        want = 1400.0 if curve > 160.0 else 900.0  # cm/s cruise, slower into bends
        if speed < want:
            van.do_throttle(0.85 if curve > 160.0 else 0.6)
            van.do_brake(0.0)
        else:
            van.do_throttle(0.0)
            van.do_brake(0.4)
        movement.set_handbrake_input(False)

        # sampling
        now = time.monotonic() - state["start_wall"]
        if now - state["last_sample"] >= SAMPLE_EVERY:
            state["last_sample"] = now
            state["samples"].append({
                "t": round(now, 1), "wp": state["wp"],
                "x": round(pos.x / 100, 1), "y": round(pos.y / 100, 1), "z": round(pos.z / 100, 1),
                "v_kmh": round(abs(speed) * 0.036, 1),
                "off_m": round(lateral_offset_cm(pos, pts, state["wp"]) / 100, 2),
                "steer": round(steer, 2),
            })

        # stall detection while throttle is being applied
        if speed < STALL_SPEED_CM:
            state["throttle_since"] = state["throttle_since"] or now
            if now - state["throttle_since"] > STALL_WINDOW_S:
                finish("stalled")
                return
        else:
            state["throttle_since"] = None

        if state["wp"] >= len(pts) - 2:
            finish("completed")

    def finish(reason):
        if state["finished"]:
            return
        state["finished"] = True
        samples = state["samples"]
        pts = state["pts"]
        wp = state["wp"]
        progress = wp / max(1, len(pts) - 1)
        total_m = total_km * 100
        # travelled estimate: waypoint arc length up to wp
        arc = sum(math.dist(pts[i], pts[i + 1]) for i in range(wp)) / 100.0
        offs = [s["off_m"] for s in samples] or [0]
        vels = [s["v_kmh"] for s in samples] or [0]
        checks = {
            "reason": reason,
            "route_points": len(pts),
            "route_length_m": round(total_m, 0),
            "reached_waypoint": wp,
            "progress": round(progress, 3),
            "travelled_m": round(arc, 0),
            "avg_kmh": round(sum(vels) / len(vels), 1),
            "max_kmh": round(max(vels), 1),
            "offroad_samples_over_limit": sum(1 for o in offs if o > OFFROAD_LIMIT_M),
            "offroad_fraction": round(sum(1 for o in offs if o > OFFROAD_LIMIT_M) / max(1, len(offs)), 3),
        }
        checks["passed"] = (
            reason == "completed"
            and progress >= 0.95
            and checks["offroad_fraction"] <= 0.05
            and checks["avg_kmh"] >= 12.0
        )
        checks["samples_head"] = samples[:8]
        checks["samples_tail"] = samples[-8:]
        OUT.write_text(json.dumps(checks, indent=2))
        unreal.log_warning("[ROUTE-RESULT] " + json.dumps(checks))
        try:
            unreal.unregister_slate_post_tick_callback(state["handle"])
        except Exception:
            pass
        try:
            state["van"].destroy_actor()
        except Exception:
            pass
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        unreal.SystemLibrary.quit_editor()

    state["handle"] = unreal.register_slate_post_tick_callback(tick)


main()
