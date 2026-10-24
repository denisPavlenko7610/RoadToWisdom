# UT Combat

Two modules. **UTCombatCore** is the damage contract — `FUTDamageContext` /
`FUTDamageResult` / `IUTDamageReceiver` — plus the death message and actor-level
dispatch helpers (`UTCombat::ResolveDamageReceiver / CanDamageActor /
ApplyDamageToActor`). Sources must use the dispatch: `IUTDamageReceiver` may live on an
actor OR a component (the reference health implementation is a component on an ordinary
pawn), and raw `Actor->Implements<>()` checks miss the component case. **UTCombatLite** is
the lightweight stack for projects without GAS: health (damage rules, revive),
stats (modifiers), status effects (display replication), named cooldowns, stat
conditions.

**Dependencies:** Core <- Foundation; Lite <- Core + Foundation. **Multiplayer:**
health/stats/effects server-authoritative with replicated state; clients read final
values via OnRep. UUTHealthComponent is one IUTDamageReceiver implementation — a GAS
adapter is expected as a second one. **Example:** UTCombatDemo. **Not solved:** it is
explicitly not a GAS clone — no abilities, prediction, or gameplay cues.

## Reuse and verification

See [the reuse guide](../../REUSE_GUIDE.md) for the mandatory plugin closure,
minimal executable C++ host, Blueprint setup, ownership, state notifications and
save contract. [TEST_MATRIX.md](../../TEST_MATRIX.md) records executed checks and
remaining acceptance work; a declaration of replication is not a test result.
