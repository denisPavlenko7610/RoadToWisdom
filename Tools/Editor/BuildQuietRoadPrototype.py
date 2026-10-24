"""Editor-time builder for the Quiet Road vertical slice (GDD M0/M1).

Creates the Enhanced Input assets, the fuse item definition and the prototype map
L_QuietRoad_Prototype with the mission 1 staging (van, relay station, fuse box,
crate with the fuse world item, lever, mission manager, lights, player start).

Run from the command line (headless, no editor window needed):
  "C:\\Program Files\\Epic Games\\U_5.8\\UE_5.8\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
    "<project>.uproject" -run=pythonscript -script="Tools/Editor/BuildQuietRoadPrototype.py" ^
    -stdout -unattended -nopause -nosplash

The script is idempotent: existing QR_* actors in the map are removed before the
scene is rebuilt, and asset creation is skipped when the asset already exists.
"""

import unreal

INPUT_ACTIONS = "/Game/QuietRoad/Input/Actions"
IMC_PATH = "/Game/QuietRoad/Input"
DATA_PATH = "/Game/QuietRoad/Data"
MAP_PATH = "/Game/QuietRoad/Maps/L_QuietRoad_Prototype"


def log(msg):
    unreal.log_warning("[QuietRoad] " + str(msg))


# ---------------------------------------------------------------------------
# Asset helpers
# ---------------------------------------------------------------------------

def create_asset_if_missing(name, package_path, asset_class, factory):
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    existing = unreal.EditorAssetLibrary.does_asset_exist(f"{package_path}/{name}")
    if existing:
        log(f"asset exists, reusing: {package_path}/{name}")
        return unreal.EditorAssetLibrary.load_asset(f"{package_path}/{name}")
    asset = asset_tools.create_asset(name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"failed to create asset {package_path}/{name}")
    log(f"created asset: {package_path}/{name}")
    return asset


def make_input_actions():
    specs = [
        ("IA_QR_Move", unreal.InputActionValueType.AXIS2D),
        ("IA_QR_Look", unreal.InputActionValueType.AXIS2D),
        ("IA_QR_Interact", unreal.InputActionValueType.BOOLEAN),
        ("IA_QR_ToggleVehicle", unreal.InputActionValueType.BOOLEAN),
    ]
    actions = {}
    for name, value_type in specs:
        ia = create_asset_if_missing(name, INPUT_ACTIONS, unreal.InputAction, unreal.InputActionFactory())
        ia.set_editor_property("value_type", value_type)
        actions[name] = ia
    unreal.EditorAssetLibrary.save_directory(INPUT_ACTIONS, only_if_is_dirty=False)
    return actions


def key(name):
    return unreal.Key(unreal.Name(name))


def swizzle(order):
    mod = unreal.InputAxisSwizzleModifier()
    mod.set_editor_property("order", order)
    return mod


def negate(x=False, y=False, z=False):
    mod = unreal.NegateModifier()
    mod.set_editor_property("x", x)
    mod.set_editor_property("y", y)
    mod.set_editor_property("z", z)
    return mod


def make_mapping(action, key_name, modifiers=None):
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("action", action)
    m.set_editor_property("key", key(key_name))
    if modifiers:
        m.set_editor_property("modifiers", modifiers)
    return m


def make_mapping_contexts(actions):
    # On-foot: WASD move, mouse look, E interact, F toggle.
    try:
        swizzle_enum = unreal.InputAxisSwizzle
    except AttributeError:
        swizzle_enum = unreal.InputAxisSwizzleModifierAxisOrder

    on_foot = create_asset_if_missing("IMC_QR_OnFoot", IMC_PATH, unreal.InputMappingContext, unreal.InputMappingContextFactory())
    on_foot.set_editor_property("mappings", [
        make_mapping(actions["IA_QR_Move"], "W", [swizzle(swizzle_enum.YXZ)]),
        make_mapping(actions["IA_QR_Move"], "S", [swizzle(swizzle_enum.YXZ), negate(y=True)]),
        make_mapping(actions["IA_QR_Move"], "A", [swizzle(swizzle_enum.ZXY), negate(z=True)]),
        make_mapping(actions["IA_QR_Move"], "D", [swizzle(swizzle_enum.ZXY)]),
        make_mapping(actions["IA_QR_Look"], "Mouse2D"),
        make_mapping(actions["IA_QR_Interact"], "E"),
        make_mapping(actions["IA_QR_ToggleVehicle"], "F"),
    ])

    # Driving: template IMC_Default keeps W/S/A/D/Space; this context adds only F.
    vehicle = create_asset_if_missing("IMC_QR_VehicleActions", IMC_PATH, unreal.InputMappingContext, unreal.InputMappingContextFactory())
    vehicle.set_editor_property("mappings", [
        make_mapping(actions["IA_QR_ToggleVehicle"], "F"),
    ])

    unreal.EditorAssetLibrary.save_directory(IMC_PATH, only_if_is_dirty=False)
    log("input mapping contexts ready")


def make_item_definition():
    item_class = unreal.load_class(None, "/Script/UTItemsRuntime.UTItemDefinition")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_class", item_class)
    da = create_asset_if_missing("DA_QR_Fuse", DATA_PATH, item_class, factory)
    da.set_editor_property("item_id", unreal.Name("Item.Fuse"))
    da.set_editor_property("display_name", unreal.Text("Предохранитель"))
    da.set_editor_property("description", unreal.Text("Стеклянный предохранитель для старого щитка."))
    da.set_editor_property("max_stack_size", 2)
    unreal.EditorAssetLibrary.save_directory(DATA_PATH, only_if_is_dirty=False)
    log("item definition ready (Item.Fuse)")


