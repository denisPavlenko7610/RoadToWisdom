// UT Templates - equipment component implementation.
#include "UTEquipmentComponent.h"

#include "Net/UnrealNetwork.h"
#include "UTItemRegistry.h"

namespace
{
	// Inline map entries reserved while broadcasting clear events before falling back to heap.
	constexpr int32 ClearSlotInlineCapacity = 8;
}

UUTEquipmentComponent::UUTEquipmentComponent()
{
	SetIsReplicatedByDefault(true);
}

void UUTEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTEquipmentComponent, EquippedEntries);
}

void UUTEquipmentComponent::OnRep_EquippedEntries()
{
	const auto PreviousItems = EquippedLookup;
	const auto PreviousInstances = InstanceLookup;
	EquippedLookup.Reset();
	InstanceLookup.Reset();
	for (const auto& Entry : EquippedEntries) {
		EquippedLookup.Add(Entry.SlotId, Entry.ItemId);
		InstanceLookup.Add(Entry.SlotId, Entry.InstanceId);
	}
	// Commit caches before callbacks so UI can query the new state inside its notification.
	for (const auto& Previous : PreviousItems) {
		if (GetEquippedItem(Previous.Key) != Previous.Value || GetEquippedInstanceId(Previous.Key) != PreviousInstances.FindRef(Previous.Key)) {
			FUTUnequippedSignature Event = OnUnequipped;
			Event.Broadcast(Previous.Key, Previous.Value);
		}
	}
	for (const auto& Entry : EquippedEntries) {
		if (PreviousItems.FindRef(Entry.SlotId) != Entry.ItemId || PreviousInstances.FindRef(Entry.SlotId) != Entry.InstanceId) {
			FUTEquippedSignature Event = OnEquipped;
			Event.Broadcast(Entry.SlotId, Entry.ItemId);
		}
	}
}

void UUTEquipmentComponent::RecordEquip(FName SlotId, FName ItemId, FGuid InstanceId)
{
	bool bFound = false;
	for (FUTEquipmentSlotEntry& Entry : EquippedEntries) {
		if (Entry.SlotId == SlotId) {
			Entry.ItemId = ItemId;
			Entry.InstanceId = InstanceId;
			bFound = true;
			break;
		}
	}
	if (!bFound) {
		FUTEquipmentSlotEntry Entry;
		Entry.SlotId = SlotId;
		Entry.ItemId = ItemId;
		Entry.InstanceId = InstanceId;
		EquippedEntries.Add(Entry);
	}
	EquippedLookup.Add(SlotId, ItemId);
	InstanceLookup.Add(SlotId, InstanceId);
}

FUTResult UUTEquipmentComponent::ValidateItemForSlot(FName SlotId, FName ItemId) const
{
	const FUTEquipmentSlot* Slot = FindSlot(SlotId);
	if (!Slot) {
		return FUTResult::Failure(TEXT("SlotNotFound"),
								  FText::FromString(FString::Printf(TEXT("Slot '%s' is not configured."), *SlotId.ToString())));
	}
	if (ItemId.IsNone()) {
		return FUTResult::Failure(TEXT("InvalidItem"), FText::FromString(TEXT("ItemId must not be None.")));
	}

	UUTItemRegistrySubsystem* Registry = UUTItemRegistrySubsystem::Get(this);
	const UUTItemDefinition* LoadedDefinition = Registry ? Registry->GetItemDefinition(ItemId) : nullptr;
	if (!LoadedDefinition) {
		return FUTResult::Failure(TEXT("ItemNotFound"),
								  FText::FromString(FString::Printf(TEXT("No item definition registered for '%s'."), *ItemId.ToString())));
	}

	if (!Slot->AcceptedTags.IsEmpty() && !(Slot->bRequireAllTags
											   ? LoadedDefinition->ItemTags.HasAll(Slot->AcceptedTags)
											   : LoadedDefinition->ItemTags.HasAny(Slot->AcceptedTags))) {
		return FUTResult::Failure(TEXT("ItemNotCompatible"),
								  FText::FromString(FString::Printf(TEXT("Item '%s' does not fit slot '%s'."), *ItemId.ToString(), *SlotId.ToString())));
	}

	return FUTResult::Success();
}
FUTResult UUTEquipmentComponent::Equip(FName SlotId, FName ItemId)
{
	return EquipInternal(SlotId, ItemId, FGuid());
}
FUTResult UUTEquipmentComponent::EquipInstance(FName SlotId, const FUTItemInstance& Instance)
{
	if (!Instance.IsValid()) {
		return FUTResult::Failure(TEXT("InvalidInstance"), FText::FromString(TEXT("Equipment requires a valid item identity/state.")));
	}
	return EquipInternal(SlotId, Instance.ItemId, Instance.InstanceId);
}
FUTResult UUTEquipmentComponent::EquipInternal(FName SlotId, FName ItemId, FGuid InstanceId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("Equip must run on the server.")));
	}
	const FUTResult Validation = ValidateItemForSlot(SlotId, ItemId);
	if (!Validation.IsSuccess()) {
		return Validation;
	}
	if (InstanceId.IsValid()) {
		for (const auto& Entry : EquippedEntries) {
			if (Entry.SlotId != SlotId && Entry.InstanceId == InstanceId) {
				return FUTResult::Failure(TEXT("InstanceAlreadyEquipped"), FText::FromString(TEXT("A unique item cannot occupy two slots.")));
			}
		}
	}
	const FName Previous = GetEquippedItem(SlotId);
	if (Previous == ItemId && GetEquippedInstanceId(SlotId) == InstanceId) {
		return FUTResult::Success();
	}
	RecordEquip(SlotId, ItemId, InstanceId);
	if (!Previous.IsNone()) {
		FUTUnequippedSignature Event = OnUnequipped;
		Event.Broadcast(SlotId, Previous);
	}
	FUTEquippedSignature Event = OnEquipped;
	Event.Broadcast(SlotId, ItemId);
	return FUTResult::Success();
}
FGuid UUTEquipmentComponent::GetEquippedInstanceId(FName SlotId) const
{
	return InstanceLookup.FindRef(SlotId);
}
FUTEquipmentState UUTEquipmentComponent::MakeEquipmentSnapshot() const
{
	FUTEquipmentState Snapshot;
	Snapshot.Entries = EquippedEntries;
	return Snapshot;
}
FUTResult UUTEquipmentComponent::RestoreEquipmentSnapshot(const FUTEquipmentState& Snapshot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("Restore must run on the server.")));
	}
	TSet<FName> UsedSlots;
	TSet<FGuid> UsedInstances;
	for (const auto& Entry : Snapshot.Entries) {
		const FUTResult Validation = ValidateItemForSlot(Entry.SlotId, Entry.ItemId);
		if (!Validation.IsSuccess()) {
			return Validation;
		}
		if (UsedSlots.Contains(Entry.SlotId) || (Entry.InstanceId.IsValid() && UsedInstances.Contains(Entry.InstanceId))) {
			return FUTResult::Failure(TEXT("DuplicateEquipment"), FText::FromString(TEXT("Snapshot repeats a slot or unique instance.")));
		}
		UsedSlots.Add(Entry.SlotId);
		if (Entry.InstanceId.IsValid()) {
			UsedInstances.Add(Entry.InstanceId);
		}
	}
	EquippedEntries = Snapshot.Entries;
	EquippedLookup.Reset();
	InstanceLookup.Reset();
	for (const auto& Entry : EquippedEntries) {
		EquippedLookup.Add(Entry.SlotId, Entry.ItemId);
		InstanceLookup.Add(Entry.SlotId, Entry.InstanceId);
	}
	return FUTResult::Success();
}

