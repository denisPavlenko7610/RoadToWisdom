"""Small original camper mesh, shared flat-color MIs, existing Chaos chassis.

No external packs/textures. OBJ source is generated under Saved, not tracked.
Re-running replaces only the project's camper presentation and its generated mesh.
"""
import math
from pathlib import Path
import unreal

ROOT = "/Game/QuietRoad"
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

COLORS = {
    "Cream": (0.79, 0.72, 0.53), "Sage": (0.17, 0.36, 0.31),
    "Glass": (0.025, 0.085, 0.12), "Trim": (0.045, 0.055, 0.052),
    "Metal": (0.43, 0.48, 0.44), "Lamp": (1.0, 0.78, 0.35),
    "Tail": (0.6, 0.045, 0.025), "Copper": (0.62, 0.3, 0.13),
}


class Mesh:
    def __init__(self):
        self.vertices, self.faces = [], []

    def face(self, points, mat):
        start = len(self.vertices) + 1
        self.vertices.extend(points)
        for i in range(1, len(points) - 1):
            self.faces.append((mat, (start, start + i, start + i + 1)))

    def box(self, center, size, mat):
        x, y, z = center
        a, b, c = [v / 2 for v in size]
        p = [(x - a, y - b, z - c), (x + a, y - b, z - c), (x + a, y + b, z - c), (x - a, y + b, z - c),
             (x - a, y - b, z + c), (x + a, y - b, z + c), (x + a, y + b, z + c), (x - a, y + b, z + c)]
        for indices in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.face([p[i] for i in indices], mat)

    def prism_x(self, x0, x1, yz, mat):
        self.face([(x0, y, z) for y, z in reversed(yz)], mat)
        self.face([(x1, y, z) for y, z in yz], mat)
        for i in range(len(yz)):
            y0, z0 = yz[i]
            y1, z1 = yz[(i + 1) % len(yz)]
            self.face([(x0, y0, z0), (x0, y1, z1), (x1, y1, z1), (x1, y0, z0)], mat)

    def write(self, path):
        mtl = path.with_suffix(".mtl")
        mtl.write_text("\n".join(f"newmtl {m}\nKd {r} {g} {b}\n" for m, (r, g, b) in COLORS.items()))
        lines = [f"mtllib {mtl.name}", "o QuietRoadCamper", "s off"]
        lines += [f"v {x} {y} {z}" for x, y, z in self.vertices]
        lines += ["vt 0 0", "vt 1 0", "vt 0 1"]
        current = None
        for mat, indices in self.faces:
            if mat != current:
                lines.append("usemtl " + mat)
                current = mat
            lines.append("f " + " ".join(f"{v}/{uv+1}" for uv,v in enumerate(indices)))
        path.write_text("\n".join(lines))


