"""Materialize prepared data with UE Landscape, World Partition, spline meshes and HISM.
Run in the Editor after PrepareWorld.py. Existing maps remove ONLY QRWorld.Generated
actors in the selected category; hand-authored actors are never selected for deletion.
"""
from collections import Counter
import json
import math
from pathlib import Path
import random
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = "/Game/QuietRoadWorld"
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LIB = unreal.QRWorldEditorLibrary
EDIT = unreal.MaterialEditingLibrary


def vector(values): return unreal.Vector(*values)


def mat_expression(material, cls, **props):
    expression = EDIT.create_material_expression(material, cls)
    for k, v in props.items(): expression.set_editor_property(k, v)
    return expression


def connect(source, output, target, pin):
    assert EDIT.connect_material_expressions(source, output, target, pin), (target.get_class().get_name(), pin)


def material(name, texture=None, tint=(1, 1, 1), normal=False, rebuild=False):
    path = OUT + "/Materials/" + name
    existing = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if existing and not rebuild: return existing
    mat = existing or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, OUT + "/Materials", unreal.Material,
                                                                  unreal.MaterialFactoryNew())
    if existing: EDIT.delete_all_material_expressions(mat)
    pos = mat_expression(mat, unreal.MaterialExpressionWorldPosition)
    xy = mat_expression(mat, unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
    connect(pos, "", xy, "")
    scale = mat_expression(mat, unreal.MaterialExpressionScalarParameter, parameter_name="TileSize",
                           default_value=500.0)
    uv = mat_expression(mat, unreal.MaterialExpressionDivide)
    connect(xy, "", uv, "A");
    connect(scale, "", uv, "B")
    color = mat_expression(mat, unreal.MaterialExpressionVectorParameter, parameter_name="Tint",
                           default_value=unreal.LinearColor(*tint, 1))
    if texture:
        tex = mat_expression(mat, unreal.MaterialExpressionTextureSample, texture=unreal.load_asset(texture))
        connect(uv, "", tex, "UVs")
        multiply = mat_expression(mat, unreal.MaterialExpressionMultiply)
        connect(tex, "RGB", multiply, "A");
        connect(color, "", multiply, "B")
        base = multiply
    else:
        base = color
    EDIT.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mat_expression(mat, unreal.MaterialExpressionConstant, r=.92)
    EDIT.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if normal:
        sample = mat_expression(mat, unreal.MaterialExpressionTextureSample,
                                texture=unreal.load_asset(
                                    "/Game/Nimikko_WesternTown/Materials/Surface/Textures/T_Ex_Sand_Normal"),
                                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        connect(uv, "", sample, "UVs")
        EDIT.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mat.set_editor_property("used_with_spline_meshes", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    if name == "M_DryGrass": mat.set_editor_property("two_sided", True)
    EDIT.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def landscape_material():
    """Editable project instance; original Fab materials remain untouched."""
    import runpy
    parent = runpy.run_path(str(ROOT / "Tools/World/LandscapeMaterial.py"))["build"]()
    path = OUT + "/Materials/MI_RegionLandscape"
    instance = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not instance:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_RegionLandscape", OUT + "/Materials", unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        EDIT.set_material_instance_parent(instance, parent)
        EDIT.set_material_instance_scalar_parameter_value(instance, "TileSize", 500.0)
        EDIT.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(.74, .69, .57, 1))
    unreal.EditorAssetLibrary.save_loaded_asset(instance)
    return instance


def primitive_mesh(name, kind):
    path = OUT + "/Meshes/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    vertices = [];
    triangles = []
    if kind == "rock":
        rng = random.Random(1423)
        for z, r in ((-20, .6), (15, 1), (95, .83), (155, .4)):
            for i in range(8):
                angle = i * math.tau / 8
                vertices.append((math.cos(angle) * r * 110 * rng.uniform(.85, 1.15),
                                 math.sin(angle) * r * 90 * rng.uniform(.85, 1.15), z + rng.uniform(-8, 8)))
        for ring in range(3):
            for i in range(8):
                a = ring * 8 + i;
                b = ring * 8 + (i + 1) % 8;
                c = b + 8;
                d = a + 8
                triangles.extend(((a, b, c), (a, c, d)))
        for i in range(1, 7): triangles.extend(((0, i + 1, i), (24, 24 + i, 25 + i)))
    else:
        for i in range(6):
            angle = i * math.tau / 6
            dx, dy = math.cos(angle) * 12, math.sin(angle) * 12
            start = len(vertices)
            vertices.extend(((-dx, -dy, 0), (dx, dy, 0), (math.cos(angle) * 18, math.sin(angle) * 18, 45 + i % 3 * 8)))
            triangles.append((start, start + 1, start + 2))
    source = ROOT / "Saved/WorldGeneration" / (name + ".obj")
    lines = ["o " + name, "s 1"] + [f"v {x} {y} {z}" for x, y, z in vertices] + ["vt 0 0", "vt 1 0", "vt 0 1"]
    lines += ["f " + " ".join(f"{v + 1}/{j + 1}" for j, v in enumerate(face)) for face in triangles]
    source.write_text("\n".join(lines), encoding="ascii")
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 0")
    task = unreal.AssetImportTask()
    task.filename = str(source);
    task.destination_path = OUT + "/Meshes";
    task.destination_name = name
    task.automated = True;
    task.save = True
    options = unreal.FbxImportUI();
    options.import_materials = False;
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property("convert_scene", False)
    options.static_mesh_import_data.set_editor_property("convert_scene_unit", False)
    options.static_mesh_import_data.set_editor_property("normal_import_method",
                                                        unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    options.static_mesh_import_data.set_editor_property("normal_generation_method",
                                                        unreal.FBXNormalGenerationMethod.BUILT_IN)
    task.options = options;
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    result = unreal.load_asset(path)
    assert result, path
    return result


def light_scene():
    sun = EAS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 50000))
    sun.set_actor_label("QRW_Sun");
    sun.set_actor_rotation(unreal.Rotator(pitch=-23, yaw=-42, roll=0), False)
    comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    comp.set_editor_property("intensity", 22000.0);
    comp.set_editor_property("atmosphere_sun_light", True)
    comp.set_editor_property("light_source_angle", 1.7)
    sky = EAS.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0));
    sky.set_actor_label("QRW_Atmosphere")
    ambient = EAS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0));
    ambient.set_actor_label("QRW_Skylight")
    ambient.get_component_by_class(unreal.SkyLightComponent).set_editor_property("real_time_capture", True)
    fog = EAS.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0));
    fog.set_actor_label("QRW_ValleyHaze")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", .0035);
    fc.set_editor_property("fog_height_falloff", .12)
    fc.set_editor_property("start_distance", 25000.0)
    for actor in (sun, sky, ambient, fog):
        actor.tags = ["QRWorld.Generated", "QRWorld.Lighting"]
        actor.set_folder_path("Generated/Atmosphere")
        LIB.set_spatial_loading(actor, False)


