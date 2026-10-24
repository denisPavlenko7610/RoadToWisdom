# UT Interaction

Trace-based interaction with candidate **scoring** (priority, then angular miss, then
distance), always-fresh **prompt refresh** (same actor still re-evaluates Open -> Close /
Locked -> Unlock), **multi-option targets** (`FUTInteractionOption`: id, prompt, hold
duration), a built-in **hold interaction** state machine, and server-validated client
requests (`ServerInteract` re-checks target, range and CanInteract).

**Dependencies:** engine only. **Multiplayer:** focus tracing is local UI; taps and holds are server-authoritative.
Holds run a server-side contract: the client reports start/cancel, the server verifies
the option is currently offered, owns the HoldDuration timer and executes itself - a
client cannot skip or shorten a hold. **Example:** ADemoInteractableDoor in UTExamples
(Open / Lockpick-hold / Examine). **Not solved:** UI framework (CommonUI), input
binding (Enhanced Input), lag compensation.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