def create_mesh():
    m = Mesh()
    m.prism_x(-220, 225, [(-96, 58), (96, 58), (104, 66), (104, 125), (96, 133), (-96, 133), (-104, 125), (-104, 66)],
              "Sage")
    m.prism_x(-220, 78, [(-104, 126), (104, 126), (104, 232), (92, 245), (-92, 245), (-104, 232)], "Cream")
    # Cabin roof slopes down toward the short nose.
    profile = [(65, 127), (220, 127), (220, 152), (130, 237), (65, 237)]
    m.face([(x, -104, z) for x, z in profile], "Cream")
    m.face([(x, 104, z) for x, z in reversed(profile)], "Cream")
    for i in range(len(profile)):
        x0, z0 = profile[i];
        x1, z1 = profile[(i + 1) % len(profile)]
        m.face([(x0, -104, z0), (x0, 104, z0), (x1, 104, z1), (x1, -104, z1)], "Cream")
    # Opaque stylized glazing avoids needing an interior.
    m.face([(140, -88, 229), (140, 88, 229), (211, 88, 162), (211, -88, 162)], "Glass")
    for side in (-1, 1):
        y = side * 105
        m.face([(80, y, 153), (188, y, 153), (125, y, 222), (80, y, 222)], "Glass")
        m.box((-105, y, 184), (112, 3, 58), "Trim")
        m.box((-105, side * 107, 184), (99, 3, 45), "Glass")
        m.box((-147, side * 109, 184), (8, 2, 44), "Cream")
        m.box((-62, side * 109, 184), (8, 2, 44), "Cream")
        # Belt line, door seams, mirrors and step.
        m.box((0, side * 105, 133), (433, 3, 6), "Copper")
        for x in (69, 192):
            m.box((x, side * 106, 111), (2, 2, 76), "Trim")
        m.box((130, side * 106, 73), (123, 2, 2), "Trim")
        m.box((84, side * 109, 143), (19, 5, 5), "Trim")
        m.box((178, side * 118, 181), (8, 29, 5), "Trim")
        m.box((178, side * 134, 182), (22, 10, 29), "Trim")
        m.box((130, side * 116, 55), (105, 28, 9), "Metal")
    m.box((227, 0, 99), (7, 116, 27), "Trim")
    for y in (-78, 78):
        m.box((230, y, 123), (8, 32, 23), "Lamp")
        m.box((-224, y, 105), (7, 16, 36), "Tail")
    for x in (-225, 228):
        m.box((x, 0, 63), (12, 214, 14), "Trim")
    m.box((-224, 0, 103), (6, 130, 57), "Trim")
    m.box((-228, 0, 103), (4, 122, 49), "Sage")
    m.box((-231, 0, 119), (3, 24, 4), "Metal")
    m.box((-222, 0, 184), (5, 142, 58), "Trim")
    m.box((-226, 0, 184), (4, 130, 46), "Glass")
    for y in (-87, 87):
        m.box((-72, y, 254), (254, 7, 7), "Metal")
    for x in (-180, 25):
        m.box((x, 0, 254), (7, 182, 7), "Metal")
    m.box((-90, 0, 269), (155, 118, 25), "Sage")
    m.box((-90, 0, 284), (146, 110, 7), "Trim")
    for x in (-145, -110, -75, -40):
        m.box((x, 0, 289), (2, 102, 1), "Metal")
    m.box((30, 0, 250), (45, 46, 9), "Cream")
    source = Path(unreal.Paths.project_saved_dir()) / "CamperSource"
    source.mkdir(parents=True, exist_ok=True)
    path = source / "SM_QR_Camper.obj"
    m.write(path)
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 0")
    task = unreal.AssetImportTask()
    task.filename = str(path.resolve())
    task.destination_path = ROOT + "/Meshes"
    task.destination_name = "SM_QR_Camper"
    task.automated = True;
    task.replace_existing = True;
    task.save = True
    opts = unreal.FbxImportUI()
    opts.import_mesh = True;
    opts.import_materials = False;
    opts.import_textures = False
    opts.import_as_skeletal = False
    opts.static_mesh_import_data.set_editor_property("combine_meshes", True)
    opts.static_mesh_import_data.set_editor_property("convert_scene", False)
    opts.static_mesh_import_data.set_editor_property("convert_scene_unit", False)
    opts.static_mesh_import_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    opts.static_mesh_import_data.set_editor_property("normal_generation_method", unreal.FBXNormalGenerationMethod.BUILT_IN)
    task.options = opts
    task.factory = unreal.FbxFactory()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(ROOT + "/Meshes/SM_QR_Camper")
    assert mesh, task.imported_object_paths
    return mesh


