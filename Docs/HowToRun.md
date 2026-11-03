# How to run RoadToWisdom

UE 5.8.4: `C:\Program Files\Epic Games\U_5.8\UE_5.8`.

## Setup and build

Restore Toolkit junctions after cloning:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LinkToolkitPlugins.ps1
& "C:\Program Files\Epic Games\U_5.8\UE_5.8\Engine\Build\BatchFiles\Build.bat" RoadToWisdomEditor Win64 Development "-project=$PWD\RoadToWisdom.uproject" -WaitMutex
```

Build with the Editor closed. The editor target includes RoadToWisdomEditor, a small editor-only native Landscape/spline/HISM bridge. No runtime world generation.

## Open and explore

Open RoadToWisdom.uproject. EditorStartupMap and GameDefaultMap point to `/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld`. The level uses the existing BP_VehicleAdvGameMode with BP_UtilityVan_Pawn as transport; it contains no permanent vehicle or mission actors. SportsCar assets/classes were explicitly retired; shared materials were moved to Vehicles/Shared. VehicleStart and ExplorerStart are grounded reserved spawn markers. World Partition cells can be loaded using its editor panel; PIE streams around the pawn.

The old L_QuietRoad_Prototype map was deleted on request. Vehicle Template and original Fab maps remain for reference. Earlier gameplay tests/tools naming that prototype are legacy, not acceptance tests for this new map.

## Persistence regression (isolated scene)

After building with the editor closed, run
`powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Test-SaveSession.ps1`.
The runner requires two passing exported Automation results (Persistence and RadioHold),
not merely exit code zero. It uses only QuietRoad_Automation, never QuietRoad01.
Reports: Saved/Automation/MultiSave/index.json; log: Saved/Logs/MultiSaveTests.log.
The fixture /Game/Tests/L_SaveSession is separate from the production world and uses
the existing Utility Van, mission classes, input and Toolkit. It does not certify
the mission loop on the main World Partition map.
To rebuild only the fixture, use UnrealEditor-Cmd with this project and
`-run=pythonscript -script=<absolute path>/Tools/Editor/BuildSaveTestFixture.py -NullRHI -unattended`.
The builder replaces only this dedicated fixture map; never run the retired prototype builder.

World v1 remains compatible with the same authored saveable set. Every saveable must
have a nonempty unique ID and valid state. Restore requires the same loaded ID/type set;
load all persistent actors first. Adding mandatory actors requires explicit migration.
On actor rejection the coordinator compensates from the previous snapshot. If that
also fails, reload the map: further save/restore is blocked for that world.

## Generate World

Edit `Config/WorldGeneration.json`. Settings: Seed, WorldSize (1008 / 2016 / 4032 m), TerrainVariation, RoadDensity, POIDensity, VegetationDensity, RockDensity. Defaults produce a 4032 m region.

Host Python needs numpy (preparation only):

```powershell
python -m pip install numpy
python Tools/World/PrepareWorld.py
```

**Do not generate Landscape with -NullRHI.** Native Landscape proxy splitting copies height textures through the rendering interface; a no-RHI run can retain valid collision while saving blank rendered heightmaps. VerifyWorld now checks rendered bounds as well as collision.

Then use the running editor's Python console (or MCP/Rider execute Python):

```python
import runpy, unreal
root = unreal.Paths.project_dir()
runpy.run_path(root + "Tools/World/GenerateWorld.py")["main"]()
```

This creates/saves the Landscape, roads, buildings, HISM and External Actors. It removes only Actor tagged QRWorld.Generated. Untagged hand-authored Actor survive. Regenerate only one category with `main("POI")`, `main("Roads")`, `main("Vegetation")`, `main("Rocks")`, `main("Terrain")` or `main("Lighting")`. Change terrain/road settings together using full generation, because their masks depend on each other. Arbitrary rectangle regeneration is not implemented.

Pilot: prepare with `python Tools/World/PrepareWorld.py --size 1008`, then run generation. Pilot map is L_QuietRoad_WorldPilot. Re-run preparation without override to return to the main map manifest. Saved/WorldGeneration contains temporary manifests, never required cooked assets.

## Landscape appearance

Edit `/Game/QuietRoadWorld/Materials/MI_RegionLandscape`: TileSize (cm), BroadTileSize, DetailMix, MacroTileSize, MacroTintStrength, DistanceStart, DistanceBlendLength, RockTint, RockSlopeStart/Strength, Roughness, NormalStrength. Parent M_RegionGround is project-owned; source Fab textures/materials are unchanged. Fine normals fade with distance. Shader graph creation is in `Tools/World/LandscapeMaterial.py`; `build(True)` intentionally rebuilds it, ordinary world regeneration preserves material-instance tuning.

## Scene organization

Generated/Terrain, Generated/Roads/<route>, Generated/POI/<settlement>, Generated/Nature/Vegetation/<type>, Generated/Nature/Rocks/<type>, Generated/Atmosphere. Start markers belong to Manual/Starts. System actors belong to World/System. Do not give manual gameplay Actor QRWorld.Generated tags.

## Verification and screenshots

```python
runpy.run_path(root + "Tools/World/VerifyWorld.py")["main"]()
runpy.run_path(root + "Tools/World/CaptureWorld.py")
```

VerifyWorld checks actual loaded Landscape collision, sample road contact, camper envelope clearance, building overlaps/foundations, instance counts, unique labels and VehicleStart. It records evidence in Saved/WorldGeneration. CaptureWorld uses a nonblocking editor tick callback for eight real HighResShot views; leave the editor running until Screenshots.json appears.

Programmatic surface/envelope checks and PIE streaming do not prove Chaos suspension/driving or foreground FPS. Full vehicle drive and Standalone profiling remain manual checks. No temporary vehicle is saved into the level.

## MCP

Engine ModelContextProtocol and EditorToolset are enabled, HTTP endpoint `http://127.0.0.1:8000/mcp`. Config is in .mcp.json and .codex/config.toml. Editor must be running. Diagnose native tools with `python Tools/Editor/UnrealMcp.py list`. RiderLink can execute editor Python. Never claim editor verification when the editor is down.

