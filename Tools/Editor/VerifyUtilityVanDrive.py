"""Short actual PIE Chaos drive; does not add a persistent actor."""
import json,time,unreal
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
state={'started':time.monotonic(),'last':-1,'samples':[]}
def tick(delta):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:return
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    if not pawn:return
    if 't0' not in state:
        state['t0']=unreal.GameplayStatics.get_time_seconds(world)
        state['origin']=pawn.get_actor_location()
    t=unreal.GameplayStatics.get_time_seconds(world)-state['t0']
    movement=pawn.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)
    sk=pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    if t<8: pawn.do_throttle(.4 if t>2 else 0)
    else: pawn.do_brake(1.0 if movement.get_forward_speed()>50 else 0.0)
    movement.set_handbrake_input(t>=8 and movement.get_forward_speed()<=50)
    pawn.do_steering(.12 if 5<t<6 else 0)
    if int(t)!=state['last']:
        state['last']=int(t)
        p=pawn.get_actor_location()
        sample={'time':t,'position':[p.x,p.y,p.z],'speed_cm_s':movement.get_forward_speed(),
          'distance_m':(p-state['origin']).length()/100,'rotation':str(pawn.get_actor_rotation()),
          'wheel_states':[str(movement.break_wheel_status(movement.get_wheel_state(i))) for i in range(4)],
          'wheel_transforms':[str(sk.get_socket_transform(n,unreal.RelativeTransformSpace.RTS_COMPONENT)) for n in ('Wheel_FL','Wheel_FR','Wheel_BL','Wheel_BR')]}
        state['samples'].append(sample)
    if t>=13 or time.monotonic()-state['started']>90:
        samples=state['samples']
        checks={'correct_pawn':'UtilityVan' in pawn.get_class().get_path_name(),
                'moved':samples[-1]['distance_m']>5,
                'accelerated':max(s['speed_cm_s'] for s in samples)>200,
                'stopped':abs(samples[-1]['speed_cm_s'])<50,
                'wheel_animation_changed':samples[0]['wheel_transforms']!=samples[6]['wheel_transforms'] if len(samples)>6 else False}
        result={'pawn':pawn.get_class().get_path_name(),'samples':samples,'elapsed_game_seconds':t,'checks':checks,'passed':all(checks.values())}
        (ROOT/'Saved/WorldGeneration/UtilityVanDrive.json').write_text(json.dumps(result,indent=2))
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        unreal.log('[VAN-DRIVE] '+json.dumps({'distance_m':state['samples'][-1]['distance_m'],'elapsed':t}))
state['handle']=unreal.register_slate_post_tick_callback(tick)
