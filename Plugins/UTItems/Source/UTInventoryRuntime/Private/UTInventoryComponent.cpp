// UT Templates - inventory component implementation.
#include "UTInventoryComponent.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
static TAutoConsoleVariable<int32> CVarUTDebugItems(
	TEXT("ut.Debug.Items"), 0, TEXT("Enable toolkit items diagnostics."), ECVF_Cheat);
#endif

#include "Net/UnrealNetwork.h"
#include "UTItemRegistry.h"

UUTInventoryComponent::UUTInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	ReplicatedStacks.Owner = this;
	ReplicatedInstances.Owner = this;
}

void UUTInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTInventoryComponent, ReplicatedStacks);
	DOREPLIFETIME(UUTInventoryComponent, ReplicatedInstances);
}

void FUTInventoryFastArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (Owner) {
		for (const int32 Index : RemovedIndices) {
			Owner->OnItemRemoved.Broadcast(Items[Index].Stack.ItemId, Items[Index].Stack.Count);
		}
	}
}

void FUTInventoryFastArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (Owner) {
		for (const int32 Index : AddedIndices) {
			Owner->OnItemAdded.Broadcast(Items[Index].Stack.ItemId, Items[Index].Stack.Count);
		}
	}
}

void FUTInventoryFastArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	// The aggregate notification runs after all removals/adds/changes are committed.
}

void FUTInventoryFastArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
	if (Owner) {
		Owner->OnRep_InventoryChanged();
	}
}

void UUTInventoryComponent::OnRep_InventoryChanged()
{
	if (bSuppressNotifications) {
		return;
	}
#if !UE_BUILD_SHIPPING
	if (CVarUTDebugItems.GetValueOnGameThread() != 0) {
		UE_LOG(
			LogTemp, Display, TEXT("UTItems inventory=%s owner=%s role=%d stacks=%d instances=%d"),
			*GetPathName(), *GetNameSafe(GetOwner()), GetOwner() ? int32(GetOwner()->GetLocalRole()) : -1,
			ReplicatedStacks.Items.Num(), ReplicatedInstances.Items.Num());
		for (const auto& Entry : ReplicatedInstances.Items) {
			UE_LOG(
				LogTemp, Display, TEXT("UTItems instance=%s definition=%s"), *Entry.Instance.InstanceId.ToString(),
				*Entry.Instance.ItemId.ToString());
		}
	}
#endif
	FUTInventoryChangedSignature ChangedEvent = OnInventoryChanged;
	ChangedEvent.Broadcast();
}

void UUTInventoryComponent::BroadcastChange(const FName* ItemId, bool bAdded, int32 Count)
{
	if (bSuppressNotifications) {
		return;
	}
	if (ItemId) {
		if (bAdded) {
			FUTItemAddedSignature AddedEvent = OnItemAdded;
			AddedEvent.Broadcast(*ItemId, Count);
		}
		else {
			FUTItemRemovedSignature RemovedEvent = OnItemRemoved;
			RemovedEvent.Broadcast(*ItemId, Count);
		}
	}
#if !UE_BUILD_SHIPPING
	if (CVarUTDebugItems.GetValueOnGameThread() != 0) {
		UE_LOG(
			LogTemp, Display, TEXT("UTItems inventory=%s owner=%s role=%d stacks=%d instances=%d"),
			*GetPathName(), *GetNameSafe(GetOwner()), GetOwner() ? int32(GetOwner()->GetLocalRole()) : -1,
			ReplicatedStacks.Items.Num(), ReplicatedInstances.Items.Num());
		for (const auto& Entry : ReplicatedInstances.Items) {
			UE_LOG(
				LogTemp, Display, TEXT("UTItems instance=%s definition=%s"), *Entry.Instance.InstanceId.ToString(),
				*Entry.Instance.ItemId.ToString());
		}
	}
#endif
	FUTInventoryChangedSignature ChangedEvent = OnInventoryChanged;
	ChangedEvent.Broadcast();
}

TArray<FUTItemStack> UUTInventoryComponent::GetStacksCopy() const
{
	TArray<FUTItemStack> Stacks;
	Stacks.Reserve(ReplicatedStacks.Items.Num());
	for (const FUTInventoryStackItem& Item : ReplicatedStacks.Items) {
		Stacks.Add(Item.Stack);
	}
	return Stacks;
}

