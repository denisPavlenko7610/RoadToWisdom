"""Capture eight real editor views without creating persistent camera actors."""
import json
from pathlib import Path
import time
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
world=json.loads((ROOT/'Saved/WorldGeneration/World.json').read_text())
pois={p['name']:p['center'] for p in world['pois']}
main=world['roads'][0]['points']
def road_view(index):
 p=main[index];q=main[min(index+8,len(main)-1)]
 return (p[0],p[1],p[2]+2.8),(q[0],q[1],q[2]+1.5)
views=[
 ('01_Landscape',(0,-3100,2400),(0,0,100)),
 ('02_WindingRoad',*road_view(140)),
 ('03_HillRoad',*road_view(350)),
 ('04_Settlement',(pois['DustCrossing'][0]-100,pois['DustCrossing'][1]-140,pois['DustCrossing'][2]+60),pois['DustCrossing']),
 ('05_LonelyHouse',(pois['RidgeCabin'][0]-60,pois['RidgeCabin'][1]-80,pois['RidgeCabin'][2]+28),pois['RidgeCabin']),
 ('06_RoadNetwork',(0,0,3400),(0,0,0)),
 ('07_LargeSettlement',(pois['IronHollow'][0]-140,pois['IronHollow'][1]-180,pois['IronHollow'][2]+85),pois['IronHollow']),
 ('08_Overlook',(pois['NorthOverlook'][0],pois['NorthOverlook'][1]-10,pois['NorthOverlook'][2]+12),(0,0,50))]
system=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state={'step':0,'next':time.monotonic()+1,'files':[], 'pending':None}

def tick(dt):
 level.editor_invalidate_viewports()
 if state['pending']:
  path,stamp,deadline=state['pending']
  if path.exists() and path.stat().st_mtime>stamp:
   state['files'].append(str(path)); state['pending']=None
  elif time.monotonic()>deadline:
   unreal.unregister_slate_post_tick_callback(state['handle'])
   unreal.log_error('[WORLD-CAPTURE] Screenshot was not rendered: '+str(path));return
  else:return
 if time.monotonic()<state['next']:return
 step=state['step']
 if step>=len(views)*2:
  unreal.unregister_slate_post_tick_callback(state['handle'])
  (ROOT/'Saved/WorldGeneration/Screenshots.json').write_text(json.dumps(state['files'],indent=2))
  unreal.log('[WORLD-CAPTURE] Completed eight editor screenshots')
  return
 name,pos,target=views[step//2]
 if step%2==0:
  p=unreal.Vector(*[v*100 for v in pos]); t=unreal.Vector(*[v*100 for v in target])
  system.set_level_viewport_camera_info(p,unreal.MathLibrary.find_look_at_rotation(p,t))
 else:
  path=ROOT/'Saved/WorldGeneration'/str(name+'.png')
  state['pending']=(path,path.stat().st_mtime if path.exists() else 0,time.monotonic()+45)
  unreal.SystemLibrary.execute_console_command(None,'HighResShot 1600x900 filename="'+path.as_posix()+'"')
 state['step']+=1
 state['next']=time.monotonic()+3
state['handle']=unreal.register_slate_post_tick_callback(tick)
