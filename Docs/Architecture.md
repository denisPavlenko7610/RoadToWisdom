# Architecture — Quiet Road

## Current playable world

The default map is `/Game/QuietRoadWorld/Maps/L_QuietRoad_OpenWorld`, a saved
4032 m World Partition region. It uses BP_VehicleAdvGameMode and
BP_UtilityVan_Pawn, with the existing template vehicle controller and HUD.
The undamaged supplied Utility Van has a native Chaos rig and WheelController
animation. SportsCar is retired; Offroad and shared template dependencies remain.
The main map does not currently run the legacy mission, on-foot or save loop.

## Ownership and module boundaries

- Game rules live in the RoadToWisdom C++ runtime module. Blueprints configure
  meshes, animation, wheels, input and presentation. Dependency direction is
  game → Toolkit; Toolkit never depends on this game.
- The seven Adventure closure plugins are shared NTFS junctions into the sibling
  UnrealTemplates repository. Never patch them through this project.
- VehicleTemplate contains retained template input, cameras, controllers and
  variants. Gameplay/Vehicle contains the Utility Van configuration and lights.
  Input completion/cancellation resets the corresponding vehicle command;
  releasing throttle does not clear a simultaneously held brake.
- Gameplay/Character, Player, Items, Missions, Save and UI retain the earlier
  slice implementation. Their presence does not mean they are active on the
  current default map. The retired prototype map is not an acceptance fixture.

## Existing Toolkit integration (legacy slice)

UTInteraction provides focus, prompts and hold interactions; UTItems owns item
transfer and inventory snapshots. Game-owned fuse-box and mission code composes
these with UTWorldMechanics' lever. UTSave owns versioned sections and saveable
world state; the game SaveCoordinator owns orchestration, pickup identities and
validation of the six required sections, including Pickups v2.
The coordinator matches the complete loaded saveable set by stable ID and exact
captured struct type. Mission-specific fuse consistency is checked independently.
Rejected replay attempts compensation using the pre-load world snapshot; failed
compensation blocks persistence in that world until map reload. Toolkit remains
unchanged. World v1 is preserved; new mandatory actors require an explicit migration.
Streaming persistence is not implemented: all persistent actors must be loaded.
The isolated /Game/Tests/L_SaveSession fixture exercises the earlier mission;
it does not enable that mission on the main map.
Restore publishes no gameplay events; the orchestrator rebuilds derived state.
The lever lacks IUTSaveable, so the existing mission section carries its state.
UTProgression is available but has no active reward integration. UTCombat is a
mandatory WorldMechanics dependency, with no direct game usage.

## Editor generation

Tools/World/PrepareWorld.py prepares deterministic height, route and placement
arrays with numpy. GenerateWorld.py creates and saves native Landscape, spline
roads and cell-based HISM through RoadToWisdomEditor. No runtime world generation
or custom streaming framework is introduced. World Partition streams saved actors.
GPU RHI is required for Landscape generation and proxy heightmap splitting.

The generator replaces only QRWorld.Generated actors in selected categories;
manual actors survive. Project materials and collision derivatives live under
QuietRoadWorld; original Fab assets remain unchanged. External Actors are part
of source control. HLOD and foreground performance need separate evidence.

## Next integration boundary

First revalidate the corrected 35.95 cm van wheels across the road network and
profile a foreground drive. Missions remain deferred under the current project
scope. When that scope is expanded, reconnect one park/exit/interact/return/save
loop using the existing game classes and Toolkit contracts, adapting persistence
to World Partition actor availability before expanding mission content.
No GAS, StateTree, SmartObjects, quest framework or new manager is needed.
