// UT Templates - collectible component.
#pragma once

#include "Components/ActorComponent.h"
#include "Components/SphereComponent.h"
#include "GameplayTagContainer.h"
#include "UTCollectibleComponent.generated.h"

class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTCollectedSignature, AActor*, Collector);

/**
 * Pickup trigger for any actor: its own sphere overlap fires OnCollected and, by
 * default, destroys the owner.
 *
 * Composability rule: this component never grants items or currency itself. Game code
 * (or a tiny bridge component) subscribes to OnCollected and calls UTInventory/UTWallet -
 * the mechanic stays independent of every module.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTWORLDMECHANICSRUNTIME_API UUTCollectibleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Collectible")
	FUTCollectedSignature OnCollected;

	/** Trigger sphere created by this component and attached to the owner's root. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Collectible")
	TObjectPtr<USphereComponent> CollectTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Collectible", meta = (ClampMin = "1.0"))
	float CollectRadius = 60.f;

	/** Empty = any actor; otherwise the collector must own these tags (any/all per RequireAllTags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Collectible")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Collectible")
	bool bRequireAllTags = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Collectible")
	bool bDestroyOnCollect = true;

	UUTCollectibleComponent();

	/** Gate for subclasses/tests; default checks RequiredTags. */
	UFUNCTION(BlueprintNativeEvent, Category = "UT|Collectible")
	bool CanCollect(AActor* Collector) const;
	virtual bool CanCollect_Implementation(AActor* Collector) const;

	/** Collect from code (interact bridges): runs the same pipeline as an overlap. */
	UFUNCTION(BlueprintCallable, Category = "UT|Collectible")
	bool Collect(AActor* Collector);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