## Git

No Git LFS. Every source asset must be below 100 MiB, preferably below 50 MiB. Run `python Tools/CheckAssetBudget.py`; it includes pending assets and checks Git attributes. Commit maps together with Content/__ExternalActors__ and Content/__ExternalObjects__. Saved, Binaries, Intermediate and DerivedDataCache stay ignored.

## Utility van assets

Only the undamaged supplied model is imported. Rebuild its assets with the editor closed:

```powershell
& "C:/Program Files/Epic Games/U_5.8/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/RoadToWisdom.uproject" -run=pythonscript "-script=$PWD/Tools/Editor/ImportUtilityVan.py" -unattended -nop4
```

SourceArt/UtilityVan contains the undamaged OBJ, material groups, five textures and wheel centers. The native editor helper creates Root + four wheel bones, rigid skin weights, one chassis collision box and a VehicleAnimationInstance graph with WheelController. Runtime uses the stock template input/camera code and Chaos wheels. Assets live in Vehicles/UtilityVan/{Meshes,Materials,Textures,Physics,Animation,Blueprints}. Do not run synchronous Interchange import through Rider's task-graph callback; use this commandlet. Never regenerate Landscape with NullRHI.

Van material check: run `Tools/Editor/VerifyUtilityVanMaterials.py` in the editor, and again while PIE is active. It checks the saved skeletal usage on the base/instance, all five texture references, skeletal slots and the actual pawn component. A correct editor preview alone does not establish PIE/cooked shader support. The importer saves the skeletal usage explicitly.

## M2.3 acceptance tools

- `VerifyWorldAssets.py`: Asset Registry / native Blueprint parents and compilation, then loads VehicleBasic, Offroad, TimeTrial, pilot and main map. Run as an editor Python commandlet with the editor closed; check the log for Blueprint errors as well as its JSON result.
- `VerifyWorldBranches.py`: run in the normal editor on the main map. It starts PIE and drives the existing van down all seven non-Main routes. No vehicle or map edits are saved. Main has its separate QRW.RouteDrive acceptance. Results: M23Branches.json.
- `VerifyWorldStreaming.py`: separate PIE stress sweep of the player streaming source across the region, checking actual Landscape collision and loaded actor sets. This uses teleports, not vehicle-driving physics.
- `VerifyWorldRegeneration.py`: destructive generation acceptance **only in an isolated project copy whose path contains M23Clean**. Prepare its manifest first. Two full GPU-editor generations compare instance signatures, collision checks and a manual sentinel. Never run this fixture on production content. Commandlet Landscape generation can stall even with AllowCommandletRendering; use a regular GPU editor process.
- `CaptureWorld.py`: eight actual local editor shots, including small/large settlements and the overlook. NoAI Fab renders are kept local for human art approval.

The native editor repair ClearUnusedMaterialStreamingData refuses materials that use any texture. It removes stale streaming references from texture-free template materials without changing their parent or appearance.

## Van wheel animation

Run `FixVanWheelAnim.py` main() to persist the existing native wheel animation binding (editor, outside PIE). The importer applies the same settings. `VerifyVanWheelAnimation.py` main() starts a 13-second actual PIE drive and checks physical angles and bone rotations for all four wheels; it ends PIE automatically. Optional main(True) captures two isolated side views while retaining Landscape collision; only that temporary PIE world's environment rendering is hidden. The map is not saved or changed by this capture.

Wheel visibility: MI_UtilityVanWheels inherits the same five textures from M_UtilityVan. UV-bound steel vents and a valve make spin visible; the shader has no Time-driven rotation. Only wheel slots use this instance; the body retains MI_UtilityVan. Import explicitly writes edited material/wheel structs back to their Unreal arrays. The pawn uses BP_UtilityVanWheelFront/Rear (35.95 cm), not native Offroad wheels (50 cm).

The wheel regression also checks actual skinned vertices relative to each wheel bone, excluding suspension movement as a false positive. QRVehicleEditorLibrary provides this readback only in the editor module.

Motion blur is disabled globally by r.DefaultFeature.MotionBlur=False and r.MotionBlurQuality=0. The van mesh also disables per-bone blur. When importing the van, the editor helper restores the LOD active/required bone lists from skin sections; wheel regression checks use the render-path matrices, not the unfiltered bone cache.

Utility Van lighting: press L while driving to toggle headlights and rear running lights. Braking or the handbrake brightens the rear lights; verify placement and appearance in PIE.

## Vehicle input release — 2026-11-02

Completed and Canceled vehicle actions now explicitly clear steering/throttle,
service brake and handbrake. Releasing throttle no longer calls DoThrottle(0),
which also cleared a simultaneously held service brake. This affects the common
vehicle pawn used by Utility Van and retained template vehicles.
Manual check: hold brake and throttle, release throttle, confirm brake remains;
release/cancel each input and confirm its command clears. Context removal and
window focus changes still require actual PIE checks; no blanket guarantee of
focus-loss handling is made by these action bindings.