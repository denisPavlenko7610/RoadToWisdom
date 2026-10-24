// UT Templates - pressure plate actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "GameplayTagContainer.h"
#include "UTPressurePlate.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTPressurePlatePressedSignature, AActor*, Activator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTPressurePlateReleasedSignature, AActor*, LastActivator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTPressurePlateActivatorSignature, AActor*, Activator);

/**
 * Overlap-triggered plate. Pressed while at least one valid activator stands on it;
 * OnPressed/OnReleased fire on the 0->1 and 1->0 transitions.
 *
 * Activation filtering uses GameplayTags via IGameplayTagAssetInterface: with empty
 * RequiredTags any overlapping actor activates; otherwise the actor must own the tags.
 *
 * An actor is counted once however many of its components overlap, so a capsule plus a
 * weapon mesh does not press the plate twice or release it while still standing on it.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTPressurePlate : public AActor
{
	GENERATED_BODY()

public:
	// Named with a Plate prefix: AActor already owns OnPressed/OnReleased.
	UPROPERTY(BlueprintAssignable, Category = "UT|PressurePlate")
	FUTPressurePlatePressedSignature OnPlatePressed;

	UPROPERTY(BlueprintAssignable, Category = "UT|PressurePlate")
	FUTPressurePlateReleasedSignature OnPlateReleased;

	/** Per-actor enter/exit, for sounds and one-off reactions. */
	UPROPERTY(BlueprintAssignable, Category = "UT|PressurePlate")
	FUTPressurePlateActivatorSignature OnActivatorEntered;

	UPROPERTY(BlueprintAssignable, Category = "UT|PressurePlate")
	FUTPressurePlateActivatorSignature OnActivatorExited;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|PressurePlate")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Empty = any actor; otherwise the actor must own these tags (any/all per RequireAllTags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|PressurePlate")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|PressurePlate")
	bool bRequireAllTags = false;

	/** True while at least one tracked activator has a component on the plate. */
	UFUNCTION(BlueprintPure, Category = "UT|PressurePlate")
	bool IsPressed() const;

	AUTPressurePlate();

protected:
	virtual void BeginPlay() override;

private:
	/**
	 * Accepted overlapping components per activator. A bare counter (the previous int32
	 * ActiveActivators) could not tell which actor left: one actor's second component
	 * exiting released the plate while its first component was still on the plate.
	 */
	TMap<TWeakObjectPtr<AActor>, int32> ActivatorComponentCounts;

	bool IsValidActivator(AActor* Actor) const;

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
						  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
