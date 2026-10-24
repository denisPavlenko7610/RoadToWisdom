#pragma once
#include "GameFramework/Actor.h"
#include "UTInventoryTypes.h"
#include "UTInteractable.h"
#include "UTWorldItem.generated.h"
class UStaticMeshComponent;

/** Exactly one stack OR one unique instance, shared by pickups and containers. */
USTRUCT(BlueprintType)
struct UTINVENTORYRUNTIME_API FUTItemPayload
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	bool bUnique = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items", meta = (EditCondition = "!bUnique", EditConditionHides))
	FUTItemStack Stack;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items", meta = (EditCondition = "bUnique", EditConditionHides))
	FUTItemInstance Instance;
	bool IsValid() const { return bUnique ? Instance.IsValid() : !Stack.ItemId.IsNone() && Stack.Count > 0; }
	FName GetItemId() const { return bUnique ? Instance.ItemId : Stack.ItemId; }
	int32 GetCount() const { return bUnique ? (Instance.IsValid() ? 1 : 0) : Stack.Count; }
};

/** Server-owned world representation. Interaction RPCs go through the player's
 * UTInteractor, never through an unowned pickup. Only accepted quantity disappears.
 */
UCLASS(Blueprintable)
class UTINVENTORYRUNTIME_API AUTWorldItem : public AActor, public IUTInteractable
{
	GENERATED_BODY()
public:
	AUTWorldItem();
	/** Optional authored content for placed pickups. Empty leaves InitializeItem to server spawning code.
	 * Unique authored content may omit InstanceId; authority creates it once in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Items")
	FUTItemPayload InitialItem;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Items")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Items", meta = (ClampMin = "0"))
	float PickupDistance = 250.f;
	/** Server only; initial assignment, not an item minting RPC. Refuses reassignment. */
	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	bool InitializeItem(const FUTItemPayload& Item);
	UFUNCTION(BlueprintPure, Category = "UT|Items")
	FUTItemPayload GetItem() const { return Content; }
	/** Validates existence, world, distance and inventory on every call. Returns accepted quantity. */
	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	int32 TryPickup(AActor* Collector);
	virtual bool CanInteract_Implementation(const FUTInteractionInfo& Info) const override;
	virtual void Interact_Implementation(const FUTInteractionInfo& Info) override;
	virtual FText GetInteractionPrompt_Implementation(const FUTInteractionInfo&) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Replicated)
	FUTItemPayload Content;
	bool bTransacting = false;
};
