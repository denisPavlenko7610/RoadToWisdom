# UT Save

Versioned save sections over engine `USaveGame` with an exact-version read contract,
whole-slot **transactional load** (temp + validate + replace; failures never destroy
live state), engine-async `SaveToSlotAsync`/`LoadFromSlotAsync`, and the world-state
layer: `IUTSaveable` (StableId / CaptureState / RestoreState) +
`UUTWorldStateSubsystem` (capture/apply by stable id, duplicate-id validation).

An editor module (UTSaveEditor) validates loaded worlds for duplicate
`IUTSaveable` stable ids. **Dependencies:** UTFoundation. **Multiplayer:** world capture/restore is server-only; restored
world state reaches clients through normal replication. Low-level slot/section IO
is also usable for local project data; game code owns gameplay authority. **Example:** AUTDoor implements
IUTSaveable; demo maps carry a saveable door. **Not solved:** migration callbacks
(read raw bytes via ReadSectionBytes), client-side saves.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
