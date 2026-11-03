"""Build only the isolated persistence fixture; never touches the main world.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
"""
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
fixture = '/Game/Tests/L_SaveSession'
if unreal.EditorAssetLibrary.does_asset_exist(fixture):
    assert les.load_level(fixture)
    owned = {'StaticMeshActor', 'BP_UtilityVan_Pawn_C', 'PlayerStart',
             'RoadToWisdomFuseBox', 'UTLever', 'RoadToWisdomPlacedPickup',
             'RoadToWisdomMissionManager', 'RoadToWisdomVanRadioPanel'}
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_name() in owned:
            assert eas.destroy_actor(actor)
else:
    assert les.new_level(fixture)


def spawn(path, xyz):
    cls = unreal.load_class(None, path)
    assert cls, path
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(*xyz))
    assert actor, path
    return actor


ground = spawn('/Script/Engine.StaticMeshActor', (0, 0, -50))
ground.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
ground.set_actor_scale3d(unreal.Vector(100, 100, 1))
van = spawn('/Game/Vehicles/UtilityVan/Blueprints/BP_UtilityVan_Pawn.BP_UtilityVan_Pawn_C', (0, 0, 100))
van.set_editor_property('tags', [unreal.Name('QuietRoad.Van')])
# Keep the panel outside the supplied van's wider body, accessible from the left.
radio = spawn('/Script/RoadToWisdom.RoadToWisdomVanRadioPanel', (160, -220, 210))
radio.get_editor_property('panel_mesh').set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
radio.get_editor_property('panel_mesh').set_world_scale3d(unreal.Vector(0.25, 0.55, 0.7))
spawn('/Script/Engine.PlayerStart', (300, 500, 100))
box = spawn('/Script/RoadToWisdom.RoadToWisdomFuseBox', (2400, 300, 100))
lever = spawn('/Script/UTWorldMechanicsRuntime.UTLever', (2400, -100, 90))
pickup = spawn('/Script/RoadToWisdom.RoadToWisdomPlacedPickup', (2800, 300, 100))
pickup.set_editor_property('save_stable_id', unreal.Name('QuietRoad.Relay.Fuse01'))
payload = unreal.UTItemPayload()
stack = unreal.UTItemStack()
stack.set_editor_property('item_id', unreal.Name('Item.Fuse'))
stack.set_editor_property('count', 1)
payload.set_editor_property('stack', stack)
pickup.set_editor_property('initial_item', payload)
mission = spawn('/Script/RoadToWisdom.RoadToWisdomMissionManager', (2400, 0, 200))
mission.set_editor_property('fuse_box', box)
mission.set_editor_property('lever', lever)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None,
                                                                                      '/Script/RoadToWisdom.RoadToWisdomQuietRoadGameMode'))
assert les.save_current_level()
unreal.log('[SAVE-FIXTURE] PASSED')
