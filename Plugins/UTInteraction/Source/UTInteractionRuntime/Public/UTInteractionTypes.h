// UT Templates - interaction types.
#pragma once

#include "CoreMinimal.h"
#include "UTInteractionTypes.generated.h"

class AActor;

/** Context passed through one interaction attempt. */
USTRUCT(BlueprintType)
struct UTINTERACTIONRUNTIME_API FUTInteractionInfo
{
	GENERATED_BODY()

	/** Actor that initiated the interaction (player pawn, AI pawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Interactor;

	/**
	 * Which option the interactor chose ("Open", "Lockpick", "Talk"). NAME_None is the
	 * target's single default behavior - targets that do not care about options can
	 * ignore this field and keep implementing the plain Interact path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName OptionId;
};

/**
 * One selectable behavior offered by an interactable. A door may offer Open (tap) and
 * Lockpick (hold); an NPC may offer Talk and Trade. Options are reported live through
 * IUTInteractable::GetInteractionOptions, so the list can change with target state
 * (locked vs. unlocked) without any extra plumbing.
 */
USTRUCT(BlueprintType)
struct UTINTERACTIONRUNTIME_API FUTInteractionOption
{
	GENERATED_BODY()

	/** Stable id the target switches on in Interact. NAME_None is the default behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName OptionId;

	/** Short prompt for UI; empty hides this option from the prompt line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FText Prompt;

	/** Seconds the player must hold interact to trigger; 0 means a tap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT", meta = (ClampMin = "0.0"))
	float HoldDuration = 0.f;
};
