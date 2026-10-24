"""M2 visual reference generator for the Quiet Road slice (GDD 24).

Deterministic editor-time placement of mass decor onto a single
ARoadToWisdomDecorCluster (HISM components): pines (trunk + crown), bushes,
rocks, plus a small coherent material palette and a late-afternoon light preset.
Road, start pad, relay station and the van door zone stay fixed and excluded.

Run headlessly:
  UnrealEditor-Cmd.exe RoadToWisdom.uproject -ExecutePythonScript=Tools/Editor/GenerateM2Decor.py

Re-running replaces the previous decor (one cluster actor per run).
"""

import math
import random
import unreal

LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

MAP_PATH = '/Game/QuietRoad/Maps/L_QuietRoad_Prototype' 

# -- Generator inputs (edit these; the map itself stays fixed) -----------------
SEED = 20261024
AREA = unreal.Vector(24000, 25000, 0)   # X extent of the strip, Y full width
AREA_MIN_Y = -12500                     # Y range centered on the road
AREA_MAX_Y = 12500
TREE_COUNT = 110
BUSH_COUNT = 130
ROCK_COUNT = 45
MIN_TREE_DIST = 450.0                   # cm between tree centers
MIN_BUSH_DIST = 160.0

# Exclusion rects in cm [min_x, min_y, max_x, max_y]: road with vehicle clearance,
# the start parking pad (plus door zone), and the relay station footprint.
ROAD_RECT = [-600, -700, 25000, 700]
PAD_RECT = [-1600, -1600, 1600, 1600]
STATION_RECT = [22200, -1400, 26200, 1400]

PALETTE = {  # engine-material color parameters (approximate soft-stylized values)
    "Ground":  (0.32, 0.34, 0.18),
    "Road":    (0.36, 0.33, 0.28),
    "Pine":    (0.10, 0.24, 0.12),
    "Trunk":   (0.28, 0.19, 0.11),
    "Bush":    (0.16, 0.30, 0.14),
    "Rock":    (0.45, 0.44, 0.42),
}


def log(msg):
    unreal.log_warning("[M2Decor] " + str(msg))


def load_first(paths):
    for path in paths:
        obj = unreal.load_object(None, path)
        if obj:
            return obj
    return None


def transform_at(x, y, z, scale, yaw_deg=0.0, pitch_deg=0.0, roll_deg=0.0):
    t = unreal.Transform()
    t.translation = unreal.Vector(x, y, z)
    t.rotation = unreal.Rotator(pitch_deg, yaw_deg, roll_deg).quaternion()
    t.scale3d = unreal.Vector(scale, scale, scale)
    return t


def in_rect(x, y, rect):
    return rect[0] <= x <= rect[2] and rect[1] <= y <= rect[3]


def excluded(x, y):
    return in_rect(x, y, ROAD_RECT) or in_rect(x, y, PAD_RECT) or in_rect(x, y, STATION_RECT)


def make_parent_material(path="/Game/QuietRoad/Materials/M_M2_Tinted"):
    mat = unreal.EditorAssetLibrary.load_asset(path)
    if mat:
        return mat
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = asset_tools.create_asset("M_M2_Tinted", "/Game/QuietRoad/Materials",
                                   unreal.Material, unreal.MaterialFactoryNew())
    editing = unreal.MaterialEditingLibrary
    param = editing.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
    param.set_editor_property("parameter_name", "Tint")
    editing.connect_material_property(param, "", unreal.MaterialProperty.MP_BASE_COLOR)
    editing.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def make_material_instance(name, parent, color, package_path="/Game/QuietRoad/Materials"):
    package = f"{package_path}/{name}"
    existing = unreal.EditorAssetLibrary.load_asset(package)
    if existing is None:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        existing = asset_tools.create_asset(name, package_path,
                                            unreal.MaterialInstanceConstant, factory)
    existing.set_editor_property("parent", parent)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(existing, "Tint", unreal.LinearColor(*color, 1.0))
    unreal.EditorAssetLibrary.save_loaded_asset(existing)
    return existing


def old_cluster():
    for actor in EAS.get_all_level_actors():
        if actor.get_actor_label().startswith("QR_Ground_M2"):
            EAS.destroy_actor(actor)


def find_cluster():
    for actor in EAS.get_all_level_actors():
        if actor.get_actor_label() == "QR_DecorCluster":
            return actor
    return None


def place_points(rng, count, min_dist, placed):
    points = []
    attempts = count * 30
    while len(points) < count and attempts > 0:
        attempts -= 1
        x = rng.uniform(-800.0, AREA.x + 800.0)
        y = rng.uniform(AREA_MIN_Y, AREA_MAX_Y)
        if excluded(x, y):
            continue
        if any((x - px) * (x - px) + (y - py) * (y - py) < min_dist * min_dist for px, py in placed):
            continue
        points.append((x, y))
        placed.append((x, y))
    return points


