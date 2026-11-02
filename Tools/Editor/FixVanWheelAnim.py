"""Restore and persist native Chaos wheel animation on the existing undamaged van."""
import unreal
OUT='/Game/Vehicles/UtilityVan'

def main():
 bp=unreal.load_asset(OUT+'/Blueprints/BP_UtilityVan_Pawn')
 anim=unreal.load_asset(OUT+'/Animation/ABP_UtilityVan')
 assert bp and anim,'Missing van blueprint/animation'
 bp.modify()
 default=unreal.get_default_object(bp.generated_class());default.modify()
 mesh=default.get_component_by_class(unreal.SkeletalMeshComponent);assert mesh
 mesh.modify()
 mesh.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_BLUEPRINT)
 mesh.set_editor_property('anim_class',anim.generated_class())
 mesh.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 saved=unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.SkeletalMeshComponent)
 assert saved.get_editor_property('anim_class')==anim.generated_class()
 assert saved.get_editor_property('animation_mode')==unreal.AnimationMode.ANIMATION_BLUEPRINT
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
 unreal.log('[WHEELFIX] Native Chaos animation binding persisted')

if __name__=='__main__':main()
