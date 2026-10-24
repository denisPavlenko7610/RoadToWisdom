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
