# Architecture — Quiet Road slice (1 page)

## Layers

```

UnrealTemplates plugins (junction, sibling repo)      game module RoadToWisdom

┌──────────────────────────────────────────┐   ┌────────────────────────────────────┐

│ UTFoundation   FUTResult, message bus    │   │ Gameplay/Vehicle — (template pawn, │

│ UTInteraction  IUTInteractable,          │   │   no game subclass yet)            │

│                UUTInteractorComponent    │◄──┤ Gameplay/Character                 │

│ UTItems        definitions, inventory,   │   │   OnFootCharacter (capsule +       │

│                world items               │   │   interactor + inventory)          │

│ UTSave         sections, world state,    │◄──┤ Gameplay/ (root)                   │

│                IUTSaveable               │   │   ExplorerController (possession   │

│ UTWorldMechanics lever/door/…            │   │   switch, IMCs, HUD, F)            │

│ UTCombat       (dependency only)         │   │ Gameplay/Missions                  │

│                                          │   │   FuseBox (IUTInteractable +       │

│ Blueprint/content: BP_SportsCar_Pawn as  │   │   IUTSaveable), MissionManager,    │

│ the van, maps, IMCs/IAs, item DA         │   │   SaveCoordinator (GameInstance    │

│                                          │   │   subsystem)                       │

│                                          │   │ Gameplay/UI: HUDWidget (programmatic)

└──────────────────────────────────────────┘   └────────────────────────────────────┘

```

Dependency direction: game → UT only. `Plugins/UT*` are junctions into the

UnrealTemplates repo (never edited from here; never committed here).

## Ownership of the core loop

- **Possession switching** (`ARoadToWisdomExplorerController`) — one controller owns

  both modes; the template controller's `OnPossess`/`SetupInputComponent` are

  overridden because they assume a vehicle-only player. The van is a placed actor,

  never respawned; the on-foot character is spawned once and stowed while driving.

  F is bound once in the controller, so rapid presses cannot duplicate pawns.

- **Mission 1** (`ARoadToWisdomMissionManager`) — a linear enum stage machine, not a

  quest framework (GDD §10). It wires the fuse box (`IUTInteractable` via

  UTInteraction; consumes `Item.Fuse` through the UT inventory) and the

  WorldMechanics lever (`OnSwitched`, stage-guarded so restores are idempotent).

- **Persistence** (`URoadToWisdomSaveCoordinator`, GameInstance subsystem) — the

  "orchestration in game code" half of the UT Save ownership table: writes

  Mission/Inventory/World/Vehicle/Player sections through `UUTSaveSubsystem` and

  captures saveable world state through `UUTWorldStateSubsystem`. `IUTSaveable` +

  stable ids are the identity contract (the fuse box carries

  `QuietRoad.Relay.FuseBox01`). Saves happen only on committed stage advances.

  Restore is deferred to the first possession and applies state silently.

- **HUD** (`URoadToWisdomHUDWidget`) — programmatic UMG (objective / prompt / hint);

  no Blueprint asset needed to run the slice, BP subclassing can restyle later.

- **Per-map GameMode** — `RoadToWisdomQuietRoadGameMode` binds via

  `GameModeMapOverrides`; template maps keep `BP_VehicleAdvGameMode` (no regression).

## Deliberate omissions

No GAS/StateTree/SmartObjects, no quest platform, no networking framework, no

physics/spawn missions yet (GDD M2/M3), no World Partition/PCG (until M2), no

UTExamples/UTShowcase/UTIntegrations.

## Known toolkit gap (documented, not patched here)

`AUTLever` lacks the `IUTSaveable` contract that `AUTDoor` has, so lever state rides

in the mission save section. Fix belongs in the UnrealTemplates repository.

M1.4: all six sections (including Pickups v2) are required. The coordinator validates

all payloads and fuse conservation before actor changes. Partial world-item payloads

are re-created through AUTWorldItem.InitializeItem; transfer logic stays in Toolkit.

Collected IDs reset on each restore attempt. Restore of AUTLever updates presentation

without broadcasting gameplay events. Radio E press starts UT's Save hold; release

within 0.3 s cancels it and requests Radio. Stowing cancels active interactions.

## Editor world generation (current priority)

Tools/World/PrepareWorld.py builds deterministic height/route/placement data outside UE (numpy). GenerateWorld.py saves native Landscape, road spline meshes and cell-based HISM through the editor-only RoadToWisdomEditor module. There is no custom runtime generation or streaming implementation. World Partition streams saved spatial actors; the main map uses existing VehicleAdv GameMode with temporary Offroad while mission integration is deferred.

Generator ownership is explicit: QRWorld.Generated plus category tags. Manual actors are never selected for replacement. Material instances and collision derivatives live under QuietRoadWorld; Fab originals are referenced, not edited. World Outliner folders separate routes, named settlements and nature categories. Gameplay/Toolkit ownership is unchanged.

## Source folders

The runtime module root contains only module/build entry files. Gameplay/Character, Player, Game, Items, Save, Missions, Vehicle, UI and World contain the corresponding game code. VehicleTemplate/Core, Wheels, UI, OffroadCar and Variants keep the retained template implementation separate. Tests remain under Tests. Editor-only native generation tools are under RoadToWisdomEditor/Public/WorldGeneration and Private/WorldGeneration. Include paths are module-relative; reflected class names and /Script/RoadToWisdom paths are unchanged.

SportsCar was explicitly retired by the user. Shared parent materials and their texture dependencies were moved to Vehicles/Shared before deletion; spawn/respawn Blueprint defaults and the VehicleBasic placed pawn now use Offroad. General chassis/wheel classes remain because Offroad inherits them. Offroad is temporary pending the future camper.

UtilityVan: concrete template pawn + Blueprint configuration; editor-only QRVehicleEditorLibrary creates the supplied undamaged mesh skeleton/skin weights, chassis PhysicsAsset and native WheelController AnimBP. No runtime mesh generation or custom wheel animation.
