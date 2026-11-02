"""Read-only asset/dependency and legacy map loading regression, suitable for a clean checkout."""
import json,time
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
reg=unreal.AssetRegistryHelpers.get_asset_registry();reg.search_all_assets(True)
assets=reg.get_assets_by_path('/Game',True)
options=unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
missing=set();blueprints=[];maps=[]
for a in assets:
 for dep in reg.get_dependencies(a.package_name,options):
  name=str(dep)
  if name.startswith('/Game/') and not reg.get_assets_by_package_name(dep):missing.add(name)
 if str(a.asset_class_path.asset_name) in ('Blueprint','AnimBlueprint','WidgetBlueprint'):
  bp=a.get_asset();parent=unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp)
  assert parent,str(a.package_name)+' missing native parent'
  unreal.BlueprintEditorLibrary.compile_blueprint(bp)
  assert bp.generated_class(),str(a.package_name)+' missing generated class'
  blueprints.append(str(a.package_name))
les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for path in ('/Game/VehicleTemplate/Maps/VehicleBasic','/Game/Variant_OffRoad/Maps/Lvl_Offroad','/Game/Variant_TimeTrial/Maps/Lvl_Timetrial','/Game/QuietRoadWorld/Maps/L_QuietRoad_WorldPilot','/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld'):
 start=time.monotonic();assert les.load_level(path),path
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 maps.append({'map':path,'loaded':True,'seconds':time.monotonic()-start,'actors':len(actors)})
result={'assets':len(assets),'missing_game_dependencies':sorted(missing),'compiled_blueprints':blueprints,'maps':maps,'passed':not missing}
out=ROOT/'Saved/WorldGeneration/M23Assets.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2))
unreal.log('[M23-ASSETS] '+json.dumps(result))
assert not missing,sorted(missing)
