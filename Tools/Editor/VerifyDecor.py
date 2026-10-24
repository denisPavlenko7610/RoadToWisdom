"""Run twice and reopen from disk, asserting deterministic world-space HISM data."""
import hashlib
import json
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import GenerateM2Decor as decor


def signature():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    clusters = [a for a in actors if a.get_class().get_name() == "RoadToWisdomDecorCluster"]
    assert len(clusters) == 1
    cluster = clusters[0]
    data = {}
    for prop in ("TreeTrunks", "TreeCrowns", "Bushes", "Rocks"):
        comp = cluster.get_editor_property(prop)
        data[prop] = []
        for i in range(comp.get_instance_count()):
            t = comp.get_instance_transform(i, world_space=True)
            data[prop].append([round(v, 3) for v in (
                t.translation.x, t.translation.y, t.translation.z,
                t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w,
                t.scale3d.x, t.scale3d.y, t.scale3d.z)])
            if prop != "TreeCrowns":
                assert not decor.excluded(t.translation.x, t.translation.y)
        if prop in ("TreeCrowns", "Bushes"):
            assert comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    return data


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    decor.main()
    first = signature()
    # Nonzero carrier origin verifies AddInstance(..., true) uses world coordinates.
    cluster = decor.find_cluster()
    cluster.set_actor_location(unreal.Vector(1700, -500, 0), False, False)
    assert les.save_current_level()
    decor.main()
    second = signature()
    assert first == second, "same seed changed world transforms / generator duplicated instances"
    assert les.load_level("/Engine/Maps/Entry")
    assert les.load_level(decor.MAP_PATH)
    assert second == signature(), "map reload lost/changed instances"
    digest = hashlib.sha256(json.dumps(second, sort_keys=True).encode()).hexdigest()
    result = dict(seed=decor.SEED, counts={p: len(v) for p, v in second.items()}, sha256=digest,
                  repeated_generation=True, reopened=True, nonzero_cluster_origin=True)
    path = Path(unreal.Paths.project_saved_dir()) / "M21DecorVerification.json"
    path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("[DECOR-RESULT] PASSED " + json.dumps(result))


if __name__ == "__main__":
    main()