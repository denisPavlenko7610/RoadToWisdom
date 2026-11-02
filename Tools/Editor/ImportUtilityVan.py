"""Import only the undamaged van, build a native five-bone rig, configure Chaos."""
import json
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[2]
OUT='/Game/Vehicles/UtilityVan'
EAL=unreal.EditorAssetLibrary

def blueprint(name,parent):
    path=OUT+'/Blueprints/'+name
    bp=unreal.load_asset(path)
    if not bp:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',parent)
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,OUT+'/Blueprints',None,factory)
    return bp

def wheel_material(material):
    """Readable steel wheel faces; UV detail rotates with the skinned wheel, never Time."""
    edit=unreal.MaterialEditingLibrary
    color=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)
    if not isinstance(color,unreal.MaterialExpressionCustom):
        material.modify()
        base=next(o for o in unreal.ObjectIterator() if o.get_outer()==material
                  and isinstance(o,unreal.MaterialExpressionTextureSampleParameter2D)
                  and str(o.get_editor_property('parameter_name'))=='BaseColor')
        uv=edit.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate)
        strength=edit.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
        strength.set_editor_property('parameter_name','WheelDetailStrength')
        strength.set_editor_property('default_value',0.0)
        face=edit.create_material_expression(material,unreal.MaterialExpressionCustom)
        face.set_editor_property('description','Steel wheel vents and valve; follows original wheel UVs')
        face.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs=[]
        for name in ('UV','Base','Strength'):
            entry=unreal.CustomInput()
            entry.set_editor_property('InputName',name)
            inputs.append(entry)
        face.set_editor_property('inputs',inputs)
        face.set_editor_property('code',"""
float2 p=(UV-float2(0.06601,0.10696))/0.053;
float r=length(p);
float rim=smoothstep(0.24,0.29,r)*(1-smoothstep(0.51,0.56,r));
float vents=smoothstep(0.30,0.58,cos(atan2(p.y,p.x)*5.0));
vents*=smoothstep(0.31,0.34,r)*(1-smoothstep(0.45,0.48,r));
float3 steel=lerp(float3(0.40,0.37,0.32),float3(0.012,0.011,0.010),vents);
float3 color=lerp(Base,steel,rim*saturate(Strength));
float valve=1-smoothstep(0.065,0.085,length(p-float2(0.64,0.19)));
return lerp(color,float3(0.62,0.58,0.49),valve*saturate(Strength));
""")
        assert edit.connect_material_expressions(uv,'',face,'UV')
        assert edit.connect_material_expressions(base,'RGB',face,'Base')
        assert edit.connect_material_expressions(strength,'',face,'Strength')
        assert edit.connect_material_property(face,'',unreal.MaterialProperty.MP_BASE_COLOR)
        edit.recompile_material(material)
    path=OUT+'/Materials/MI_UtilityVanWheels'
    instance=unreal.load_asset(path)
    if not instance:
        instance=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_UtilityVanWheels',OUT+'/Materials',unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(instance,material)
    edit.set_material_instance_scalar_parameter_value(instance,'WheelDetailStrength',1.0)
    return instance


