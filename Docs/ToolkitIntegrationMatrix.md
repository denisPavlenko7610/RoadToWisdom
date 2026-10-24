# Toolkit Integration Matrix — RoadToWisdom / Quiet Road

Source toolkit: sibling repository `C:\_DiskE\Projects\Unreal\UnrealTemplates`
(revision `fdcf1bcc07b3685dcf9f28c9f8297e46bf97192a`, branch `main`, "next step").
The plugins are **referenced, not copied**: `Plugins/UT*` here are NTFS directory
junctions into `UnrealTemplates/Plugins/*` (both projects are developed in
parallel and share one copy of the sources). Recreate the junctions after a fresh
clone with `Tools/LinkToolkitPlugins.ps1` (idempotent); `Plugins/UT*` is gitignored.

Adopted set: **Adventure Kit closure** — `UTFoundation`, `UTInteraction`, `UTSave`,
`UTItems`, `UTProgression`, `UTWorldMechanics`, plus `UTCombat` (mandatory
dependency of `UTWorldMechanics`). Dependency closure verified against the actual
`.uplugin` manifests, not the GDD table alone.

Status vocabulary: `implemented` (code exists) → `compiled` (UBT error-free) →
`editor-verified` (map/asset actually loaded by an editor process) →
`manual-check-required` (needs human in PIE).

## Found plugins

| Plugin | Copied to | Runtime modules | Used by game code for | Game code touchpoints (actual API) | Status | Risk / test |
|---|---|---|---|---|---|---|
| UTFoundation | `Plugins/UTFoundation` | `UTFoundationRuntime` | `FUTResult` return values from save calls; message bus later | `FUTResult` (`UTResult.h`) | implemented → see build log | Low. Re-verified by every UT automation run; here compile + save/load smoke |
| UTInteraction | `Plugins/UTInteraction` | `UTInteractionRuntime` | Mission interaction (fuse box, fuse pickup prompt, lever focus) | `IUTInteractable` (`CanInteract/Interact/GetInteractionPrompt/GetInteractionOptions`), `UUTInteractorComponent` (`UpdateFocus`, `TryInteract`, `OnFocusChanged`, `InteractionDistance`) | implemented → see build log | Low. Trace channel `ECC_Visibility`; bind Enhanced Input action to `TryInteract` |
| UTSave | `Plugins/UTSave` | `UTSaveRuntime`, `UTSaveEditor` | Mission/inventory/world persistence | `UUTSaveSubsystem` (`WriteSection<T>/ReadSection<T>`, `SaveToSlot/LoadFromSlot`), `UUTWorldStateSubsystem` (`CaptureWorldState/ApplyWorldState`), `IUTSaveable` (stable `FName` ids) | implemented → see build log | Medium: stable ids must be configured per placed actor; duplicates fail capture by design — test via save/load roundtrip in PIE |
| UTItems | `Plugins/UTItems` | `UTItemsRuntime`, `UTInventoryRuntime`, `UTEquipmentRuntime`, `UTItemsEditor` | Fuse pickup + player inventory | `UUTItemDefinition` (Primary Data Asset, `ItemId`, `MaxStackSize`), `UUTItemRegistrySubsystem` (id → definition), `UUTInventoryComponent` (`AddItem/ConsumeItems/GetItemCount/HasItems`, `MakeSnapshot/RestoreFromSnapshot`), `AUTWorldItem` (placed pickup implementing `IUTInteractable`, `TryPickup`) | implemented → see build log | Low-Medium: item definitions are project Data Assets (`/Game/QuietRoad/Data`); registry must know `Item.Fuse` before pickup |
| UTProgression | `Plugins/UTProgression` | `UTProgressionRuntime`, `UTProgressionEditor` | Mission 1 reward (radio scanner unlock), later XP | `UUTProgressionComponent`, quest types reviewed (`UTQuestComponent`, `UTQuestDefinition`) — first slice keeps a plain mission-state journal in C++ and wires progression on M3 | integrated (plugin present) | GDD forbids a big quest framework now; keep journal minimal |
| UTWorldMechanics | `Plugins/UTWorldMechanics` | `UTWorldMechanicsRuntime` | Fuse-box switch (рубильник) | `AUTLever` (`OnSwitched`, `SetOn/Toggle`, `bUsable`, prompt), reviewed: `UUTTriggerVolume`, `UUTCollectibleComponent` | implemented → see build log | Low. Lever never references game actors — game subscribes to `OnSwitched` |
| UTCombat | `Plugins/UTCombat` | `UTCombatCore`, `UTCombatLite` | *none directly* — mandatory dependency of `UTWorldMechanics` | none (not declared in game `Build.cs`) | integrated (plugin present) | Do not call UTCombat APIs until an actual combat test map exists (GDD §11.1) |

## Engine plugins

| Plugin | Role | Status |
|---|---|---|
| ChaosVehiclesPlugin | Vehicle template physics (`AWheeledVehiclePawn`, `UChaosWheeledVehicleMovementComponent`) | enabled in `.uproject`, `compiled` (initial commit) |
| EnhancedInput | All player input (vehicle + on-foot contexts) | enabled, used by template C++ |
| ModelContextProtocol | HTTP MCP server for the UE editor (`http://127.0.0.1:8000/mcp`, autostart settings added to `DefaultEditorPerProjectUserSettings.ini`) | configured; `manual-check-required` (server only runs while the editor is open) |
| DataValidation | Editor-only, required by UTSave/UTItems/UTProgression manifests | enabled for editor targets in `.uproject` |
| PCG | Not enabled yet — needed only at M2 (GDD §17 M2), not before | not enabled |

## Deliberately not adopted

`UTExamples`, `UTShowcase`, `UTIntegrations` (GAS / StateTree / SmartObjects
adapters), `UTPhysicsMechanics`, `UTSpawnMechanics` — GDD §16 keeps them for later
missions (M2/M3) or separate test maps. They are **not copied** into this project;
when a mission needs them, copy the closure again per REUSE_GUIDE and add a row
here.

## Integration rules in force

- Game code declares only modules it actually calls (`Source/RoadToWisdom/RoadToWisdom.Build.cs`).
- No file under `Plugins/UT*` may be modified; bugs go back to the UT repo.
- World-state persistence goes through `IUTSaveable` + configured stable ids;
  save orchestration (sections + world snapshot) lives in game code
  (`Source/RoadToWisdom/Gameplay/...`), matching the UT ownership table.
- UT save/event contract: restore publishes no gameplay events; the loading
  orchestrator rebuilds derived state once at the end.

## Verification plan

1. UBT build of `RoadToWisdomEditor Win64 Development` — proves plugin closure
   compiles against this project. **Executed: Succeeded** (2026-10-24).
2. Headless editor smoke: load generated map `L_QuietRoad_Prototype`, check log for
   plugin load errors / missing assets. **Executed: PASSED**
   (`Tools/Editor/VerifyPrototype.py` — all actors present, mission refs wired,
   Item.Fuse payload correct, input IMCs resolve, 0 CDO errors).
3. Game-mode boot: `-game -NullRHI` on the slice map. **Executed: clean exit 0**
   (caught and fixed a missing-UFUNCTION delegate bug in the process).
4. PIE checklist: `Docs/TestingChecklist.md` — **manual-check-required** (needs a
   human in the editor; Unreal MCP server was not running during the session).