def main():
    rng = random.Random(SEED)

    # Edit and save the exact target map, not whatever world the editor opened with.
    if not LES.load_level(MAP_PATH):
        raise RuntimeError(f'cannot load {MAP_PATH}')
    old_cluster()

    basic = "/Engine/BasicShapes/"
    trunk_mesh = load_first([basic + "Cylinder.Cylinder", basic + "Cube.Cube"])
    crown_mesh = load_first([basic + "Cone.Cone", basic + "Pyramid.Pyramid", basic + "Cube.Cube"])
    bush_mesh = load_first([basic + "Sphere.Sphere"])
    rock_mesh = load_first([basic + "Sphere.Sphere", basic + "Cube.Cube"])
    ground_mesh = load_first([basic + "Plane.Plane"])
    basic_mat = load_first(["/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"])
    if not all([trunk_mesh, crown_mesh, bush_mesh, rock_mesh, ground_mesh, basic_mat]):
        raise RuntimeError("engine basic shapes or material not found")

    parent = make_parent_material()
    palette = {}
    for name, color in PALETTE.items():
        palette[name] = make_material_instance("MI_M2_" + name, parent, color)

    # Tint the existing ground / road / pad (fixed level geometry, not decor).
    for actor in EAS.get_all_level_actors():
        label = actor.get_actor_label()
        mesh_component = actor.get_editor_property("static_mesh_component")             if actor.get_class().get_name() == "StaticMeshActor" else None
        if not mesh_component:
            continue
        if label == "QR_Ground":
            mesh_component.set_material(0, palette["Ground"])
            unreal.log_warning(f"[M2Decor] ground material now: {mesh_component.get_material(0).get_name()}")
        elif label == "QR_Road":
            mesh_component.set_material(0, palette["Road"])
            unreal.log_warning(f"[M2Decor] road material now: {mesh_component.get_material(0).get_name()}")
        elif label == "QR_StartPad":
            mesh_component.set_material(0, palette["Road"])

    cluster = find_cluster()
    if cluster is None:
        raise RuntimeError("QR_DecorCluster not found - run BuildQuietRoadPrototype.py first")
    components = {
        0: cluster.get_editor_property("TreeTrunks"),
        1: cluster.get_editor_property("TreeCrowns"),
        2: cluster.get_editor_property("Bushes"),
        3: cluster.get_editor_property("Rocks"),
    }
    components[0].set_static_mesh(trunk_mesh)
    components[1].set_static_mesh(crown_mesh)
    components[2].set_static_mesh(bush_mesh)
    components[3].set_static_mesh(rock_mesh)
    for slot, mat in ((0, "Trunk"), (1, "Pine"), (2, "Bush"), (3, "Rock")):
        components[slot].set_material(0, palette[mat])

    placed = []
    trees = place_points(rng, TREE_COUNT, MIN_TREE_DIST, placed)
    trunks, crowns = [], []
    for x, y in trees:
        s = rng.uniform(0.8, 1.4)
        # Cylinder (h=200cm) scaled to 3z -> 600cm*? trunk half-height 300*s? keep:
        # trunk scale z 3s makes it clearly taller than wide; crown cone (h=100cm)
        # scaled 2.5s starts exactly at the trunk top.
        trunks.append(transform_at(x, y, 150.0 * s, s, roll_deg=0.0))
        trunks[-1].scale3d = unreal.Vector(0.55 * s, 0.55 * s, 1.5 * s)
        crowns.append(transform_at(x, y, 300.0 * s + 125.0 * s, 2.5 * s,
                                   yaw_deg=rng.uniform(0, 360)))
    for slot, instances in ((0, trunks), (1, crowns)):
        cluster.set_instances(slot, instances) if hasattr(cluster, "set_instances") else \
            cluster.call_method("SetInstances", args=(slot, instances))

    bushes = place_points(rng, BUSH_COUNT, MIN_BUSH_DIST, placed)
    bush_tfs = [transform_at(x, y, 25.0, rng.uniform(0.5, 1.0)) for x, y in bushes]
    cluster.set_instances(2, bush_tfs) if hasattr(cluster, "set_instances") else \
        cluster.call_method("SetInstances", args=(2, bush_tfs))

    rocks = place_points(rng, ROCK_COUNT, MIN_BUSH_DIST, placed)
    rock_tfs = [transform_at(x, y, 20.0, rng.uniform(0.5, 1.4),
                             yaw_deg=rng.uniform(0, 360),
                             roll_deg=rng.uniform(-12, 12)) for x, y in rocks]
    cluster.set_instances(3, rock_tfs) if hasattr(cluster, "set_instances") else \
        cluster.call_method("SetInstances", args=(3, rock_tfs))

    counts = {p: len(components[p].get_editor_property('per_instance_sm_data')) for p in components}
    log(f"placed: trees={len(trees)} bushes={len(bushes)} rocks={len(rocks)} (seed {SEED}); serialized counts={counts}")

    # The earlier light-preset experiment left a dense fog in the saved map;
    # bring it back to a light mist (GDD 24: light fog).
    for actor in EAS.get_all_level_actors():
        if actor.get_actor_label() == "QR_Fog":
            fog = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            if fog:
                fog.set_editor_property("fog_density", 0.02)

    if not LES.save_current_level():
        raise RuntimeError("save_current_level failed")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("M2 decor saved")
    # Reload from disk and confirm the instances actually persisted.
    LES.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")
    reloaded = find_cluster()
    if reloaded is None:
        log("RELOAD CHECK: cluster MISSING after reload")
    else:
        rcounts = {p: reloaded.get_editor_property(p).get_instance_count()
                   for p in ("TreeTrunks", "TreeCrowns", "Bushes", "Rocks")}
        log(f"RELOAD CHECK counts={rcounts}")


main()
