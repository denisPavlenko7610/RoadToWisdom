"""Targeted M2.1 scene corrections. Keeps all gameplay actors and references."""
import math
import sys
from pathlib import Path
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from QuietRoadLayout import road_y, ROAD_HALF_WIDTH

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert les.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
for label, actor in actors.items():
    if label == "QR_Road" or label.startswith(("QR_Road_", "QR_M2Detail_")):
        eas.destroy_actor(actor)


def material(name):
    return unreal.load_asset("/Game/QuietRoad/Materials/MI_M2_" + name)


def shape(label, kind, xyz, scale, mat, yaw=0):
    actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz), unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    actor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+kind))
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.static_mesh_component.set_material(0, material(mat))
    return actor


ground = actors["QR_Ground"]
ground.set_actor_location(unreal.Vector(12000, 0, 0), False, False)
ground.set_actor_scale3d(unreal.Vector(290, 280, 1))
for i in range(80):
    x0, x1 = i * 300, (i + 1) * 300
    y0, y1 = road_y(x0), road_y(x1)
    length = math.hypot(x1 - x0, y1 - y0)
    # Adjacent planes overlap slightly. Ground carries the physical road, avoiding
    # seams/edges affecting Chaos tires; visual ribbon is nonblocking.
    road = shape(f"QR_Road_{i:03}", "Plane", ((x0 + x1) / 2, (y0 + y1) / 2, 5 + i * 0.0001),
                 ((length + 15) / 100, ROAD_HALF_WIDTH * 2 / 100, 1), "Road",
                 math.degrees(math.atan2(y1 - y0, x1 - x0)))
    road.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

shape("QR_M2Detail_RelayPad", "Plane", (24000, 200, 6), (24, 22, 1), "Road")
# Mast and dishes make the existing station readable without moving its gameplay.
mast = actors["QR_RelayMast"]
mast.set_actor_location(unreal.Vector(24000, 650, 550), False, False)
mast.set_actor_scale3d(unreal.Vector(0.75, 0.75, 11))
for z in (550, 850):
    dish = shape(f"QR_M2Detail_Dish{z}", "Sphere", (24000, 610, z), (1.7, 0.3, 1.7), "Rock")
    dish.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
shape("QR_M2Detail_StationRoof", "Cube", (24000, 330, 260), (3.6, 2.6, 0.15), "Trunk")
for x in (23840, 24160):
    shape(f"QR_M2Detail_Post{x}", "Cylinder", (x, 390, 125), (0.15, 0.15, 2.5), "Trunk")
# A rest bench beside the starting parking space.
shape("QR_M2Detail_Bench", "Cube", (-400, -600, 65), (2.6, 0.65, 0.18), "Trunk")
for x in (-500, -300):
    shape(f"QR_M2Detail_BenchLeg{x}", "Cube", (x, -600, 30), (0.15, 0.55, 0.6), "Rock")
shape("QR_M2Detail_SignPost", "Cylinder", (1300, -750, 120), (0.18, 0.18, 2.4), "Trunk")
shape("QR_M2Detail_Sign", "Cube", (1300, -750, 210), (1.8, 0.14, 0.55), "Rock")

sun = actors["QR_Sun"].get_component_by_class(unreal.DirectionalLightComponent)
sun.set_editor_property("intensity", 18000.0)
sun.set_editor_property("atmosphere_sun_light", True)
sun.set_editor_property("light_source_angle", 2.0)
actors["QR_Sun"].set_actor_rotation(unreal.Rotator(pitch=-25,yaw=-45,roll=0), False)
sky = actors["QR_SkyLight"].get_component_by_class(unreal.SkyLightComponent)
sky.set_editor_property("intensity", 0.8)
sky.set_editor_property("real_time_capture", True)
fog = actors["QR_Fog"].get_component_by_class(unreal.ExponentialHeightFogComponent)
fog.set_editor_property("fog_density", 0.008)
fog.set_editor_property("fog_height_falloff", 0.3)

light = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(24000, 250, 220))
light.set_actor_label("QR_M2Detail_RelayLight")
lc = light.get_component_by_class(unreal.PointLightComponent)
lc.set_editor_property("intensity", 1800.0)
lc.set_editor_property("attenuation_radius", 650.0)
lc.set_light_color(unreal.LinearColor(1, 0.65, 0.3))
assert les.save_current_level()
unreal.log("[M21] Fixed route, ground coverage, station silhouette and lighting saved")