def main():
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.OBJ 0')
    tasks=[]
    for source in sorted((ROOT/'SourceArt/UtilityVan/Textures').glob('*.png')):
        if EAL.does_asset_exist(OUT+'/Textures/'+source.stem):continue
        task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=OUT+'/Textures'
        task.destination_name=source.stem;task.automated=True;task.save=True;tasks.append(task)
    if tasks:tools.import_asset_tasks(tasks)
    tex={key:unreal.load_asset(OUT+'/Textures/T_UtilityVan_'+key) for key in ('BaseColor','Normal','Metallic','Roughness','AO')}
    assert all(tex.values()),'missing undamaged texture'
    for key,t in tex.items():
        t.set_editor_property('srgb',key=='BaseColor')
        if key=='Normal':t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif key!='BaseColor':t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    material=unreal.load_asset(OUT+'/Materials/M_UtilityVan')
    if not material:
        material=tools.create_asset('M_UtilityVan',OUT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        props={'BaseColor':unreal.MaterialProperty.MP_BASE_COLOR,'Normal':unreal.MaterialProperty.MP_NORMAL,
               'Metallic':unreal.MaterialProperty.MP_METALLIC,'Roughness':unreal.MaterialProperty.MP_ROUGHNESS,'AO':unreal.MaterialProperty.MP_AMBIENT_OCCLUSION}
        for i,(key,t) in enumerate(tex.items()):
            node=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionTextureSampleParameter2D,-400,i*220)
            node.set_editor_property('parameter_name',key);node.set_editor_property('texture',t)
            node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if key=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if key=='BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            assert unreal.MaterialEditingLibrary.connect_material_property(node,'RGB' if key in ('BaseColor','Normal') else 'R',props[key])
        unreal.MaterialEditingLibrary.recompile_material(material)
    # Persist skeletal permutations explicitly: editor auto-usage is not reliable in PIE/cook.
    unreal.MaterialEditingLibrary.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    instance=unreal.load_asset(OUT+'/Materials/MI_UtilityVan')
    if not instance:
        instance=tools.create_asset('MI_UtilityVan',OUT+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance,material)
    unreal.MaterialEditingLibrary.set_material_usage_override(instance,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH,False,True)
    wheel_instance=wheel_material(material)
    source=unreal.load_asset(OUT+'/Meshes/SM_UtilityVan_Source')
    if not source:
        task=unreal.AssetImportTask();task.filename=str(ROOT/'SourceArt/UtilityVan/UtilityVan.obj')
        task.destination_path=OUT+'/Meshes';task.destination_name='SM_UtilityVan_Source';task.automated=True;task.save=True
        options=unreal.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False
        options.import_materials=False;options.import_textures=False;options.automated_import_should_detect_type=False
        options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        data=options.static_mesh_import_data;data.combine_meshes=True;data.convert_scene=False;data.convert_scene_unit=False
        data.generate_lightmap_u_vs=False;data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
        task.options=options;tools.import_asset_tasks([task]);source=unreal.load_asset(OUT+'/Meshes/SM_UtilityVan_Source')
    assert source,'source mesh import failed'
    for i in range(len(source.get_editor_property('static_materials'))):source.set_material(i,instance)
    unreal.log('VAN source bounds '+str(source.get_bounds()))
    unreal.log('VAN slots '+str([str(m.material_slot_name) for m in source.get_editor_property('static_materials')]))
    centers=json.loads((ROOT/'SourceArt/UtilityVan/WheelCenters.json').read_text())
    mesh=unreal.load_asset(OUT+'/Meshes/SK_UtilityVan')
    if not mesh:mesh=unreal.QRVehicleEditorLibrary.build_van_rig(source,OUT,[unreal.Vector(*centers[k]) for k in ('Wheel_FL','Wheel_FR','Wheel_BL','Wheel_BR')])
    assert mesh,'native van rig failed'
    assert unreal.QRVehicleEditorLibrary.repair_van_active_bones(mesh),'Van render LOD bone repair failed'
    # Re-running import must also repair existing skeletal assets, not just the static source.
    slots=mesh.get_editor_property('materials')
    for i in range(len(slots)):
        slot=slots[i]
        slot.set_editor_property('material_interface',wheel_instance if str(slot.material_slot_name).startswith('Wheel_') else instance)
        slots[i]=slot
    mesh.set_editor_property('materials',slots)
    anim=unreal.load_asset(OUT+'/Animation/ABP_UtilityVan')
    if not anim:anim=unreal.QRVehicleEditorLibrary.build_van_animation(mesh,OUT+'/Animation/ABP_UtilityVan')
    assert anim,'native wheel animation graph failed'
    for side,parent in [('Front',unreal.RoadToWisdomOffroadWheelFront),('Rear',unreal.RoadToWisdomOffroadWheelRear)]:
        bp=blueprint('BP_UtilityVanWheel'+side,parent)
        default=unreal.get_default_object(bp.generated_class())
        default.set_editor_property('wheel_radius',35.95);default.set_editor_property('wheel_width',29.1)
        default.set_editor_property('suspension_max_raise',12.0);default.set_editor_property('suspension_max_drop',15.0)
        if side=='Front':default.set_editor_property('max_steer_angle',32.0)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    bp=blueprint('BP_UtilityVan_Pawn',unreal.RoadToWisdomUtilityVan)
    default=unreal.get_default_object(bp.generated_class())
    off=unreal.get_default_object(EAL.load_blueprint_class('/Game/VehicleTemplate/Blueprints/OffroadCar/BP_OffroadCar_Pawn'))
    for prop in ('steering_action','throttle_action','brake_action','handbrake_action','look_around_action','toggle_camera_action','reset_vehicle_action'):
        default.set_editor_property(prop,off.get_editor_property(prop))
    sk=default.get_component_by_class(unreal.SkeletalMeshComponent)
    sk.set_skeletal_mesh_asset(mesh)
    sk.set_editor_property('bPerBoneMotionBlur',False)
    sk.set_editor_property('animation_mode',unreal.AnimationMode.ANIMATION_BLUEPRINT)
    sk.set_editor_property('anim_class',anim.generated_class())
    sk.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    sk.set_editor_property('relative_location',unreal.Vector(0,0,0));sk.set_editor_property('relative_rotation',unreal.Rotator())
    sk.set_collision_profile_name('Vehicle');sk.set_simulate_physics(True)
    movement=default.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)
    original=off.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)
    for prop in ('engine_setup','transmission_setup','differential_setup','steering_setup'):
        movement.set_editor_property(prop,original.get_editor_property(prop))
    engine=movement.get_editor_property('engine_setup');engine.set_editor_property('max_torque',450.0);engine.set_editor_property('max_rpm',4500.0);movement.set_editor_property('engine_setup',engine)
    setup=movement.get_editor_property('wheel_setups')
    for i,w in enumerate(setup):
        w.set_editor_property('wheel_class',EAL.load_blueprint_class(OUT+'/Blueprints/BP_UtilityVanWheel'+('Front' if i<2 else 'Rear')))
        setup[i]=w  # Unreal struct-array iteration returns copies.
    movement.set_editor_property('wheel_setups',setup)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    configured=unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.SkeletalMeshComponent)
    assert configured.get_editor_property('anim_class')==anim.generated_class(),'Wheel animation binding lost during Blueprint compilation'
    assert configured.get_editor_property('animation_mode')==unreal.AnimationMode.ANIMATION_BLUEPRINT,'Incorrect van animation mode'
    assert not configured.get_editor_property('bPerBoneMotionBlur'),'Wheel per-bone motion blur enabled'
    configured_movement=unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)
    for i,w in enumerate(configured_movement.get_editor_property('wheel_setups')):
        expected=EAL.load_blueprint_class(OUT+'/Blueprints/BP_UtilityVanWheel'+('Front' if i<2 else 'Rear'))
        assert w.get_editor_property('wheel_class')==expected,'Wheel setup lost during struct-array assignment'
    for path in ('/Game/VehicleTemplate/Blueprints/BP_VehicleAdvGameMode','/Game/VehicleTemplate/Blueprints/BP_VehicleAdvPlayerController'):
        obj=unreal.load_asset(path);cdo=unreal.get_default_object(obj.generated_class())
        cdo.set_editor_property('default_pawn_class' if 'GameMode' in path else 'vehicle_pawn_class',bp.generated_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(obj);EAL.save_loaded_asset(obj)
    assert unreal.MaterialEditingLibrary.has_material_usage(instance,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH),'Van material lacks skeletal usage'
    assert all(s.get_editor_property('material_interface')==(wheel_instance if str(s.material_slot_name).startswith('Wheel_') else instance) for s in mesh.get_editor_property('materials')),'Van skeletal slots were not assigned'
    EAL.save_directory(OUT,only_if_is_dirty=False,recursive=True)
    unreal.log('[UTILITY-VAN] undamaged native rig and Chaos pawn saved')

if __name__=='__main__':main()
