"""Actual PIE wheel spin/steer regression. Inspects rotation, not suspension movement."""
import json,math,time,unreal
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main(capture=False):
 state={'start':time.monotonic(),'last':-1,'samples':[]}
 bones=('Wheel_FL','Wheel_FR','Wheel_BL','Wheel_BR')
 wheel_classes=[unreal.EditorAssetLibrary.load_blueprint_class('/Game/Vehicles/UtilityVan/Blueprints/BP_UtilityVanWheel'+side) for side in ('Front','Rear')]
 assert all(wheel_classes),'Missing van wheel classes'
 def quat(q):return [q.x,q.y,q.z,q.w]
 def angle(a,b):return math.degrees(2*math.acos(min(1,abs(sum(x*y for x,y in zip(a,b))))))
 def tick(dt):
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:return
  p=unreal.GameplayStatics.get_player_pawn(w,0)
  if not p:return
  m=p.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent);sk=p.get_component_by_class(unreal.SkeletalMeshComponent)
  if 't0' not in state:state['t0']=unreal.GameplayStatics.get_time_seconds(w)
  t=unreal.GameplayStatics.get_time_seconds(w)-state['t0']
  if capture:
   if not state.get('isolated'):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
     if actor!=p and (isinstance(actor,unreal.LandscapeProxy) or actor.get_components_by_class(unreal.StaticMeshComponent)):actor.set_actor_hidden_in_game(True)
    state['isolated']=True
   for boom in p.get_components_by_class(unreal.SpringArmComponent):
    if boom.get_name().startswith('Back'):
     boom.set_relative_rotation(unreal.Rotator(pitch=0,yaw=90,roll=0),False,False)
     boom.set_editor_property('target_arm_length',550.0)
     boom.set_editor_property('socket_offset',unreal.Vector(0,0,35))
   for index,when in enumerate((4.2,4.55)):
    key='photo'+str(index)
    if t>when and not state.get(key):
     state[key]=True
     path=(ROOT/'Saved/WorldGeneration'/('VanWheelSpin_'+str(index)+'.png')).as_posix()
     unreal.SystemLibrary.execute_console_command(w,'HighResShot 1400x900 filename="'+path+'"')
 
  if t<9:p.do_throttle(.5 if t>2 else 0)
  else:p.do_brake(1 if m.get_forward_speed()>50 else 0)
  m.set_handbrake_input(t>=9 and m.get_forward_speed()<=50)
  p.do_steering(.35 if 5<t<7 else 0)
  if len(state.setdefault('vertex_samples',[]))<3 and t>(1.0,4.0,5.0)[len(state['vertex_samples'])]:
   positions=unreal.QRVehicleEditorLibrary.get_skinned_vertex_positions(sk)
   assert positions,'Missing skinned render vertices'
   centers=[sk.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_COMPONENT).translation for b in bones]
   if 'vertex_indices' not in state:
    state['vertex_indices']=[min(range(len(positions)),key=lambda i:(positions[i].x-c.x-30)**2+(positions[i].y-c.y-(-13 if c.y<0 else 13))**2+(positions[i].z-c.z)**2) for c in centers]
   state['vertex_samples'].append([[positions[i].x-c.x,positions[i].z-c.z] for i,c in zip(state['vertex_indices'],centers)])
  if int(t*10)!=state['last']:
   state['last']=int(t*10)
   qs=[quat(sk.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_COMPONENT).rotation) for b in bones]
   physical=[{'spin':x.get_rotation_angle(),'steer':x.get_steer_angle()} for x in m.wheels]
   state['samples'].append({'t':t,'speed':m.get_forward_speed(),'bones':qs,'physics':physical,'anim':str(sk.get_anim_instance()),'rendered':sk.was_recently_rendered(1.0)})
  if t>=13:
   samples=state['samples'];moving=[s for s in samples if s['speed']>100]
   checks={}
   for i,b in enumerate(bones):
    checks[b+'_vertices_rotate']=len(state['vertex_samples'])==3 and max(math.dist(state['vertex_samples'][0][i],sample[i]) for sample in state['vertex_samples'][1:])>8
    checks[b+'_spins']=bool(moving) and max(angle(moving[0]['bones'][i],s['bones'][i]) for s in moving)>20
    expected=wheel_classes[0 if i<2 else 1]
    checks[b+'_van_wheel_class']=m.wheels[i].get_class()==expected
    checks[b+'_physical_spins']=bool(moving) and max(abs(s['physics'][i]['spin']-moving[0]['physics'][i]['spin']) for s in moving)>60
   checks['front_steers']=any(abs(s['physics'][0]['steer'])>3 for s in samples)
   checks['anim_instance']=sk.get_anim_instance() is not None
   checks['wheel_motion_blur_disabled']=not sk.get_editor_property('bPerBoneMotionBlur')
   checks['mesh_rendered']=any(s['rendered'] for s in moving)
   result={'passed':all(checks.values()),'checks':checks,'vertex_indices':state.get('vertex_indices',[]),'vertex_samples':state['vertex_samples'],'samples':samples,'anim_class':str(sk.get_editor_property('anim_class')),'animation_mode':str(sk.get_editor_property('animation_mode'))}
   (ROOT/'Saved/WorldGeneration/VanWheelAnimation.json').write_text(json.dumps(result,indent=2))
   unreal.unregister_slate_post_tick_callback(state['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
   unreal.log('[VAN-WHEELS] '+json.dumps(checks))
 state['handle']=unreal.register_slate_post_tick_callback(tick)
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()

if __name__=='__main__':main()
