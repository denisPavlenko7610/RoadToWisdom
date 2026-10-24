"""Project-owned landscape shader: distance blending, rotated detail and macro tint."""
import unreal

PATH = "/Game/QuietRoadWorld/Materials/M_RegionGround"
E = unreal.MaterialEditingLibrary


def build(force=False):
    m = unreal.load_asset(PATH) if unreal.EditorAssetLibrary.does_asset_exist(PATH) else None
    if m and not force:
        if any(str(p) == "MacroTintStrength" for p in E.get_scalar_parameter_names(m)):
            return m
    if not m:
        m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_RegionGround",
                                                                    "/Game/QuietRoadWorld/Materials", unreal.Material,
                                                                    unreal.MaterialFactoryNew())
    E.delete_all_material_expressions(m)

    def node(cls, **props):
        n = E.create_material_expression(m, getattr(unreal, "MaterialExpression" + cls))
        if cls == "ComponentMask":
            props = dict(dict(r=False, g=False, b=False, a=False), **props)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def wire(a, b, pin, output=""):
        assert E.connect_material_expressions(a, output, b, pin), (b.get_class().get_name(), pin)

    def scalar(name, value):
        return node("ScalarParameter", parameter_name=name, default_value=value)

    def op(cls, a, b):
        n = node(cls);
        wire(a, n, "A");
        wire(b, n, "B");
        return n

    def lerp(a, b, alpha):
        n = node("LinearInterpolate");
        wire(a, n, "A");
        wire(b, n, "B");
        wire(alpha, n, "Alpha");
        return n

    def saturate(a):
        n = node("Clamp", min_default=0.0, max_default=1.0);
        wire(a, n, "");
        return n

    def sample(texture, uv, normal=False):
        n = node("TextureSample", texture=unreal.load_asset(
            "/Game/Nimikko_WesternTown/Materials/Surface/Textures/" + texture),
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        wire(uv, n, "UVs");
        return n

    pos = node("WorldPosition")
    xy = node("ComponentMask", r=True, g=True);
    wire(pos, xy, "")
    near_uv = op("Divide", xy, scalar("TileSize", 450.0))
    broad_uv = op("Divide", xy, scalar("BroadTileSize", 1900.0))
    # A 90-degree rotation plus offset avoids aligned repeating details.
    x = node("ComponentMask", r=True);
    wire(broad_uv, x, "")
    y = node("ComponentMask", g=True);
    wire(broad_uv, y, "")
    neg = op("Multiply", x, node("Constant", r=-1.0))
    rotate = node("AppendVector");
    wire(y, rotate, "A");
    wire(neg, rotate, "B")
    broad_uv = op("Add", rotate, node("Constant2Vector", r=.37, g=.71))
    fine = sample("T_Ex_Sand_Albedo", near_uv)
    broad = sample("T_Ex_Sand_02_Albedo", broad_uv)
    fine_rgb = node("ComponentMask", r=True, g=True, b=True);
    wire(fine, fine_rgb, "")
    broad_rgb = node("ComponentMask", r=True, g=True, b=True);
    wire(broad, broad_rgb, "")
    close = lerp(fine_rgb, broad_rgb, scalar("DetailMix", .32))
    depth = node("PixelDepth")
    fade = saturate(op("Divide", op("Subtract", depth, scalar("DistanceStart", 6000.0)),
                       scalar("DistanceBlendLength", 22000.0)))
    color = lerp(close, broad_rgb, fade)
    # A separate 137 m sample changes tint across hills without giant crisp detail.
    macro = sample("T_Ex_Sand_02_Albedo", op("Divide", xy, scalar("MacroTileSize", 13700.0)))
    desat = node("ComponentMask", r=True);
    wire(macro, desat, "")
    variation = op("Multiply", op("Subtract", desat, node("Constant", r=.5)),
                   scalar("MacroTintStrength", .7))
    macro_factor = op("Add", node("Constant", r=1.0), variation)
    color = op("Multiply", color, macro_factor)
    tint = node("VectorParameter", parameter_name="Tint", default_value=unreal.LinearColor(.74, .69, .57, 1))
    color = op("Multiply", color, tint)
    normal = node("VertexNormalWS")
    z = node("ComponentMask", b=True);
    wire(normal, z, "")
    slope = saturate(op("Multiply", op("Subtract", scalar("RockSlopeStart", .94), z), scalar("RockSlopeStrength", 4.0)))
    rock_tint = node("VectorParameter", parameter_name="RockTint", default_value=unreal.LinearColor(.52, .47, .39, 1))
    rock = op("Multiply", broad_rgb, rock_tint)
    color = lerp(color, rock, slope)
    assert E.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert E.connect_material_property(scalar("Roughness", .94), "", unreal.MaterialProperty.MP_ROUGHNESS)
    n = sample("T_Ex_Sand_Normal", near_uv, True)
    n_rgb = node("ComponentMask", r=True, g=True, b=True);
    wire(n, n_rgb, "")
    flat = node("Constant3Vector", constant=unreal.LinearColor(0, 0, 1, 1))
    # Fade the fine normals with distance to avoid sparkling hills.
    normal_strength = op("Multiply", scalar("NormalStrength", .38), op("Subtract", node("Constant", r=1), fade))
    assert E.connect_material_property(lerp(flat, n_rgb, normal_strength), "", unreal.MaterialProperty.MP_NORMAL)
    E.layout_material_expressions(m)
    E.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


if __name__ == "__main__":
    build(True)