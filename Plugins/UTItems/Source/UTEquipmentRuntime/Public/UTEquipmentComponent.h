// UT Templates - actor-component equipment.
#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UTResult.h"
#include "UTItemInstance.h"
#include "UTEquipmentComponent.generated.h"

/** One equipment slot definition. Empty AcceptedTags accepts any known item. */
USTRUCT(BlueprintType)
struct UTEQUIPMENTRUNTIME_API FUTEquipmentSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName SlotId;

	/** The item's ItemTags must contain any of these (all of them when bRequireAllTags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTagContainer AcceptedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bRequireAllTags = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTEquippedSignature, FName, SlotId, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTUnequippedSignature, FName, SlotId, FName, ItemId);

/** One replicated loadout entry (TMap cannot be replicated directly). */
USTRUCT(BlueprintType)
struct UTEQUIPMENTRUNTIME_API FUTEquipmentSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT")
	FName SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT")
	FName ItemId;
	/** Valid only for unique equipment. Runtime item state remains in the owning inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT")
	FGuid InstanceId;
};
USTRUCT(BlueprintType)
struct UTEQUIPMENTRUNTIME_API FUTEquipmentState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "UT")
	TArray<FUTEquipmentSlotEntry> Entries;
};

/**
 * Named equipment slots with compatibility rules. Item identities are FNames; item
 * tags come from UUTItemDefinition data assets resolved through UUTItemRegistrySubsystem
 * (the item domain's single lookup point).
 *
 * This module records the loadout and validates compatibility; it does not move items
 * between containers or apply stat modifiers - both are composition concerns of game
 * code (hook OnEquipped/OnUnequipped). It depends on UTItemsRuntime, never on inventory.
 *
 * Multiplayer: server-authoritative. Equip/Unequip run on the authority only; the
 * loadout is replicated (late joiners see current equipment), and clients observe
 * OnEquipped/OnUnequipped through OnRep diffs.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTEQUIPMENTRUNTIME_API UUTEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Equipment")
	FUTEquippedSignature OnEquipped;

	UPROPERTY(BlueprintAssignable, Category = "UT|Equipment")
	FUTUnequippedSignature OnUnequipped;

	/** Slot layout; SlotId must be unique. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	TArray<FUTEquipmentSlot> Slots;

	UUTEquipmentComponent();

	/** Validate and record the loadout. Does not remove the item from any inventory. */
	UFUNCTION(BlueprintCallable, Category = "UT|Equipment")
	FUTResult Equip(FName SlotId, FName ItemId);
	/** Record an exact unique identity. Caller validates ownership; equipment never depends on inventory. */
	UFUNCTION(BlueprintCallable, Category = "UT|Equipment")
	FUTResult EquipInstance(FName SlotId, const FUTItemInstance& Instance);
	UFUNCTION(BlueprintPure, Category = "UT|Equipment")
	FGuid GetEquippedInstanceId(FName SlotId) const;
	/** Full loadout snapshot preserving unique identities. The legacy map snapshot is definition-only. */
	UFUNCTION(BlueprintPure, Category = "UT|Equipment")
	FUTEquipmentState MakeEquipmentSnapshot() const;
	UFUNCTION(BlueprintCallable, Category = "UT|Equipment")
	FUTResult RestoreEquipmentSnapshot(const FUTEquipmentState& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "UT|Equipment")
	FUTResult Unequip(FName SlotId);

	UFUNCTION(BlueprintPure, Category = "UT|Equipment")
	FName GetEquippedItem(FName SlotId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Equipment")
	bool IsSlotFree(FName SlotId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Equipment")
	TArray<FName> GetSlotIds() const;

	UFUNCTION(BlueprintCallable, Category = "UT|Equipment")
	void ClearEquipment();

	UFUNCTION(BlueprintPure, Category = "UT|Equipment")
	TMap<FName, FName> MakeSnapshot() const;

	/** Restore raw loadout without publishing events (save-load path). */
	UFUNCTION(BlueprintCallable, Category = "UT|Equipment")
	void RestoreFromSnapshot(const TMap<FName, FName>& Snapshot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** One replicated loadout entry. */
	UFUNCTION()
	void OnRep_EquippedEntries();

	UPROPERTY(ReplicatedUsing = OnRep_EquippedEntries, VisibleAnywhere, Category = "UT")
	TArray<FUTEquipmentSlotEntry> EquippedEntries;

	/** Authority-side mirror for quick lookup; rebuilt from entries on clients. */
	UPROPERTY(Transient)
	TMap<FName, FName> EquippedLookup;
	TMap<FName, FGuid> InstanceLookup;

	const FUTEquipmentSlot* FindSlot(FName SlotId) const;
	void RecordEquip(FName SlotId, FName ItemId, FGuid InstanceId);
	FUTResult ValidateItemForSlot(FName SlotId, FName ItemId) const;
	FUTResult EquipInternal(FName SlotId, FName ItemId, FGuid InstanceId);
};