FUTResult UUTEquipmentComponent::Unequip(FName SlotId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("Unequip must run on the server.")));
	}

	if (!FindSlot(SlotId)) {
		return FUTResult::Failure(TEXT("SlotNotFound"),
								  FText::FromString(FString::Printf(TEXT("Slot '%s' is not configured."), *SlotId.ToString())));
	}

	FName* Current = EquippedLookup.Find(SlotId);
	if (!Current || Current->IsNone()) {
		return FUTResult::Failure(TEXT("SlotEmpty"),
								  FText::FromString(FString::Printf(TEXT("Slot '%s' is already empty."), *SlotId.ToString())));
	}

	const FName Removed = *Current;
	EquippedLookup.Remove(SlotId);
	InstanceLookup.Remove(SlotId);
	for (int32 Index = 0; Index < EquippedEntries.Num(); ++Index) {
		if (EquippedEntries[Index].SlotId == SlotId) {
			EquippedEntries.RemoveAt(Index);
			break;
		}
	}

	FUTUnequippedSignature UnequippedEvent = OnUnequipped;
	UnequippedEvent.Broadcast(SlotId, Removed);
	return FUTResult::Success();
}

FName UUTEquipmentComponent::GetEquippedItem(FName SlotId) const
{
	const FName* Item = EquippedLookup.Find(SlotId);
	return Item ? *Item : FName();
}

bool UUTEquipmentComponent::IsSlotFree(FName SlotId) const
{
	return GetEquippedItem(SlotId).IsNone();
}

TArray<FName> UUTEquipmentComponent::GetSlotIds() const
{
	TArray<FName> SlotIds;
	SlotIds.Reserve(Slots.Num());
	for (const FUTEquipmentSlot& Slot : Slots) {
		SlotIds.Add(Slot.SlotId);
	}
	return SlotIds;
}

void UUTEquipmentComponent::ClearEquipment()
{
	TArray<FName, TInlineAllocator<ClearSlotInlineCapacity>> SlotIds;
	EquippedLookup.GetKeys(SlotIds);
	for (const FName& SlotId : SlotIds) {
		Unequip(SlotId);
	}
}

TMap<FName, FName> UUTEquipmentComponent::MakeSnapshot() const
{
	return EquippedLookup;
}

void UUTEquipmentComponent::RestoreFromSnapshot(const TMap<FName, FName>& Snapshot)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}

	EquippedEntries.Reset();
	InstanceLookup.Reset();
	EquippedLookup = Snapshot;
	for (const TPair<FName, FName>& Pair : Snapshot) {
		FUTEquipmentSlotEntry Entry;
		Entry.SlotId = Pair.Key;
		Entry.ItemId = Pair.Value;
		EquippedEntries.Add(Entry);
	}
	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.
}

const FUTEquipmentSlot* UUTEquipmentComponent::FindSlot(FName SlotId) const
{
	return Slots.FindByPredicate([&SlotId](const FUTEquipmentSlot& Slot) { return Slot.SlotId == SlotId; });
}
