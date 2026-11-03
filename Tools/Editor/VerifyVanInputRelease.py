"""Inject actual Enhanced Input actions into the current map's pawn in PIE; no asset writes."""
import json, time, traceback
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]


def main():
    state = {'started': time.monotonic(), 'stage': 0, 'checks': {}, 'samples': []}
    stages = [
        ('idle', {}),
        ('throttle', {'throttle': .6}),
        ('both_pedals', {'throttle': .6, 'brake': .8}),
        ('brake_after_throttle_release', {'brake': .8}),
        ('pedals_released', {}),
        ('steering', {'steering': .5}),
        ('steering_released', {}),
        ('handbrake', {'handbrake': 1.0}),
        ('handbrake_released', {}),
    ]

    def finish(error=None):
        if error: state['error'] = error
        state['passed'] = not error and bool(state['checks']) and all(state['checks'].values())
        out = ROOT / 'Saved/WorldGeneration/VanInputRelease.json'
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(
            json.dumps({k: v for k, v in state.items() if k not in ('handle', 'subsystem', 'actions')}, indent=2))
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        unreal.log('[VAN-INPUT] ' + str(state.get('checks')) + ' error=' + str(error))

    def tick(dt):
        try:
            if time.monotonic() - state['started'] > 90:
                finish('PIE timeout');
                return
            w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not w: return
            p = unreal.GameplayStatics.get_player_pawn(w, 0)
            if not p: return
            m = p.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)
            if 'subsystem' not in state:
                subs = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if
                        isinstance(s.get_outer(), unreal.LocalPlayer)]
                assert len(subs) == 1, 'Expected one local player subsystem'
                state['subsystem'] = subs[0]
                state['actions'] = {key: p.get_editor_property(key + '_action') for key in
                                    ('throttle', 'brake', 'steering', 'handbrake')}
                state['since'] = time.monotonic()
                state['pawn_class'] = p.get_class().get_path_name()
            name, values = stages[state['stage']]
            for key, action in state['actions'].items():
                state['subsystem'].inject_input_vector_for_action(action, unreal.Vector(values.get(key, 0), 0, 0), [],
                                                                  [])
            if time.monotonic() - state['since'] < .8: return
            sample = {'stage': name, 'throttle': m.get_throttle_input(), 'brake': m.get_brake_input(),
                      'steering': m.get_steering_input(), 'handbrake': m.get_handbrake_input()}
            state['samples'].append(sample)
            expected = {key: values.get(key, 0) for key in state['actions']}
            if name == 'both_pedals': expected['throttle'] = 0
            state['checks'][name] = all(abs(float(sample[key]) - value) < .05 for key, value in expected.items())
            state['stage'] += 1
            state['since'] = time.monotonic()
            if state['stage'] == len(stages): finish()
        except Exception:
            finish(traceback.format_exc())

    state['handle'] = unreal.register_slate_post_tick_callback(tick)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()


if __name__ == '__main__': main()
