// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UTInteractable.h"
#include "RoadToWisdomVanRadioPanel.generated.h"

class UStaticMeshComponent;
class ARoadToWisdomMissionManager;
class ARoadToWisdomExplorerController;
struct FUTInteractionInfo;
struct FUTInteractionOption;

/**
 *  Minimal radio panel of the van (GDD §23.1): the on-foot player taps E to hear the
 *  current objective and a short radio line, or holds E to save the session through
 *  the existing save coordinator (UTInteraction hold contract, no new UI framework).
 *
 *  The panel is a placed actor next to the driver door that attaches itself to the
 *  one tagged van in BeginPlay so it rides along; it owns no state and therefore has
 *  nothing to save of its own. A trunk (GDD §23.2) is deliberately deferred: UTItems
 *  does provide UUTLootContainer, but nothing in the current loop needs a second
 *  storage, and duplicating storage just for looks is forbidden by the GDD.
 */
UCLASS()
class ARoadToWisdomVanRadioPanel : public AActor, public IUTInteractable
{
	GENERATED_BODY()

public:
	ARoadToWisdomVanRadioPanel();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuietRoad")
	TObjectPtr<UStaticMeshComponent> PanelMesh;
	UPROPERTY(EditAnywhere, Category = "QuietRoad", meta = (ClampMin = "0"))
	float UseDistance = 250.f;

	// -- IUTInteractable -------------------------------------------------------
	virtual bool CanInteract_Implementation(const FUTInteractionInfo& Info) const override;

	virtual void Interact_Implementation(const FUTInteractionInfo& Info) override;

	virtual FText GetInteractionPrompt_Implementation(const FUTInteractionInfo& Info) const override;

	virtual void GetInteractionOptions_Implementation(
		const FUTInteractionInfo& Info, TArray<FUTInteractionOption>& OutOptions
	) const override;

	virtual int32 GetInteractionPriority_Implementation() const override;

protected:
	virtual void BeginPlay() override;

private:
	void ShowHudMessage(const FText& Message, float Seconds) const;

	/** Tag of the van this panel attaches to (must match the controller's). */
	UPROPERTY(EditAnywhere, Category = "QuietRoad")
	FName VanTag = TEXT("QuietRoad.Van");

	/** Seconds a save hold must run (UTInteraction hold contract, server-verified). */
	UPROPERTY(EditAnywhere, Category = "QuietRoad", meta = (ClampMin = "0.5"))
	float SaveHoldSeconds = 1.2f;
};