def place_buildings(manifest):
    options = ["Building_01_Abandoned", "Building_02_Abandoned", "Building_03_Abandoned",
               "Building_01", "Building_02", "Building_03"]
    buildings = [];
    rng = random.Random(manifest["settings"]["Seed"] + 91)
    for poi in manifest["pois"]:
        cx, cy, z = poi["center"]
        street, nearest_index = min(((r, i) for r in manifest['roads'] for i in range(len(r['points']))),
                                    key=lambda ri: math.hypot(ri[0]['points'][ri[1]][0] - cx,
                                                              ri[0]['points'][ri[1]][1] - cy))
        pa = street['points'][max(0, nearest_index - 1)]
        pb = street['points'][min(len(street['points']) - 1, nearest_index + 1)]
        angle = math.atan2(pb[1] - pa[1], pb[0] - pa[0])
        # Buildings face the central through road; bounded layouts keep a clear street.
        if poi["buildings"] >= 6:
            layout = [(t, side * 32) for side in (-1, 1) for t in (-70, -23, 23, 70)]
        elif poi["buildings"] == 3:
            layout = [(-28, -28), (26, -28), (0, 29)]
        elif poi["buildings"] == 2:
            layout = [(-22, -26), (22, 28)]
        else:
            layout = [(0, 28)] if poi["buildings"] else []
        for index, (dx, dy) in enumerate(layout):
            name = options[(index + len(buildings)) % len(options)]
            cls = unreal.load_class(None, "/Game/Nimikko_WesternTown/Assets/Buildings/" + name + "." + name + "_C")
            assert cls, name
            yaw = math.degrees(angle) + (0 if dy > 0 else 180)
            actor = EAS.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
            actor.set_actor_label("QRW_POI_" + poi["name"] + "_" + str(index))
            origin, extent = actor.get_actor_bounds(False)
            # Align the actual bounds bottom, independent of the Blueprint pivot.
            px = (cx + dx * math.cos(angle) - dy * math.sin(angle)) * 100
            py = (cy + dx * math.sin(angle) + dy * math.cos(angle)) * 100
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            ignore = [a for a in EAS.get_all_level_actors() if not isinstance(a, unreal.LandscapeProxy)]
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(px, py, z * 100 + 10000),
                                                         unreal.Vector(px, py, z * 100 - 10000),
                                                         unreal.TraceTypeQuery.ECC_VISIBILITY,
                                                         False, ignore, unreal.DrawDebugTrace.NONE)
            assert hit, "No terrain under " + actor.get_actor_label()
            foundation_z = hit.to_dict()["location"].z
            actor.set_actor_location(
                unreal.Vector(px - origin.x, py - origin.y, foundation_z - origin.z + extent.z - 2),
                False, False)
            actor.tags = ["QRWorld.Generated", "QRWorld.POI"]
            actor.set_folder_path("Generated/POI/" + poi["name"])
            actor.set_actor_tick_enabled(False)
            # Original geometry remains untouched. Collision derivatives are made only
            # for structural meshes lacking simple collision (see ensure_collision).
            for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
                mesh = comp.static_mesh
                if mesh and "/Structure/" in mesh.get_path_name():
                    comp.set_static_mesh(ensure_collision(mesh))
                comp.set_collision_profile_name("BlockAll")
            for comp in actor.get_components_by_class(unreal.LightComponent):
                comp.set_visibility(False)
            buildings.append(dict(label=actor.get_actor_label(), blueprint=name,
                                  bounds_center=[px, py, z * 100 + extent.z], extent=[extent.x, extent.y, extent.z],
                                  poi=poi["name"]))
    return buildings


