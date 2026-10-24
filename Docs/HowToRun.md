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

Open RoadToWisdom.uproject. EditorStartupMap and GameDefaultMap point to `/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld`. The level uses the existing BP_VehicleAdvGameMode with BP_OffroadCar_Pawn as temporary transport; it contains no permanent vehicle or mission actors. SportsCar assets/classes were explicitly retired; shared materials were moved to Vehicles/Shared. VehicleStart marks the future van position. World Partition cells can be loaded using its editor panel; PIE streams around the pawn.

The old L_QuietRoad_Prototype map was deleted on request. Vehicle Template and original Fab maps remain for reference. Earlier gameplay tests/tools naming that prototype are legacy, not acceptance tests for this new map.

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

VerifyWorld checks actual loaded Landscape collision, sample road contact, camper envelope clearance, building overlaps/foundations, instance counts, unique labels and VehicleStart. It records evidence in Saved/WorldGeneration. CaptureWorld uses a nonblocking editor tick callback for six real HighResShot views; leave the editor running until Screenshots.json appears.

Programmatic surface/envelope checks and PIE streaming do not prove Chaos suspension/driving or foreground FPS. Full vehicle drive and Standalone profiling remain manual checks. No temporary vehicle is saved into the level.

## MCP

Engine ModelContextProtocol and EditorToolset are enabled, HTTP endpoint `http://127.0.0.1:8000/mcp`. Config is in .mcp.json and .codex/config.toml. Editor must be running. Diagnose native tools with `python Tools/Editor/UnrealMcp.py list`. RiderLink can execute editor Python. Never claim editor verification when the editor is down.

## Git

No Git LFS. Every source asset must be below 100 MiB, preferably below 50 MiB. Run `python Tools/CheckAssetBudget.py`; it includes pending assets and checks Git attributes. Commit maps together with Content/__ExternalActors__ and Content/__ExternalObjects__. Saved, Binaries, Intermediate and DerivedDataCache stay ignored.
