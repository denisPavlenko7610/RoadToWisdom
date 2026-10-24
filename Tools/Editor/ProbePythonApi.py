import unreal
out = []
k = unreal.Key()
out.append("Key props: %s" % [n for n in dir(k) if not n.startswith('_')])
out.append("has InputKeyNames: %s" % hasattr(unreal, "InputKeyNames"))
try:
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("key", "W")
    out.append("set key by str: OK")
except Exception as e:
    out.append("set key by str: %s" % e)
try:
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("key", unreal.Name("W"))
    out.append("set key by Name: OK")
except Exception as e:
    out.append("set key by Name: %s" % e)
try:
    imc_asset = unreal.EditorAssetLibrary.load_asset("/Game/QuietRoad/Input/IMC_QR_OnFoot")
    out.append("load imc: %s" % (imc_asset is not None))
    if imc_asset:
        mm = unreal.EnhancedActionKeyMapping()
        ia = unreal.EditorAssetLibrary.load_asset("/Game/QuietRoad/Input/Actions/IA_QR_Interact")
        mm.set_editor_property("action", ia)
        mm.set_editor_property("key", "E")
        imc_asset.set_editor_property("mappings", [mm])
        unreal.EditorAssetLibrary.save_loaded_asset(imc_asset)
        out.append("mapping with str key: OK")
except Exception as e:
    out.append("mapping with str key: %s" % e)
unreal.log_warning("[PROBE2] ||| " + " ||| ".join(out))
