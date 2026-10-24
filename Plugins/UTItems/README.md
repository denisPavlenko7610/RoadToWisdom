# UT Items

Portable item definitions/registry, stack inventory, unique runtime instances and equipment.
Inventory and equipment both resolve `ItemId` through `UUTItemRegistrySubsystem`;
neither depends on the other. No runtime module depends on UTExamples.

## Inventory API

- Stacks: `AddItem`, `RemoveItem`, `ConsumeItems`, `GetItemCount`.
- Unique items: `AddInstance`, `GetInstance`, `SetInstanceState`, `RemoveInstance`.
- Both: `GetUsedSlotCount`, `MakeSnapshot`, `RestoreFromSnapshot`, `ClearInventory`.

A stack remains `(ItemId, Count)` in the existing FastArray. A unique item is a
`FUTItemInstance` value in a separate FastArray: persistent `FGuid InstanceId`, registry
`ItemId`, and `FUTItemInstanceState`. Each unique item occupies one slot. Stack queries
and definition-based removal intentionally address stacks only: they cannot accidentally
consume a particular weapon or quest instance. Use the exact GUID to remove it.
Duplicate identities within an inventory are rejected. Ownership across inventories
must be transferred on the server by composition, never by copying an owned item.

```cpp
FUTItemInstance Weapon = FUTItemInstance::Create("Item.Rifle"); // server
Weapon.State.Ammo = 12;
Weapon.State.Durability = 100.f;
if (Inventory->AddInstance(Weapon))
{
    Weapon.State.Ammo = 11;
    Inventory->SetInstanceState(Weapon.InstanceId, Weapon.State);
}
```

The state has optional ammo/durability (-1 = unused), up to 32 rolled modifiers,
and an optional game-owned custom byte schema (name + version + up to 1024 bytes).
Values must be finite. Custom serializers must contain data only, never object
references, pointers or arbitrary UObject graphs. This deliberately avoids a
reflection-driven property bag or a UObject for every ordinary stackable item.

## Multiplayer

All mutation APIs reject client calls. The owning actor must replicate. Existing stack
FastArray deltas remain unchanged; unique add/update/remove deltas and complete initial
state travel through the second FastArray, including late join. There is no item minting
RPC. Games expose validated intents through owned actors/components.
`OnInventoryChanged` is the authoritative read-refresh event for unique items.
`OnItemAdded/Removed` retain their existing stack-only meaning. Separate FastArrays may
arrive independently; UI should refresh from current state rather than assume an atomic
cross-array network update. Persistent gameplay decisions run on the server.

## Save contract

`FUTInventorySnapshot` adds `Instances` alongside `Stacks`; old snapshots load with an
empty instance list. GUIDs and full state survive struct serialization. Never regenerate
a GUID on load or transfer. Invalid or duplicate instance records reject restore without
changing live state. Restore publishes no gameplay events. Game save sections should bump
their section version when adopting the expanded snapshot. ItemId redirects/migrations
and custom schema migrations are owned by the importing game.

## Dependencies, examples and tests

UTItemsRuntime: Engine + GameplayTags. Inventory: items + NetCore. Equipment additionally
uses UTFoundation. UTItemsEditor depends on DataValidation only in editor builds.

`/Game/UTExamples/Maps/ItemsDemo`: the example cube has Potion x5 and one unique weapon;
E spends a round while preserving its GUID. Inspect its Inventory snapshot in PIE.
`UT.Items.Instances` covers real archive serialization, mixed slot budgets, identity,
invalid state, snapshot rejection and client mutation guards. `UT.Multiplayer.ItemInstances`
in UTExamplesEditor exercises two PIE clients, deltas and an additional late joiner.
Run that test with a closed PIE session.

Not included: inventory UI, trading or automatic equipment-instance ownership.

## Loot tables

`UUTLootTable` is a Data Asset with definition, positive weight, inclusive min/max count
and an optional guaranteed flag per entry. `Roll(FRandomStream&, Rolls)` advances a
caller-owned stream; `RollWithSeed(Seed, Rolls)` is the Blueprint convenience API.
Results contain only ItemId/count records. Selection uses replacement, up to 1024 draws.
Guaranteed entries appear once per call, including calls with zero weighted draws.
Invalid entries (missing ids/definitions, nonpositive weights, invalid count ranges) are
skipped. Empty tables return empty results. There is no implicit actor spawning,
inventory lookup, grant, UI or nested table recursion.

Gameplay composition rolls on the server, then persists or replicates the committed
result. Seeds are reproducible for the same authored ordering and implementation;
they are not a promise of cross-engine-version random stream compatibility.
ItemsDemo's example inventory is seeded from a table in its server BeginPlay.
`UT.Items.LootTable` covers invalid entries, weighted selection, count bounds,
determinism, empty tables and guaranteed entries.