int32 UUTInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0) {
		return 0;
	}

	// Server-authoritative: a client asking to add items is ignored.
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return 0;
	}

	const int32 MaxStack = GetMaxStackSize(ItemId);
	int32 Remaining = Count;
	int32 Added = 0;

	if (MaxStack > 1) {
		for (FUTInventoryStackItem& Item : ReplicatedStacks.Items) {
			if (Remaining <= 0) {
				break;
			}
			if (Item.Stack.ItemId == ItemId && Item.Stack.Count < MaxStack) {
				const int32 Take = FMath::Min(MaxStack - Item.Stack.Count, Remaining);
				Item.Stack.Count += Take;
				Remaining -= Take;
				Added += Take;
				ReplicatedStacks.MarkItemDirty(Item);
			}
		}
	}

	while (Remaining > 0 && CanOpenSlot()) {
		const int32 Take = FMath::Min(MaxStack, Remaining);
		FUTInventoryStackItem& Item = ReplicatedStacks.Items.AddDefaulted_GetRef();
		Item.Stack.ItemId = ItemId;
		Item.Stack.Count = Take;
		Remaining -= Take;
		Added += Take;
		ReplicatedStacks.MarkItemDirty(Item);
	}

	if (Added > 0) {
		BroadcastChange(&ItemId, /*bAdded*/ true, Added);
	}
	return Added;
}

int32 UUTInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0) {
		return 0;
	}

	// Server-authoritative: a client asking to remove items is ignored.
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return 0;
	}

	int32 Remaining = Count;
	int32 Removed = 0;
	for (int32 Index = ReplicatedStacks.Items.Num() - 1; Index >= 0 && Remaining > 0; --Index) {
		FUTInventoryStackItem& Item = ReplicatedStacks.Items[Index];
		if (Item.Stack.ItemId != ItemId) {
			continue;
		}
		const int32 Take = FMath::Min(Item.Stack.Count, Remaining);
		Item.Stack.Count -= Take;
		Remaining -= Take;
		Removed += Take;
		if (Item.Stack.Count <= 0) {
			ReplicatedStacks.Items.RemoveAtSwap(Index);
			ReplicatedStacks.MarkArrayDirty();
		}
		else {
			ReplicatedStacks.MarkItemDirty(Item);
		}
	}

	if (Removed > 0) {
		BroadcastChange(&ItemId, /*bAdded*/ false, Removed);
	}
	return Removed;
}

bool UUTInventoryComponent::ConsumeItems(FName ItemId, int32 Count)
{
	if ((GetOwner() && !GetOwner()->HasAuthority()) || ItemId.IsNone() || Count <= 0 || !HasItems(ItemId, Count)) {
		return false;
	}
	return RemoveItem(ItemId, Count) == Count;
}

int32 UUTInventoryComponent::GetItemCount(FName ItemId) const
{
	int32 Total = 0;
	for (const FUTInventoryStackItem& Item : ReplicatedStacks.Items) {
		if (Item.Stack.ItemId == ItemId) {
			Total += Item.Stack.Count;
		}
	}
	return Total;
}

bool UUTInventoryComponent::HasItems(FName ItemId, int32 Count) const
{
	return Count <= 0 || GetItemCount(ItemId) >= Count;
}

int32 UUTInventoryComponent::GetMaxStackSize(FName ItemId)
{
	const UUTItemRegistrySubsystem* Registry = UUTItemRegistrySubsystem::Get(this);
	if (!Registry || !Registry->IsItemKnown(ItemId)) {
		return FMath::Max(1, DefaultMaxStackSize);
	}
	return Registry->GetMaxStackSize(ItemId);
}

void UUTInventoryComponent::ClearInventory()
{
	if ((GetOwner() && !GetOwner()->HasAuthority()) || (ReplicatedStacks.Items.IsEmpty() && ReplicatedInstances.Items.IsEmpty())) {
		return;
	}
	ReplicatedStacks.Items.Reset();
	ReplicatedStacks.MarkArrayDirty();
	ReplicatedInstances.Items.Reset();
	ReplicatedInstances.MarkArrayDirty();
#if !UE_BUILD_SHIPPING
	if (CVarUTDebugItems.GetValueOnGameThread() != 0) {
		UE_LOG(
			LogTemp, Display, TEXT("UTItems inventory=%s owner=%s role=%d stacks=%d instances=%d"),
			*GetPathName(), *GetNameSafe(GetOwner()), GetOwner() ? int32(GetOwner()->GetLocalRole()) : -1,
			ReplicatedStacks.Items.Num(), ReplicatedInstances.Items.Num());
		for (const auto& Entry : ReplicatedInstances.Items) {
			UE_LOG(
				LogTemp, Display, TEXT("UTItems instance=%s definition=%s"), *Entry.Instance.InstanceId.ToString(),
				*Entry.Instance.ItemId.ToString());
		}
	}
#endif
	FUTInventoryChangedSignature ChangedEvent = OnInventoryChanged;
	ChangedEvent.Broadcast();
}

