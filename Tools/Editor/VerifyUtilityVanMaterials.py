"""Check the van's persisted shader usage, texture references and actual pawn slots."""
import json
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[2]
OUT='/Game/Vehicles/UtilityVan/'

def main():
    lib=unreal.MaterialEditingLibrary
    material=unreal.load_asset(OUT+'Materials/M_UtilityVan')
    instance=unreal.load_asset(OUT+'Materials/MI_UtilityVan')
    mesh=unreal.load_asset(OUT+'Meshes/SK_UtilityVan')
    assert material and instance and mesh,'Missing van material/mesh assets'
    assert instance.get_editor_property('parent')==material,'Wrong material instance parent'
    for asset in (material,instance):
        assert lib.has_material_usage(asset,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH),asset.get_name()+' lacks skeletal usage'
    textures={}
    for name in ('BaseColor','Normal','Metallic','Roughness','AO'):
        texture=lib.get_material_instance_texture_parameter_value(instance,name)
        assert texture and texture.get_path_name()==OUT+'Textures/T_UtilityVan_'+name+'.T_UtilityVan_'+name,'Missing/wrong '+name+' texture'
        assert texture.get_editor_property('srgb')==(name=='BaseColor'),'Incorrect sRGB for '+name
        textures[name]=texture.get_path_name()
    slots=mesh.get_editor_property('materials')
    assert len(slots)==5,'Unexpected van slot count'
    assert all(s.get_editor_property('material_interface')==instance for s in slots),'Missing/wrong skeletal mesh material'
    bp=unreal.load_asset(OUT+'Blueprints/BP_UtilityVan_Pawn')
    default=unreal.get_default_object(bp.generated_class())
    components=[default.get_component_by_class(unreal.SkeletalMeshComponent)]
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        assert pawn and isinstance(pawn,unreal.RoadToWisdomUtilityVan),'Wrong PIE pawn'
        components.append(pawn.get_component_by_class(unreal.SkeletalMeshComponent))
    for component in components:
        assert component and component.get_skinned_asset()==mesh,'Wrong pawn mesh'
        assert component.get_num_materials()==5,'Wrong pawn material count'
        assert all(component.get_material(i)==instance for i in range(5)),'Pawn overrides van materials'
    result={'passed':True,'context':'PIE' if world else 'Editor','skeletal_usage':True,
            'material':instance.get_path_name(),'slots':[str(s.get_editor_property('material_slot_name')) for s in slots],
            'textures':textures,'checked_components':len(components)}
    dest=ROOT/'Saved/WorldGeneration'/('VanMaterials_'+result['context']+'.json')
    dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.log('[VAN-MATERIALS] '+json.dumps(result))
    return result

if __name__=='__main__':main()
