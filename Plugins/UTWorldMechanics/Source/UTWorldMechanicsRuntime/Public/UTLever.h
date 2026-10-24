// UT Templates - lever actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "UTInteractable.h"
#include "UTLever.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTLeverSwitchedSignature, AUTLever*, Lever, bool, bIsOn);

/**
 * Interactable on/off switch. What the lever controls is wiring of other actors -
 * subscribe to OnSwitched and call Open/Toggle/etc. in game or Blueprint code;
 * the lever never references other mechanics.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTLever : public AActor, public IUTInteractable
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Lever")
	FUTLeverSwitchedSignature OnSwitched;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Lever")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	/** Optional handle mesh rotated by HandleAngle when switched on. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Lever")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	/** Local-space rotation applied to HandleMesh when on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Lever")
	FRotator HandleOnRotation = FRotator(-45.f, 0.f, 0.f);

	UPROPERTY(ReplicatedUsing = OnRep_IsOn, EditAnywhere, BlueprintReadOnly, Category = "UT|Lever")
	bool bIsOn = false;

	/** Extra gate beyond the default (e.g. key needed): return false to refuse interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Lever")
	bool bUsable = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Lever")
	FText InteractionPrompt = NSLOCTEXT("UTLever", "PullPrompt", "Pull lever");

	AUTLever();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const override;
	UFUNCTION()
	void OnRep_IsOn();

	// -- IUTInteractable -----------------------------------------------------
	virtual bool CanInteract_Implementation(const FUTInteractionInfo& Info) const override { return bUsable; }
	virtual void Interact_Implementation(const FUTInteractionInfo& Info) override;
	virtual FText GetInteractionPrompt_Implementation(const FUTInteractionInfo& Info) const override { return InteractionPrompt; }

	// ------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "UT|Lever")
	void SetOn(bool bNewOn);

	UFUNCTION(BlueprintCallable, Category = "UT|Lever")
	void Toggle();

protected:
	virtual void BeginPlay() override;

private:
	void ApplyHandleRotation();
};