## World pickup and drop

`AUTWorldItem` contains one `FUTItemPayload` (a stack or unique instance) and uses
UTInteraction. `InitializeItem` is a server-only one-time content assignment.
`TryPickup(Collector)` rechecks actor validity, range, world, inventory and actual
acceptance. It removes exactly the accepted amount; full inventories leave loot intact.
A transaction guard rejects reentrant collection of the same actor.

`UUTItemDropComponent` belongs on the replicated player pawn next to inventory.
Server gameplay uses `DropStack`/`DropInstance`; clients request `ServerDropStack` or
`ServerDropInstance` through their owned component. The server chooses class, position
and actual content. It finishes spawn preparation before debiting inventory; rejected
amounts, missing identities and failed spawns leave the source untouched. Configure
`WorldItemClass` with your project's pickup appearance. Empty prepared actors cannot
be collected. Nothing here accepts client-supplied content, transforms or actor classes.

World items replicate content and movement and are spawned only on the server; current
content reaches late join through property replication. Dynamic pickup persistence is
a game/world-spawn policy; the inventory snapshot preserves unique item identity/state.
`UT.Items.WorldPickupDrop` exercises partial pickup, range/full rejection, exact identity
transfer, drop/pickup and failed preparation. `UT.Multiplayer.PickupRace` uses competing
RPCs from two clients and checks conservation of item quantity.

ItemsDemo: E picks up stack/unique items, G drops one unit of the first stack, H drops
the first unique item. The debug overlay shows identities and state; no UI framework.
UTInventoryRuntime now also depends on UTInteractionRuntime.

## Shared loot containers

`AUTLootContainer` stays in the item domain. `Open(User)`, `GetLootState()`,
`TakeItem(User, Index, Count)` and `TakeAll(User)` are the complete interaction surface.
It uses either `InitialContents` or a `UUTLootTable`; state is Closed -> Opened -> Looted.
Opening rolls once on the server. Preconfigured unique entries without ids receive a
GUID exactly once. Invalid entries are skipped. Shared contents are replicated as one
coherent struct for initial replication/late join. The generic interaction offers Open,
then Take All. UI may read contents and route its take intent through an owned controller;
the container itself exposes no RPC on an unowned world actor. Indices must be refreshed
after a successful take. All gameplay methods revalidate authority, user and range.

A configured `StableId` opts into `IUTSaveable`. Capture contains the phase and actual
remaining contents; restore is silent, validates the snapshot and never rerolls opened
or looted containers. Closed chests generate on first subsequent open. Persist the world
snapshot through UTSave; no spawned actor pointer is stored. Per-player rolls are deferred
because player identity, ownership and persistence deserve a separate explicit policy.

`/Game/UTExamples/Maps/ItemsLootDemo`: stack/unique pickups, drop keys G/H, loot calculation,
and a shared chest (E: open, E again: take). `UT.Items.ChestPersistence` covers partial
transfer, unique identity, nested world-state serialization and looted restore.
`UT.Multiplayer.ItemInstances` additionally verifies that existing clients and a new
late joiner see the looted chest. Inventory's world mechanics module depends on UTSave.

## Composition requirements and authored world content

`UUTItemCost` implements Foundation's small action-cost contract with an item id/count.
CanPay only queries the user's inventory; Pay rechecks authority and atomically consumes
the requested stack quantity. Locks/actions depend on the generic contract, not inventory
internals; Items does not depend on WorldMechanics.

World items optionally expose InitialItem for level authoring. Empty leaves initialization
to trusted spawning code. A configured unique item without an id gets a runtime GUID on
authority in BeginPlay; clients receive Content through replication. Drop classes should
leave InitialItem empty so the prepared drop transaction can supply the actual payload.

LootContainer's optional pure UseCondition gates open and every take operation, with no
specific lock dependency. Configure it on both roles for consistent local availability.

## Exact equipment identities

Equipment keeps the existing Equip(Slot, ItemId) path for definition-only loadouts.
EquipInstance(Slot, Instance) records the stable InstanceId alongside ItemId; runtime
state stays in inventory. One instance cannot occupy two slots. Callers validate
ownership and handle transfers through composition; Equipment still does not depend
on Inventory. GetEquippedInstanceId exposes the selected identity to UI/game code.

MakeEquipmentSnapshot/RestoreEquipmentSnapshot preserve exact identities and validate
slots/definitions/duplicates before committing silently. The original map snapshot is
explicitly definition-only. Restore definitions/catalog first. RepNotify commits lookup
caches before publishing changes and detects switching between two instances of one
definition. Small equipment slot arrays remain ordinary replicated arrays.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