def main():
    assert LES.load_level(ROOT + "/Maps/L_QuietRoad_Prototype")
    # Reuse the scenery's single tint material.
    parent = unreal.load_asset(ROOT + "/Materials/M_M2_Tinted")
    assert parent
    mats = {}
    for name, color in COLORS.items():
        path = ROOT + "/Materials/MI_Camper_" + name
        mat = unreal.load_asset(path)
        if not mat:
            mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "MI_Camper_" + name, ROOT + "/Materials", unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mat, parent)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat, "Tint",
                                                                                   unreal.LinearColor(*color, 1))
        unreal.EditorAssetLibrary.save_loaded_asset(mat)
        mats[name] = mat
    mesh = create_mesh()
    materials = mesh.get_editor_property("static_materials")
    for index,entry in enumerate(materials):
        name = str(entry.material_slot_name)
        assert name in mats, name
        mesh.set_material(index,mats[name])
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    actors = EAS.get_all_level_actors()
    vans = [a for a in actors if "QuietRoad.Van" in [str(t) for t in a.tags]]
    assert len(vans) == 1
    van = vans[0]
    van.set_actor_location(unreal.Vector(0,0,12),False,False)
    # Match initial route tangent. No change to wheel setups, torque or suspension.
    van.set_actor_rotation(unreal.Rotator(pitch=0,yaw=26.45,roll=0), False)
    for actor in actors:
        if actor.get_class().get_name() == "RoadToWisdomCamperBody":
            EAS.destroy_actor(actor)
    body = EAS.spawn_actor_from_class(unreal.load_class(None, "/Script/RoadToWisdom.RoadToWisdomCamperBody"),
                                      van.get_actor_location())
    body.set_actor_label("QR_CamperBody")
    body.set_editor_property("van", van)
    body.get_editor_property("Body").set_static_mesh(mesh)
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder")
    for prop, mat in (("Wheels", mats["Trim"]), ("Hubs", mats["Cream"])):
        for comp in body.get_editor_property(prop):
            comp.set_static_mesh(cylinder)
            comp.set_material(0, mat)
    body.attach_presentation()
    # Re-run construction after all visual properties are configured.
    body.set_actor_transform(body.get_actor_transform(), False, False)
    body.attach_to_component(van.get_component_by_class(unreal.SkeletalMeshComponent),
                             unreal.Name(""), unreal.AttachmentRule.SNAP_TO_TARGET,
                             unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.KEEP_WORLD, False)
    for arm in van.get_components_by_class(unreal.SpringArmComponent):
        if "Back" in arm.get_name():
            arm.set_editor_property("do_collision_test", True)
            arm.set_editor_property("target_arm_length", 900)
            arm.set_editor_property("socket_offset", unreal.Vector(0, 0, 280))
        else:
            arm.set_relative_location(unreal.Vector(226, -45, 190), False, False)
    radio = next(a for a in actors if a.get_class().get_name() == "RoadToWisdomVanRadioPanel")
    radio.set_actor_scale3d(unreal.Vector(1, 1, 1))
    radio.get_editor_property("PanelMesh").set_relative_scale3d(unreal.Vector(.22, .45, .32))
    radio.set_actor_location(van.get_actor_transform().transform_location(unreal.Vector(30, -120, 140)), False, False)
    radio.get_editor_property("PanelMesh").set_material(0, mats["Sage"])
    # Preserve an authored local transform during loading, independent of actor BeginPlay order.
    radio.attach_to_actor(van, unreal.Name(""), unreal.AttachmentRule.KEEP_WORLD,
                          unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    # Mission geometry was previously unassigned on the placed pickup and lever.
    for actor in actors:
        cls=actor.get_class().get_name()
        if cls=='RoadToWisdomPlacedPickup':
            component=actor.get_editor_property('Mesh')
            component.set_static_mesh(cylinder)
            component.set_material(0,mats['Lamp'])
            actor.set_actor_scale3d(unreal.Vector(.16,.16,.35))
        elif cls=='RoadToWisdomFuseBox':
            component=actor.get_editor_property('FuseMesh')
            component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'))
            component.set_relative_location(unreal.Vector(58,0,0),False,False)
            component.set_relative_scale3d(unreal.Vector(.15,.15,.15))
            component.set_material(0,mats['Lamp'])
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            actor.get_editor_property('BoxMesh').set_material(0,mats['Sage'])
        elif cls=='UTLever':
            base=actor.get_editor_property('BaseMesh')
            base.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
            base.set_material(0,mats['Sage'])
            actor.set_actor_scale3d(unreal.Vector(.5,.4,.7))
            handle=actor.get_editor_property('HandleMesh')
            handle.set_static_mesh(cylinder)
            handle.set_relative_scale3d(unreal.Vector(.15,.15,.9))
            handle.set_relative_location(unreal.Vector(0,0,65),False,False)
            handle.set_material(0,mats['Copper'])
    # Input repair: A/D are X of Axis2D; the former Z-swizzle discarded sideways input.
    imc = unreal.load_asset(ROOT + "/Input/IMC_QR_OnFoot")
    mappings = imc.get_editor_property("mappings")
    for map_index,mapping in enumerate(mappings):
        key = mapping.key.export_text()
        if key in ("A", "D"):
            mods = []
            if key == "A":
                negate = unreal.InputModifierNegate()
                negate.set_editor_property("x", True)
                negate.set_editor_property("y", False)
                negate.set_editor_property("z", False)
                mods = [negate]
            mapping.set_editor_property("modifiers", mods)
            mappings[map_index]=mapping
    imc.set_editor_property("mappings", mappings)
    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    assert LES.save_current_level()
    unreal.log("[CAMPER] body mesh and presentation saved; chassis retained")


if __name__ == "__main__":
    main()