"""Local Asset Registry metadata audit. No geometry/textures are exported."""
from collections import Counter
import json
from pathlib import Path
import unreal


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    root = Path(unreal.Paths.project_dir())
    data = []
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for asset in registry.get_assets_by_path("/Game/Nimikko_WesternTown", recursive=True):
        cls = str(asset.asset_class_path.asset_name)
        path = str(asset.package_name)
        record = dict(path=path, cls=cls)
        file = root / "Content" / (path.removeprefix("/Game/") + ".uasset")
        if cls == "World": file = file.with_suffix(".umap")
        record["bytes"] = file.stat().st_size if file.exists() else 0
        record["dependencies"] = [str(p) for p in
                                  registry.get_dependencies(asset.package_name, unreal.AssetRegistryDependencyOptions(
                                      include_soft_package_references=True, include_hard_package_references=True,
                                      include_searchable_names=False, include_soft_management_references=False,
                                      include_hard_management_references=False))]
        if cls == "StaticMesh":
            mesh = asset.get_asset()
            bounds = mesh.get_bounds()
            record["extent"] = [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]
            record["origin"] = [bounds.origin.x, bounds.origin.y, bounds.origin.z]
            record["lods"] = mesh.get_num_lods()
            record["collision"] = mesh_editor.get_simple_collision_count(mesh)
            record["materials"] = [str(mesh.get_material(i).get_path_name()) if mesh.get_material(i) else None
                                   for i in range(len(mesh.static_materials))]
        data.append(record)
    maps = []
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for name in ("WildWestVillage", "Assets"):
        path = "/Game/Nimikko_WesternTown/Maps/" + name
        assert les.load_level(path)
        actors = eas.get_all_level_actors()
        groups = Counter(str(a.get_folder_path()) for a in actors)
        landscapes = [a for a in actors if "Landscape" in a.get_class().get_name()]
        maps.append(dict(path=path, actors=len(actors),
                         classes=dict(Counter(a.get_class().get_name() for a in actors)),
                         folders=dict(groups),
                         landscape_count=len(landscapes)))
    report = dict(assets=data, maps=maps, classes=dict(Counter(a["cls"] for a in data)))
    output = Path(unreal.Paths.project_saved_dir()) / "WorldAssetAudit.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("[WORLD-AUDIT] " + json.dumps(dict(classes=report["classes"], maps=maps)))
    assert les.load_level("/Game/VehicleTemplate/Maps/VehicleBasic")


if __name__ == "__main__":
    main()