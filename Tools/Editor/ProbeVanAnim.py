import unreal
out = []
lib = unreal.BlueprintEditorLibrary
for path in ("/Game/Vehicles/UtilityVan/Animation/ABP_UtilityVan", "/Game/Vehicles/OffroadCar/Offroad_AnimBP"):
    bp = unreal.EditorAssetLibrary.load_asset(path)
    parent = lib.get_blueprint_parent_class(bp) if bp else None
    out.append(f"{path}: parent={parent}")
out.append(f"ChaosVehicleAnimationInstance exists: {hasattr(unreal, 'ChaosVehicleAnimationInstance')}")
out.append(f"AnimInstance exists: {hasattr(unreal, 'AnimInstance')}")
unreal.log_warning("[VANANIM] ||| " + " ||| ".join(out))
