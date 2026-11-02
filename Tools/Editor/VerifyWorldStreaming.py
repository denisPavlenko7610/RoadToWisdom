"""Accelerated streaming-source sweep in PIE; separate from physical driving acceptance."""
import json,time,unreal
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
state={'i':0,'next':time.monotonic()+4,'samples':[]}
points=[(-170000+340000*i/40,-140000+280000*i/40) for i in range(41)]
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def tick(dt):
 w=E.get_game_world()
 if not w or time.monotonic()<state['next']:return
 pawn=unreal.GameplayStatics.get_player_pawn(w,0)
 if not pawn:return
 if state['i']>=len(points):
  result={'passed':all(s['ground'] for s in state['samples']),'samples':state['samples'],'test':'teleport source ~110 m/s, not driving speed'}
  (ROOT/'Saved/WorldGeneration/M23FastStreaming.json').write_text(json.dumps(result,indent=2))
  unreal.unregister_slate_post_tick_callback(state['handle']);L.editor_request_end_play();return
 if 'placed' in state:
  x,y=state.pop('placed');hit=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(x,y,90000),unreal.Vector(x,y,-90000),unreal.TraceTypeQuery.ECC_VISIBILITY,False,[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if not isinstance(a,unreal.LandscapeProxy)],unreal.DrawDebugTrace.NONE)
  terrain=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.LandscapeProxy)
  state['samples'].append({'wall_time':time.monotonic(),'x':x,'y':y,'ground':bool(hit),'landscape_actors':len(terrain),'actor_names':[a.get_name() for a in terrain]})
  state['i']+=1;state['next']=time.monotonic()+.01;return
 x,y=points[state['i']];pawn.get_component_by_class(unreal.SkeletalMeshComponent).set_simulate_physics(False)
 pawn.set_actor_location(unreal.Vector(x,y,50000),False,True);state['placed']=(x,y);state['next']=time.monotonic()+1
state['handle']=unreal.register_slate_post_tick_callback(tick)
L.editor_request_begin_play()
