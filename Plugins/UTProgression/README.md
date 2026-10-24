# UT Progression

XP/levels (threshold or linear curve), wallet currencies (atomic add/spend), quest
tracking over typed messages (objectives advance on bus broadcasts; rewards stay in
game code). All state server-authoritative and replicated; clients get change events
through OnRep diffs.

Quest progress is replicated compactly (ids + counts; definitions stay local assets)
with server-authoritative accept/abandon/advance. An editor module
(UTProgressionEditor) validates quest definitions. **Dependencies:** UTFoundation
(results, message bus). **Multiplayer:** AddXp / wallet
changes run on the authority only. **Example:** ProgressionDemo; quest objectives
listen on the message bus. **Not solved:** achievements, skill trees, shops.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
