import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
methods = [m for m in dir(les) if "play" in m.lower() or "pie" in m.lower() or "session" in m.lower()]
us = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
extra = [m for m in dir(us) if "game" in m.lower() or "world" in m.lower()]
gs = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log_warning("[LES] ||| play=%s ||| world=%s" % (methods, extra))
