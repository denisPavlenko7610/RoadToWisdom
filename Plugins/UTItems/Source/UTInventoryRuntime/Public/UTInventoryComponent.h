// UT Templates - actor-component inventory.
#pragma once

#include "Components/ActorComponent.h"
#include "UTInventoryTypes.h"
#include "UTInventoryComponent.generated.h"

/**
 * Slot-based inventory with stacking. Item stacks are identified by FName; stacking
 * rules come from item definitions resolved through UUTItemRegistrySubsystem - the
 * inventory does not own definitions and never references equipment.
 *
 * Composability rule: grant-to-wallet, equip-on-pickup and pickup spawning are
 * composition concerns of game code.
 *
 * Multiplayer: server-authoritative. All mutations run on the authority; stacks travel
 * through a Fast Array Serializer, so clients receive add/remove/change deltas (driving
 * OnItemAdded / OnItemRemoved / OnInventoryChanged via OnRep) instead of the full list,
 * and late joiners get the current state automatically. A client cannot add, remove or
 * consume items - the component ignores mutations without server authority.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTINVENTORYRUNTIME_API UUTInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTItemAddedSignature, FName, ItemId, int32, Count);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTItemRemovedSignature, FName, ItemId, int32, Count);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTInventoryChangedSignature);

	UPROPERTY(BlueprintAssignable, Category = "UT|Inventory")
	FUTItemAddedSignature OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "UT|Inventory")
	FUTItemRemovedSignature OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "UT|Inventory")
	FUTInventoryChangedSignature OnInventoryChanged;

	/** Maximum distinct stacks; 0 means unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "0"))
	int32 MaxSlots = 0;

	/** Stack size used when the item has no definition in the registry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "1"))
	int32 DefaultMaxStackSize = 99;

	/** Add up to Count items (fill existing stacks, then open slots). Returns the number actually added. */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	int32 AddItem(FName ItemId, int32 Count);

	/** Remove up to Count items. Returns the number actually removed. */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	int32 RemoveItem(FName ItemId, int32 Count);

	/** All-or-nothing consumption: false leaves the inventory untouched. */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	bool ConsumeItems(FName ItemId, int32 Count);

	UFUNCTION(BlueprintPure, Category = "UT|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Inventory", Meta = (DisplayName = "Has Items"))
	bool HasItems(FName ItemId, int32 Count = 1) const;

	UFUNCTION(BlueprintPure, Category = "UT|Inventory")
	int32 GetUsedSlotCount() const
	{
		return ReplicatedStacks.Items.Num() + ReplicatedInstances.Items.Num();
	}

	/** Server only. Adds one unique identity, refusing duplicates, invalid state or full slots. */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	bool AddInstance(const FUTItemInstance& Instance);

	/** Server only. Removes exactly this instance; never consumes a unique item by definition id. */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	bool RemoveInstance(FGuid InstanceId, FUTItemInstance& OutInstance);

	/** Read-only copy, valid on authority and replicated clients. */
	UFUNCTION(BlueprintPure, Category = "UT|Inventory")
	bool GetInstance(FGuid InstanceId, FUTItemInstance& OutInstance) const;

	/** Server only. Changes state while preserving identity and definition. */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	bool SetInstanceState(FGuid InstanceId, const FUTItemInstanceState& State);

	UUTInventoryComponent();

	/** Invoked by the fast array delta callbacks on clients (and directly on the server). */
	void OnRep_InventoryChanged();

	/** Registry-backed stack limit; DefaultMaxStackSize for items the registry does not know. */
	UFUNCTION(BlueprintPure, Category = "UT|Inventory")
	int32 GetMaxStackSize(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	void ClearInventory();

	UFUNCTION(BlueprintPure, Category = "UT|Inventory")
	FUTInventorySnapshot MakeSnapshot() const;

	/** Restore raw stacks without publishing events (save-load path). */
	UFUNCTION(BlueprintCallable, Category = "UT|Inventory")
	void RestoreFromSnapshot(const FUTInventorySnapshot& Snapshot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class FUTInventoryNotificationTest;
	friend class UUTItemDropComponent;
	bool bSuppressNotifications = false;
	UPROPERTY(Replicated)
	FUTInventoryInstanceArray ReplicatedInstances;
	/** Replicated stacks. Mutated only on the authority; clients receive deltas. */
	UPROPERTY(Replicated)
	FUTInventoryFastArray ReplicatedStacks;

	/** Server-side: announce a committed mutation. */
	void BroadcastChange(const FName* ItemId, bool bAdded, int32 Count);

	TArray<FUTItemStack> GetStacksCopy() const;

	bool CanOpenSlot() const;
};