def ensure_collision(mesh):
    name = mesh.get_name()
    path = OUT + "/Collision/" + name + "_World"
    result = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if result: return result
    result = unreal.EditorAssetLibrary.duplicate_asset(mesh.get_path_name().split(".")[0], path)
    assert result, path
    result.get_editor_property("body_setup").set_editor_property("collision_trace_flag",
                                                                 unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_loaded_asset(result)
    return result


def configure_streaming():
    settings = LIB.get_partition_settings()
    layers = 0
    for obj in unreal.ObjectIterator():
        if obj.get_outer() == settings:
            kind = obj.get_class().get_name()
            if kind == "RuntimePartitionPersistent":
                obj.set_editor_property("LoadingRange", 0)
            elif kind == "RuntimePartitionLHGrid":
                obj.set_editor_property("LoadingRange", max(200000, obj.get_editor_property("LoadingRange")))
                layers += 1
    assert layers, "No spatial World Partition grids configured"


def organize_scene():
    for actor in EAS.get_all_level_actors():
        tags = {str(t) for t in actor.tags}
        label = actor.get_actor_label()
        if "QRWorld.Generated" in tags:
            if "QRWorld.POI" in tags:
                continue  # Keep the named settlement subfolder.
            if "QRWorld.Terrain" in tags:
                folder = "Generated/Terrain"
            elif "QRWorld.Roads" in tags:
                folder = "Generated/Roads/" + label.removeprefix("QRW_Road_").rsplit("_", 1)[0]
            elif "QRWorld.Rocks" in tags:
                folder = "Generated/Nature/Rocks/" + label.split("_")[1]
            elif "QRWorld.Vegetation" in tags:
                folder = "Generated/Nature/Vegetation/" + label.split("_")[1]
            else:
                folder = "Generated/Atmosphere"
            actor.set_folder_path(folder)
        elif label in ("VehicleStart", "ExplorerStart"):
            actor.set_folder_path("Manual/Starts")
        elif isinstance(actor, (unreal.WorldDataLayers, unreal.WorldPartitionMiniMap)):
            actor.set_folder_path("World/System")


def main(category="All"):
    assert not unreal.SystemLibrary.parse_param(unreal.SystemLibrary.get_command_line(), "NullRHI"),         "Landscape proxy heightmap copying requires RHI: run generation in the regular GPU editor, never -NullRHI"
    manifest = json.loads((ROOT / "Saved/WorldGeneration/World.json").read_text(encoding="utf-8"))
    path = manifest["settings"]["Map"]
    exists = unreal.EditorAssetLibrary.does_asset_exist(path)
    if exists:
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if current.get_path_name().split(".")[0] != path:
            assert LES.load_level(path)
        # Load generated cells before regenerating; editor streaming helper is used
        # when available. Never delete by label/prefix or touch untagged actors.
        if hasattr(LIB, "load_editor_region"):
            LIB.load_editor_region(unreal.Vector(0, 0, 0), unreal.Vector(500000, 500000, 200000), True)
        for actor in EAS.get_all_level_actors():
            tags = [str(t) for t in actor.tags]
            if "QRWorld.Generated" in tags and (category == "All" or "QRWorld." + category in tags):
                EAS.destroy_actor(actor)
    else:
        assert category == "All", "Generate the full world first"
        assert LIB.new_partitioned_world()
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, path)
    ground = landscape_material()
    roadmat = material("M_RegionRoad", "/Game/Nimikko_WesternTown/Materials/Surface/Textures/T_Ex_Sand_02_Albedo",
                       (.85, .77, .62), True)
    rockmat = material("M_RegionRock", "/Game/Nimikko_WesternTown/Materials/Surface/Textures/T_Ex_Sand_02_Albedo",
                       (.4, .36, .3))
    grassmat = material("M_DryGrass", None, (.25, .21, .11))
    if category in ("All", "Terrain"):
        landscape = LIB.import_landscape(str(ROOT / "Saved/WorldGeneration/Terrain.r16"), manifest["resolution"], 200.0,
                                         ground)
        assert landscape, "native Landscape import failed"
        for actor in EAS.get_all_level_actors():
            if isinstance(actor, unreal.LandscapeProxy):
                actor.tags = ["QRWorld.Generated", "QRWorld.Terrain"]
                actor.set_folder_path("Generated/Terrain")
    if category in ("All", "Roads"):
        mesh = unreal.load_asset("/Engine/BasicShapes/Plane")
        for road in manifest["roads"]:
            points = road["points"]
            for part, start in enumerate(range(0, len(points) - 1, 35)):
                section = points[start:min(len(points), start + 36)]
                actor = LIB.create_road(f"QRW_Road_{road['name']}_{part}",
                                        [vector([v * 100 for v in p]) for p in section],
                                        mesh, roadmat, road["half_width"] * 100)
                assert actor, "road spline failed"
    buildings = []
    if category in ("All", "POI"): buildings = place_buildings(manifest)
    if category in ("All", "Vegetation", "Rocks"):
        shrub = unreal.load_asset("/Game/Nimikko_WesternTown/Assets/Foliage/SM_Shrub_01")
        rock = primitive_mesh("SM_RegionRock", "rock")
        grass = primitive_mesh("SM_DryGrass", "grass")
        half = manifest["settings"]["WorldSize"] / 2
        for cell in manifest["cells"]:
            origin = unreal.Vector((cell["cell"][0] * 252 - half) * 100, (cell["cell"][1] * 252 - half) * 100, 0)
            for kind, transforms in cell["categories"].items():
                cat = "Rocks" if kind.startswith("Rock") else "Vegetation"
                if category not in ("All", cat): continue
                packed = []
                for px, py, pz, yaw, sx, sy, sz in transforms:
                    t = unreal.Transform()
                    t.translation = unreal.Vector(px, py, pz)
                    t.rotation = unreal.Rotator(pitch=0, yaw=yaw, roll=0).quaternion()
                    t.scale3d = unreal.Vector(sx, sy, sz)
                    packed.append(t)
                mesh = shrub if kind == "Shrubs" else grass if kind == "Grass" else rock
                mat = None if kind == "Shrubs" else grassmat if kind == "Grass" else rockmat
                assert LIB.create_instances(f"QRW_{kind}_{cell['cell'][0]}_{cell['cell'][1]}", origin,
                                            mesh, mat, packed, kind == "RockLarge",
                                            180000 if kind == "RockLarge" else 45000)
    if category in ("All", "Lighting"): light_scene()
    if not any(a.get_actor_label() == "VehicleStart" for a in EAS.get_all_level_actors()):
        start = manifest["roads"][0]["points"][0]
        player = EAS.spawn_actor_from_class(unreal.PlayerStart,
                                            vector([start[0] * 100, start[1] * 100, start[2] * 100 + 150]))
        player.set_actor_label("ExplorerStart");
        LIB.set_spatial_loading(player, False)
        marker = EAS.spawn_actor_from_class(unreal.TargetPoint,
                                            vector([start[0] * 100, start[1] * 100, start[2] * 100 + 50]))
        marker.set_actor_label("VehicleStart");
        marker.set_folder_path("Manual");
        LIB.set_spatial_loading(marker, False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings_actor = unreal.GameplayStatics.get_actor_of_class(world, unreal.WorldSettings)
    settings_actor.set_editor_property("default_game_mode", unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/VehicleTemplate/Blueprints/BP_VehicleAdvGameMode"))
    configure_streaming()
    organize_scene()
    assert LES.save_current_level()
    unreal.EditorAssetLibrary.save_directory(OUT, only_if_is_dirty=True, recursive=True)
    report = dict(map=path, category=category, size=manifest["settings"]["WorldSize"],
                  metrics=manifest["metrics"], height_sha256=manifest["height_sha256"], buildings=buildings,
                  classes=dict(Counter(a.get_class().get_name() for a in EAS.get_all_level_actors())))
    (ROOT / "Saved/WorldGeneration" / ("Generated_" + str(manifest["settings"]["WorldSize"]) + ".json")).write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("[WORLD-GENERATED] " + json.dumps(
        dict(map=path, category=category, size=report["size"], classes=report["classes"], metrics=report["metrics"])))


if __name__ == "__main__": main()
