# UT Foundation

Shared primitives every other UT module builds on: `FUTResult` (success/failure codes),
the base condition set (`UUTConditionAnd/Or/Not/ActorHasTag` — pure, re-check safe),
the typed gameplay message bus (`UUTMessageBusSubsystem` over `FInstancedStruct` +
GameplayTag routing) and native gameplay tags (`UTTags::*`). Data validators live in
their domain plugins (UTItemsEditor, UTProgressionEditor, UTSaveEditor) so the
foundation never depends upward.

**Dependencies:** engine only (GameplayTags, StructUtils). **Multiplayer:** the message
bus is process-local; gameplay-critical messages are broadcast on the authority.
**Example:** checkpoint -> respawn communication in UTWorldMechanics.
**Deliberately not solved here:** async/threaded messaging (engine AsyncMessageSystem),
random gates (removed — conditions must be deterministic), stats-dependent conditions
(live in UTCombatLite).

## Optional action cost contract

UUTActionCost exposes pure CanPay(User) and all-or-nothing server Pay(User). It stores
no resources and knows no inventory, lock, door or combat implementation. Domains provide
small adapters (UUTItemCost); consuming mechanics revalidate and guard their transaction.
This is an interface-sized resource contract, not an action execution framework.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
