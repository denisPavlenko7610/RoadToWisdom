// UT Templates - trigger volume actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "GameplayTagContainer.h"
#include "UTTriggerVolume.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTTriggerEnteredSignature, AUTTriggerVolume*, Volume, AActor*, Actor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTTriggerExitedSignature, AUTTriggerVolume*, Volume, AActor*, Actor);

/**
 * Generic glue trigger: per-actor enter/exit events, GameplayTag filtering, optional
 * one-shot mode. The room-clearing, story-moment and tutorial-case workhorse - wire
 * it to anything via game or Blueprint code.
 *
 * Events are actor-level: an actor with a capsule, a weapon mesh and four skeletal
 * bones overlapping fires OnEntered once and OnExited once. Component overlaps are
 * counted internally so multi-body actors cannot double-fire or emit a spurious exit.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Trigger")
	FUTTriggerEnteredSignature OnEntered;

	UPROPERTY(BlueprintAssignable, Category = "UT|Trigger")
	FUTTriggerExitedSignature OnExited;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Trigger")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Empty = any actor; otherwise the actor must own these tags (any/all per RequireAllTags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Trigger")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Trigger")
	bool bRequireAllTags = false;

	/** Fire OnEntered only the first time each actor enters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Trigger")
	bool bTriggerOncePerActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Trigger")
	bool bEnabled = true;

	AUTTriggerVolume();

protected:
	virtual void BeginPlay() override;

private:
	/**
	 * Accepted overlapping components per actor. An actor with N components inside has one
	 * entry with value N: the set alone cannot tell "still inside" from "one of three parts
	 * left", which previously released the actor while the rest was still in the volume.
	 */
	TMap<TWeakObjectPtr<AActor>, int32> OverlappingComponentCounts;
	TSet<TWeakObjectPtr<AActor>> TriggeredOnce;

	bool IsValidActivator(AActor* Actor) const;

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
						  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
