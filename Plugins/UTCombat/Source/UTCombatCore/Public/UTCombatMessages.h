// UT Templates - combat-domain gameplay messages.
#pragma once

#include "CoreMinimal.h"
#include "UTCombatMessages.generated.h"

/**
 * Cross-domain death notice, broadcast on the message bus under UTTags::TAG_UT_Message_ActorDied
 * (tag "UT.Message.ActorDied") by UT health implementations on the authority.
 *
 * Consumers: respawn systems, quest objectives (kill counting), UI overlays. For
 * presentation-only reactions prefer the health component's OnDied delegate - the bus
 * message is for logic that has no reference to the target's components.
 */
USTRUCT(BlueprintType)
struct UTCOMBATCORE_API FUTActorDiedMessage
{
	GENERATED_BODY()

	/** The actor whose health reached zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Victim;

	/** Actor credited with the kill (damage instigator), if any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Killer;
};