# ---------------------------------------------------------------------------
# Map construction
# ---------------------------------------------------------------------------

def spawn_class(class_path, location, rotation=None, label=None):
    cls = unreal.load_class(None, class_path)
    if cls is None:
        raise RuntimeError(f"class not found: {class_path}")
    rot = rotation or unreal.Rotator()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, location, rot)
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_mesh(mesh_path, location, scale, rotation=None, label=None):
    mesh = unreal.load_object(None, mesh_path)
    if mesh is None:
        raise RuntimeError(f"mesh not found: {mesh_path}")
    rot = rotation or unreal.Rotator()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, location, rot)
    actor.set_actor_scale3d(scale)
    if label:
        actor.set_actor_label(label)
    return actor


def remove_previous_build():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            label = actor.get_actor_label()
        except Exception:
            continue
        if label.startswith("QR_") or actor.get_class().get_name() in {
            "PlayerStart", "DirectionalLight", "SkyLight", "ExponentialHeightFog",
            "RoadToWisdomMissionManager", "RoadToWisdomFuseBox", "UTLever", "UTWorldItem",
            "BP_SportsCar_Pawn_C",
        }:
            unreal.EditorLevelLibrary.destroy_actor(actor)
    log("previous build cleared")


def build_map():
    unreal.EditorLevelLibrary.new_level(MAP_PATH)
    remove_previous_build()

    # -- Terrain --------------------------------------------------------------
    # Ground: one 300m x 300m plane at Z=0.
    spawn_mesh("/Engine/BasicShapes/Plane.Plane", unreal.Vector(0, 0, 0), (300, 300, 1),
               label="QR_Ground")
    # Dirt road strip along X from the start pad to the relay station (~240m).
    spawn_mesh("/Engine/BasicShapes/Plane.Plane", unreal.Vector(12000, 0, 5), (240, 6, 1),
               label="QR_Road")
    # Start parking pad.
    spawn_mesh("/Engine/BasicShapes/Plane.Plane", unreal.Vector(0, 0, 6), (24, 24, 1),
               label="QR_StartPad")

    # -- The van (template sports car as the temporary camper stand-in) --------
    van_bp = unreal.load_object(None, "/Game/VehicleTemplate/Blueprints/SportsCar/BP_SportsCar_Pawn.BP_SportsCar_Pawn")
    if van_bp is None:
        raise RuntimeError("BP_SportsCar_Pawn not found")
    van = unreal.EditorLevelLibrary.spawn_actor_from_class(van_bp.generated_class(),
                                                           unreal.Vector(0, 0, 120), unreal.Rotator(0, 90, 0))
    van.set_actor_label("QR_Van")

    # -- Relay station at the end of the road ----------------------------------
    station = unreal.Vector(24000, 0, 0)
    spawn_mesh("/Engine/BasicShapes/Cylinder.Cylinder", station + unreal.Vector(0, 600, 100), (2, 2, 4),
               label="QR_RelayMast")
    fuse_box = spawn_class("/Script/RoadToWisdom.RoadToWisdomFuseBox",
                           station + unreal.Vector(0, 300, 100), unreal.Rotator(0, -90, 0),
                           label="QR_FuseBox")
    fuse_box.get_editor_property("BoxMesh").set_static_mesh(
        unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube"))
    fuse_box.get_editor_property("BoxMesh").set_world_scale3d((0.8, 0.5, 1.2))

    lever = spawn_class("/Script/UTWorldMechanicsRuntime.UTLever",
                        station + unreal.Vector(0, -100, 90), unreal.Rotator(0, -90, 0),
                        label="QR_Lever")

    # Crate with the fuse world item next to the mast.
    crate = spawn_mesh("/Engine/BasicShapes/Cube.Cube", station + unreal.Vector(400, 300, 50), (1, 1, 1),
                       label="QR_Crate")
    crate.get_static_mesh_component().set_static_mesh(unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube"))

    world_item = spawn_class("/Script/UTInventoryRuntime.UTWorldItem",
                             station + unreal.Vector(400, 300, 130), unreal.Rotator(),
                             label="QR_FuseWorldItem")
    payload = unreal.UTItemPayload()
    payload.set_editor_property("b_unique", False)
    stack = unreal.UTItemStack()
    stack.set_editor_property("item_id", unreal.Name("Item.Fuse"))
    stack.set_editor_property("count", 1)
    payload.set_editor_property("stack", stack)
    world_item.set_editor_property("initial_item", payload)

    # -- Mission wiring ----------------------------------------------------------
    mission = spawn_class("/Script/RoadToWisdom.RoadToWisdomMissionManager",
                          station + unreal.Vector(0, 0, 200), unreal.Rotator(),
                          label="QR_MissionManager")
    mission.set_editor_property("fuse_box", fuse_box)
    mission.set_editor_property("lever", lever)
    mission.set_editor_property("arrive_radius", 2000.0)

    # -- Spawn point and atmosphere ---------------------------------------------
    spawn_class("/Script/Engine.PlayerStart", unreal.Vector(300, 500, 100), unreal.Rotator(0, -90, 0),
                label="QR_PlayerStart")

    sun = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000))
    sun.set_actor_label("QR_Sun")
    sun.set_actor_rotation(unreal.Rotator(-22, -35, 0), False)
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3000)).set_actor_label("QR_SkyLight")
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)).set_actor_label("QR_Fog")

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log(f"map built and saved: {MAP_PATH}")


def main():
    actions = make_input_actions()
    make_mapping_contexts(actions)
    make_item_definition()
    build_map()
    log("DONE")


main()
