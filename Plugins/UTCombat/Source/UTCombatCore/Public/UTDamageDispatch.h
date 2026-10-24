// UT Templates - damage dispatch: actor-level entry points into the damage contract.
#pragma once

#include "CoreMinimal.h"
#include "UTDamageContract.h"

class AActor;

/**
 * Actor-level dispatch for the damage contract.
 *
 * IUTDamageReceiver may be implemented by an actor OR by one of its components (the
 * reference implementation UUTHealthComponent is a component). Damage sources must not
 * probe the interface themselves - a `Actor->Implements<>()` check misses component
 * implementations, which silently makes ordinary pawns undamageable. Route everything
 * through these helpers: they check the actor first, then the actor's components.
 */
namespace UTCombat
{
/** The actor itself when it implements IUTDamageReceiver, else its first implementing component. Null when nothing does. */
UTCOMBATCORE_API UObject* ResolveDamageReceiver(const AActor* Actor);

/** Cheap gate: true when the actor resolves to a receiver that currently accepts damage. */
UTCOMBATCORE_API bool CanDamageActor(const AActor* Actor);

/** Resolve and apply. Returns an empty result when the actor has no accepting receiver. */
UTCOMBATCORE_API FUTDamageResult ApplyDamageToActor(AActor* Actor, const FUTDamageContext& Context);
} // namespace UTCombat
