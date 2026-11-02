"""Repeat full generation only in an isolated clean-copy map, preserving a manual sentinel."""
from pathlib import Path
import json,runpy,time,unreal
root=Path(unreal.Paths.project_dir()).resolve()
assert 'M23Clean' in str(root),'Never regenerate the production world for this test'
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert les.load_level('/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld')
verify=runpy.run_path(str(root/'Tools/World/VerifyWorld.py'))
before=verify['main']()
# Remove only this test's own fixtures from an interrupted prior run.
for owned in eas.get_all_level_actors():
 if 'M23.ManualSentinel' in [str(t) for t in owned.tags]:eas.destroy_actor(owned)
sentinel=eas.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(100,100,50000));sentinel.set_actor_label('M23_ManualSentinel');sentinel.set_folder_path('Manual/Acceptance')
sentinel.tags=['M23.ManualSentinel'];sentinel.modify()
assert les.save_current_level()
generate=runpy.run_path(str(root/'Tools/World/GenerateWorld.py'))['main']
results=[]
for i in range(2):
 start=time.monotonic();generate();after=verify['main']()
 kept=[a for a in eas.get_all_level_actors() if 'M23.ManualSentinel' in [str(t) for t in a.tags]]
 assert len(kept)==1 and kept[0].get_actor_location().z==50000,'Manual content changed'
 assert after['hism_sha256']==before['hism_sha256'],'Instance transforms changed'
 results.append({'run':i+1,'seconds':time.monotonic()-start,'signature':after['hism_sha256'],'manual_preserved':True,'errors':after['errors']})
(root/'Saved/WorldGeneration/M23Regeneration.json').write_text(json.dumps({'passed':True,'runs':results},indent=2))
unreal.log('[M23-REGEN] '+str(results))
