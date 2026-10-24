import unreal

out = []
errors = []

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.load_level("/Game/QuietRoad/Maps/L_QuietRoad_Prototype"):
    errors.append("MAP FAILED TO LOAD")
else:
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    found = {}
    for a in actors:
        cn = a.get_class().get_name()
        found[cn] = found.get(cn, 0) + 1
    out.append("actor classes: %s" % ", ".join(f"{k}={v}" for k, v in sorted(found.items())))

    for cls, need in [("BP_SportsCar_Pawn_C", 1), ("RoadToWisdomFuseBox", 1),
                      ("UTLever", 1), ("UTWorldItem", 1), ("RoadToWisdomMissionManager", 1),
                      ("PlayerStart", 1), ("DirectionalLight", 1), ("SkyLight", 1),
                      ("ExponentialHeightFog", 1)]:
        if found.get(cls, 0) < need:
            errors.append(f"missing {cls} (need {need}, have {found.get(cls, 0)})")

    # Van registration contract: exactly one tagged vehicle pawn.
    vans = [a for a in actors if any(str(t) == "QuietRoad.Van" for t in a.get_editor_property("tags"))]
    out.append("tagged vans: %d" % len(vans))
    if len(vans) != 1:
        errors.append(f"expected exactly 1 tagged van, found {len(vans)}")
    elif not vans[0].get_class().get_name().endswith("_C"):
        errors.append("tagged van is not a Blueprint vehicle")

    # Mission wiring
    for a in actors:
        if a.get_class().get_name() == "RoadToWisdomMissionManager":
            fb = a.get_editor_property("fuse_box")
            lv = a.get_editor_property("lever")
            out.append(f"mission refs: fusebox={'set' if fb else 'MISSING'}, lever={'set' if lv else 'MISSING'}")
            if not fb or not lv:
                errors.append("mission manager references not wired")
        if a.get_class().get_name() == "UTWorldItem":
            payload = a.get_editor_property("initial_item")
            stack = payload.get_editor_property("stack")
            out.append(f"world item: unique={payload.get_editor_property('unique')}, item_id={stack.get_editor_property('item_id')}, count={stack.get_editor_property('count')}")
            if str(stack.get_editor_property("item_id")) != "Item.Fuse":
                errors.append("world item does not carry Item.Fuse")

# Item definition sanity
da = unreal.EditorAssetLibrary.load_asset("/Game/QuietRoad/Data/DA_QR_Fuse")
if da is None:
    errors.append("DA_QR_Fuse missing")
else:
    out.append(f"item def: item_id={da.get_editor_property('item_id')}, stack={da.get_editor_property('max_stack_size')}")

# Input assets sanity
for path in ["/Game/QuietRoad/Input/IMC_QR_OnFoot", "/Game/QuietRoad/Input/IMC_QR_VehicleActions"]:
    imc = unreal.EditorAssetLibrary.load_asset(path)
    if imc is None:
        errors.append(f"{path} missing")
    else:
        out.append(f"{path.rsplit('/', 1)[-1]} mappings={len(imc.get_editor_property('mappings'))}")

unreal.log_warning("[VERIFY] ||| " + " ||| ".join(out))
if errors:
    unreal.log_warning("[VERIFY-FAIL] ||| " + " ||| ".join(errors))
    unreal.log_warning("[VERIFY-RESULT] FAILED")
else:
    unreal.log_warning("[VERIFY-RESULT] PASSED")

try:
    unreal.SystemLibrary.quit_editor()
except Exception:
    pass
