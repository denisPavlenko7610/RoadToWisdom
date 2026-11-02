"""Editor-only route acceptance on the existing possessed Chaos pawn. No map writes."""
import json,math,time
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[2]
manifest=json.loads((ROOT/'Saved/WorldGeneration/World.json').read_text())
# Main has an existing complete acceptance; recheck its start, drive every remaining route in full.
roads=[r for r in manifest['roads'] if r['name']!='Main']
state={'index':0,'phase':'wait','results':[],'next':0,'started':time.monotonic()}
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def write():
 (ROOT/'Saved/WorldGeneration/M23Branches.json').write_text(json.dumps({'results':state['results'],'phase':state['phase'],'road_index':state['index']},indent=2))
def finish(reason):
 r=state['road'];p=state['pawn'];m=state['movement']
 result={'road':r['name'],'reason':reason,'length_m':state['length'],'progress':state['arc']/state['length'],
 'max_lateral_m':state['max_offset'],'offroad_samples':state['offroad'],'samples':state['samples'],
 'elapsed_s':time.monotonic()-state['drive_start'],'passed':reason=='completed' and state['offroad']==0}
 state['results'].append(result);unreal.log('[M23-ROUTE] '+json.dumps(result))
 m.set_throttle_input(0);m.set_brake_input(1);state['index']+=1;state['phase']='place';write()
def tick(dt):
 world=E.get_game_world()
 if not world:return
 if state['index']>=len(roads):
  state['phase']='complete';write();unreal.unregister_slate_post_tick_callback(state['handle']);L.editor_request_end_play();return
 pawn=unreal.GameplayStatics.get_player_pawn(world,0)
 if not pawn:return
 m=pawn.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent);mesh=pawn.get_component_by_class(unreal.SkeletalMeshComponent)
 now=time.monotonic()
 if state['phase'] in ('wait','place'):
  r=roads[state['index']];pts=r['points'];a,b=pts[:2]
  state.update(road=r,pawn=pawn,movement=m,pts=pts,segment=0,arc=0,max_offset=0,offroad=0,samples=0,last_move=now,last_pos=None)
  state['length']=sum(math.dist(a[:2],b[:2]) for a,b in zip(pts,pts[1:]))
  m.reset_vehicle();m.set_throttle_input(0);m.set_brake_input(1);m.set_handbrake_input(True)
  pawn.set_actor_location_and_rotation(unreal.Vector(a[0]*100,a[1]*100,a[2]*100+80),unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(b[1]-a[1],b[0]-a[0])),roll=0),False,True)
  mesh.set_physics_linear_velocity(unreal.Vector());mesh.set_physics_angular_velocity_in_degrees(unreal.Vector())
  state['phase']='settle';state['next']=now+5;write();return
 if state['phase']=='settle':
  if now<state['next']:return
  state['phase']='drive';state['drive_start']=now
 p=pawn.get_actor_location();x,y=p.x/100,p.y/100;pts=state['pts'];best=(float('inf'),0,0)
 for i in range(max(0,state['segment']-2),min(len(pts)-1,state['segment']+12)):
  a,b=pts[i:i+2];dx,dy=b[0]-a[0],b[1]-a[1];dd=dx*dx+dy*dy;t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/max(dd,1e-9)))
  d=math.hypot(x-a[0]-t*dx,y-a[1]-t*dy)
  if d<best[0]:best=(d,i,t)
 offset,i,t=best;state['segment']=i
 state['arc']=sum(math.dist(a[:2],b[:2]) for a,b in zip(pts[:i],pts[1:i+1]))+math.dist(pts[i][:2],pts[i+1][:2])*t
 state['max_offset']=max(offset,state['max_offset']);state['samples']+=1
 if state['samples']%120==0:write()
 if offset>state['road']['half_width']-1.1:state['offroad']+=1
 speed=m.get_forward_speed()/100;look=max(6,abs(speed)*.65);target=pts[i+1];remaining=look
 for j in range(i+1,len(pts)-1):
  seg=math.dist(pts[j][:2],pts[j+1][:2])
  if seg>=remaining:
   f=remaining/max(seg,1e-9);target=[pts[j][k]+f*(pts[j+1][k]-pts[j][k]) for k in range(3)];break
  remaining-=seg;target=pts[j+1]
 yaw=math.radians(pawn.get_actor_rotation().yaw);heading=math.atan2(target[1]-y,target[0]-x);alpha=math.atan2(math.sin(heading-yaw),math.cos(heading-yaw))
 dist=max(1,math.hypot(target[0]-x,target[1]-y));steer=1.4*math.atan2(2*3.18*math.sin(alpha),dist)/math.radians(32)
 m.set_steering_input(max(-1,min(1,steer)));m.set_handbrake_input(False)
 want=9 if abs(alpha)<.25 else 6
 m.set_throttle_input(max(.15,min(1,(want-speed)*.4)) if speed<want else 0);m.set_brake_input(.25 if speed>want+1 else 0)
 if abs(speed)>.5:state['last_move']=now
 if state['length']-state['arc']<5:finish('completed')
 elif now-state['last_move']>15:finish('stalled')
 elif offset>15:finish('left_road')
 elif now-state['drive_start']>900:finish('timeout')
state['handle']=unreal.register_slate_post_tick_callback(tick)
L.editor_request_begin_play()
