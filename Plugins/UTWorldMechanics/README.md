# UT World Mechanics

Door (replicated open state, saveable), lever, pressure plate, trigger volume,
teleporter (per-instance re-entry guard), jump pad, moving platform, checkpoint +
respawn (typed message contract, per-player), pickup spawner, collectible, day-night
cycle, condition gate. Cross-mechanic wiring (plate -> door, collectible -> inventory)
is composition code, never library code.

**Dependencies:** Foundation, Interaction, CombatLite, Save. **Multiplayer:**
activation logic server-only; door open state replicated; respawn server-driven.
**Example:** WorldMechanicsDemo map. **Not solved:** ambient world building tools. Lock/key composition is described below.

## Generic lock capability

`UUTLockComponent` is independent of Door/Chest/Inventory. `CanUse`, `CanUnlock`,
`TryUnlock(User)`, `Lock`, `IsLocked`, `OnUnlocked` and `OnLockChanged` are its API.
UnlockConditions accepts existing tag/stat/custom condition trees. Optional UnlockCost
is a Foundation `UUTActionCost` contract (pure CanPay + authority all-or-nothing Pay).
Items supplies UUTItemCost without a dependency on locks or WorldMechanics. bConsumeKey
chooses whether a successful unlock spends the requirement. An unconfigured cost adds
no resource requirement. UnlockDistance adds a server spatial check; zero explicitly
allows trusted non-spatial composition such as encounter completion.

Player requests go through their owned interactor; there is no RPC on an unowned lock.
Authority rechecks range/condition/cost. A transaction guard prevents reentrant double
spending. Locked state replicates and supports late join. MakeSnapshot/RestoreFromSnapshot
let the saveable owner include the capability in its aggregate actor state; restore is
silent. The component does not register a second competing actor StableId.

`UUTConditionLockUnlocked` is a pure adapter for generic gates. Loot containers accept
a Foundation UseCondition and do not import lock or inventory internals. Authored
conditions must exist on both roles for matching local prompts; server validation is
always authoritative.

`/Game/UTExamples/Maps/LockKeyDemo` composes a normal world pickup, item cost, lock,
UTDoor and UTInteraction. Thin example bridge classes aggregate interaction/save and
visuals; they do not implement door movement, key accounting or lock state themselves.
`UT.World.LockConditionsAndKey` covers key requirements/consumption, rejected conditions,
client guards, snapshots and chest gating. The broader network sequence is tracked in
the root matrix.

Respawn's selected point replicates to the owning client. Only authority subscribes
to gameplay checkpoint/death messages, and subscriptions/timers are removed on EndPlay.

Door target changes animate locally through normal replication. A connection also
receives an initial-only pose, so an already open door does not replay opening on late
join. This adds no per-frame transform RPC. Silent restore snaps state and disables
pending animation tick, preventing a delayed gameplay completion event after loading.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
