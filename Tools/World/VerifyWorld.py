"""Collision- and asset-based acceptance against the REAL loaded UE world."""
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
LIB = unreal.QRWorldEditorLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def signature(actors):
    result = []
    for actor in actors:
        for component in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            transforms = []
            for i in range(component.get_instance_count()):
                t = component.get_instance_transform(i, world_space=True)
                transforms.append([round(v, 2) for v in (t.translation.x, t.translation.y, t.translation.z,
                                                         t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w,
                                                         t.scale3d.x, t.scale3d.y, t.scale3d.z)])
            result.append((actor.get_actor_label(), component.static_mesh.get_path_name(), transforms))
    return hashlib.sha256(json.dumps(sorted(result), sort_keys=True).encode()).hexdigest()


def main():
    manifest = json.loads((ROOT / "Saved/WorldGeneration/World.json").read_text())
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert manifest["settings"]["Map"] in world.get_path_name(), world.get_path_name()
    assert LIB.load_editor_region(unreal.Vector(0, 0, 0), unreal.Vector(500000, 500000, 200000), True)
    actors = EAS.get_all_level_actors()
    labels = [a.get_actor_label() for a in actors if "QRWorld.Generated" in [str(t) for t in a.tags]]
    assert len(labels) == len(set(labels)), "duplicate generated labels"
    components = sum(
        len(a.get_components_by_class(unreal.LandscapeComponent)) for a in actors if
        isinstance(a, unreal.LandscapeProxy))
    for actor in actors:
        if isinstance(actor, unreal.LandscapeStreamingProxy):
            center, extent = actor.get_actor_bounds(False)
            assert center.z > -10000 and extent.z > 1, ("Invalid rendered heightmap/bounds", actor.get_actor_label(), center, extent)
    expected = ((manifest["resolution"] - 1) // 126) ** 2
    assert components == expected, (components, expected)
    assert not any("RoadToWisdom" in a.get_class().get_name() or "SportsCar" in a.get_class().get_name() for a in
                   actors), "gameplay/template actors in environment map"
    counts = Counter()
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            kind = actor.get_actor_label().split("_")[1]
            counts[kind] += comp.get_instance_count()
            assert comp.static_mesh
            if kind in ("Grass", "Shrubs", "RockSmall"):
                assert comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert dict(counts) == manifest["metrics"]["instances"], dict(counts)
    max_gap = 0;
    surface_checks = 0;
    clearance_checks = 0;
    errors = []
    for road in manifest["roads"]:
        points = road["points"]
        for index in range(0, len(points), 3):
            p = points[index];
            x, y, z = [v * 100 for v in p]
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, z + 200),
                                                         unreal.Vector(x, y, z - 300),
                                                         unreal.TraceTypeQuery.ECC_VISIBILITY, False, [],
                                                         unreal.DrawDebugTrace.NONE)
            surface_checks += 1
            if not hit:
                errors.append(f"No Landscape collision: {road['name']} {index}")
                continue
            data = hit.to_dict()
            if not isinstance(data["hit_actor"], unreal.LandscapeProxy):
                errors.append("Road blocked by " + data["hit_actor"].get_actor_label())
                continue
            gap = z - data["location"].z
            max_gap = max(max_gap, abs(gap))
            if abs(gap) > 12: errors.append(f"Road surface gap {gap:.1f} cm at {road['name']}:{index}")
            if index + 1 < len(points):
                q = points[index + 1];
                yaw = math.degrees(math.atan2(q[1] - p[1], q[0] - p[0]))
                center = unreal.Vector(x, y, z + 215)
                box = unreal.SystemLibrary.box_trace_single(world, center, center, unreal.Vector(250, 145, 160),
                                                            unreal.Rotator(pitch=0, yaw=yaw, roll=0),
                                                            unreal.TraceTypeQuery.ECC_VISIBILITY,
                                                            False, [], unreal.DrawDebugTrace.NONE)
                clearance_checks += 1
                if box: errors.append("Vehicle envelope blocked by " + box.to_dict()[
                    "hit_actor"].get_actor_label() + f" at {road['name']}:{index}")
    buildings = [a for a in actors if "QRWorld.POI" in [str(t) for t in a.tags]]
    for i, actor in enumerate(buildings):
        origin, extent = actor.get_actor_bounds(False)
        for other in buildings[i + 1:]:
            b, e = other.get_actor_bounds(False)
            if abs(origin.x - b.x) < extent.x + e.x and abs(origin.y - b.y) < extent.y + e.y:
                errors.append("Overlapping buildings: " + actor.get_actor_label() + " / " + other.get_actor_label())
        # Ignore every non-Landscape actor to compare actual foundation against terrain.
        ignore = [a for a in actors if not isinstance(a, unreal.LandscapeProxy)]
        hit = unreal.SystemLibrary.line_trace_single(world, origin + unreal.Vector(0, 0, 2000),
                                                     origin - unreal.Vector(0, 0, 3000),
                                                     unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignore,
                                                     unreal.DrawDebugTrace.NONE)
        if not hit:
            errors.append("No ground under " + actor.get_actor_label())
        elif abs((origin.z - extent.z) - hit.to_dict()["location"].z) > 60:
            errors.append("Floating/buried foundation: " + actor.get_actor_label())
    assert sum(a.get_actor_label() == "VehicleStart" for a in actors) == 1, "missing/duplicate VehicleStart"
    ignore = [a for a in actors if not isinstance(a, unreal.LandscapeProxy)]
    for actor in actors:
        name = actor.get_actor_label()
        if name not in ("VehicleStart", "ExplorerStart"): continue
        position = actor.get_actor_location()
        hit = unreal.SystemLibrary.line_trace_single(world,position+unreal.Vector(0,0,10000),
            position-unreal.Vector(0,0,10000),unreal.TraceTypeQuery.ECC_VISIBILITY,False,ignore,unreal.DrawDebugTrace.NONE)
        clearance = position.z-hit.to_dict()["location"].z if hit else -10000
        if not 20 <= clearance <= 250: errors.append(f"Invalid grounded start {name}: {clearance:.1f} cm")
    result = dict(map=world.get_path_name(), landscape_components=components,
                  classes=dict(Counter(a.get_class().get_name() for a in actors)),
                  instances=dict(counts), hism_sha256=signature(actors), road_surface_checks=surface_checks,
                  vehicle_envelope_checks=clearance_checks, max_road_gap_cm=max_gap, building_count=len(buildings),
                  errors=errors)
    path = ROOT / "Saved/WorldGeneration" / ("Verified_" + str(manifest["settings"]["WorldSize"]) + ".json")
    path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("[WORLD-VERIFY] " + json.dumps({k: v for k, v in result.items() if k != "classes"}))
    assert not errors, errors[:15]
    return result


if __name__ == "__main__": main()