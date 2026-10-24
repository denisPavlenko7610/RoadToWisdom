// UT Templates - interactable interface.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UTInteractionTypes.h"
#include "UTInteractable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UUTInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the interactor can use: doors, levers, NPCs, pickups.
 * Implement in Blueprint actors or C++; the interactor only talks through this interface.
 * Native callers use UHT's Execute_CanInteract / Execute_Interact / Execute_GetInteractionPrompt /
 * Execute_GetInteractionOptions / Execute_GetInteractionPriority.
 *
 * Options: GetInteractionOptions lets one target offer several behaviors (a door can
 * offer Open and Lockpick; an NPC can offer Talk and Trade). The default implementation
 * wraps the legacy single-prompt path, so existing implementers keep working unchanged.
 *
 * Server authority: the interactor re-checks CanInteract on the server before executing
 * Interact - a client trace only decides what UI shows. Implementations must therefore
 * make Interact safe to call server-side and must not rely on client-only state.
 */
class UTINTERACTIONRUNTIME_API IUTInteractable
{
	GENERATED_BODY()

public:
	/** Gate the interaction (locked, out of range conditions, already busy). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Interaction")
	bool CanInteract(const FUTInteractionInfo& Info) const;

	/** Perform the interaction. Info.OptionId says which offered option was chosen. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Interaction")
	void Interact(const FUTInteractionInfo& Info);

	/** Short prompt for the default option; empty hides the prompt. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Interaction")
	FText GetInteractionPrompt(const FUTInteractionInfo& Info) const;

	/** Behaviors this target currently offers, in display order. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Interaction")
	void GetInteractionOptions(const FUTInteractionInfo& Info, TArray<FUTInteractionOption>& OutOptions) const;

	/** Higher priority wins ties in candidate scoring (mission targets, item pickups). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Interaction")
	int32 GetInteractionPriority() const;

	virtual bool CanInteract_Implementation(const FUTInteractionInfo& Info) const { return true; }
	virtual void Interact_Implementation(const FUTInteractionInfo& Info) {}
	virtual FText GetInteractionPrompt_Implementation(const FUTInteractionInfo& Info) const
	{
		return NSLOCTEXT("UTInteraction", "DefaultPrompt", "Interact");
	}
	virtual void GetInteractionOptions_Implementation(const FUTInteractionInfo& Info, TArray<FUTInteractionOption>& OutOptions) const
	{
		FUTInteractionOption DefaultOption;
		DefaultOption.Prompt = GetInteractionPrompt_Implementation(Info);
		OutOptions.Add(DefaultOption);
	}
	virtual int32 GetInteractionPriority_Implementation() const { return 0; }
};