FUTInventorySnapshot UUTInventoryComponent::MakeSnapshot() const
{
	FUTInventorySnapshot Snapshot;
	Snapshot.Stacks = GetStacksCopy();
	for (const FUTInventoryInstanceEntry& Entry : ReplicatedInstances.Items) {
		Snapshot.Instances.Add(Entry.Instance);
	}
	return Snapshot;
}

void UUTInventoryComponent::RestoreFromSnapshot(const FUTInventorySnapshot& Snapshot)
{
	// Server-only: restored state reaches clients through replication.
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	TSet<FGuid> Ids;
	for (const FUTItemInstance& Instance : Snapshot.Instances) {
		if (!Instance.IsValid() || Ids.Contains(Instance.InstanceId)) {
			return;
		}
		Ids.Add(Instance.InstanceId);
	}
	ReplicatedInstances.Items.Reset();
	for (const FUTItemInstance& Instance : Snapshot.Instances) {
		FUTInventoryInstanceEntry& Entry = ReplicatedInstances.Items.AddDefaulted_GetRef();
		Entry.Instance = Instance;
		ReplicatedInstances.MarkItemDirty(Entry);
	}
	ReplicatedInstances.MarkArrayDirty();
	ReplicatedStacks.Items.Reset();
	for (const FUTItemStack& Stack : Snapshot.Stacks) {
		if (!Stack.ItemId.IsNone() && Stack.Count > 0) {
			FUTInventoryStackItem& Item = ReplicatedStacks.Items.AddDefaulted_GetRef();
			Item.Stack = Stack;
		}
	}
	ReplicatedStacks.MarkArrayDirty();
	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.
}

bool UUTInventoryComponent::CanOpenSlot() const
{
	return MaxSlots <= 0 || GetUsedSlotCount() < MaxSlots;
}

void FUTInventoryInstanceArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
	if (Owner) {
		Owner->OnRep_InventoryChanged();
	}
}

bool UUTInventoryComponent::GetInstance(FGuid InstanceId, FUTItemInstance& OutInstance) const
{
	OutInstance = FUTItemInstance();
	for (const FUTInventoryInstanceEntry& Entry : ReplicatedInstances.Items) {
		if (Entry.Instance.InstanceId == InstanceId) {
			OutInstance = Entry.Instance;
			return true;
		}
	}
	return false;
}

bool UUTInventoryComponent::AddInstance(const FUTItemInstance& Instance)
{
	FUTItemInstance Existing;
	if ((GetOwner() && !GetOwner()->HasAuthority()) || !Instance.IsValid() || !CanOpenSlot() || GetInstance(Instance.InstanceId, Existing)) {
		return false;
	}
	FUTInventoryInstanceEntry& Entry = ReplicatedInstances.Items.AddDefaulted_GetRef();
	Entry.Instance = Instance;
	ReplicatedInstances.MarkItemDirty(Entry);
	OnRep_InventoryChanged();
	return true;
}

bool UUTInventoryComponent::RemoveInstance(FGuid InstanceId, FUTItemInstance& OutInstance)
{
	OutInstance = FUTItemInstance();
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return false;
	}
	const int32 Index = ReplicatedInstances.Items.IndexOfByPredicate(
		[InstanceId](const FUTInventoryInstanceEntry& Entry) {
			return Entry.Instance.InstanceId == InstanceId;
		});
	if (Index == INDEX_NONE) {
		return false;
	}
	OutInstance = ReplicatedInstances.Items[Index].Instance;
	ReplicatedInstances.Items.RemoveAtSwap(Index);
	ReplicatedInstances.MarkArrayDirty();
	OnRep_InventoryChanged();
	return true;
}

bool UUTInventoryComponent::SetInstanceState(FGuid InstanceId, const FUTItemInstanceState& State)
{
	if ((GetOwner() && !GetOwner()->HasAuthority()) || !State.IsValid()) {
		return false;
	}
	for (FUTInventoryInstanceEntry& Entry : ReplicatedInstances.Items) {
		if (Entry.Instance.InstanceId == InstanceId) {
			Entry.Instance.State = State;
			ReplicatedInstances.MarkItemDirty(Entry);
			OnRep_InventoryChanged();
			return true;
		}
	}
	return false;
}
