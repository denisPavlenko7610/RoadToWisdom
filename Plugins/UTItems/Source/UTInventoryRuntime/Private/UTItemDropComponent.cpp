#include "UTItemDropComponent.h"
#include "UTInventoryComponent.h"
#include "Engine/World.h"

UUTItemDropComponent::UUTItemDropComponent()
{
	SetIsReplicatedByDefault(true);
	WorldItemClass = AUTWorldItem::StaticClass();
}
AUTWorldItem* UUTItemDropComponent::PrepareDrop()
{
	if (!WorldItemClass || !GetWorld() || !IsValid(GetOwner()) || !GetOwner()->HasAuthority()) {
		return nullptr;
	}
	const FTransform Transform(GetOwner()->GetActorRotation(), GetOwner()->GetActorTransform().TransformPosition(DropOffset));
	AUTWorldItem* Item = GetWorld()->SpawnActorDeferred<AUTWorldItem>(WorldItemClass, Transform, GetOwner(), nullptr,
																	  ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Item) {
		return nullptr;
	}
	// Content is empty throughout construction/BeginPlay: nobody can pick it up yet.
	Item->FinishSpawning(Transform);
	if (!IsValid(Item) || Item->IsActorBeingDestroyed()) {
		return nullptr;
	}
	if (Item->GetItem().IsValid()) {
		Item->Destroy();
		return nullptr;
	}
	return Item;
}
AUTWorldItem* UUTItemDropComponent::DropStack(FName ItemId, int32 Count)
{
	if (bTransacting || !GetOwner() || !GetOwner()->HasAuthority() || Count <= 0 || ItemId.IsNone()) {
		return nullptr;
	}
	TGuardValue<bool> Guard(bTransacting, true);
	UUTInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UUTInventoryComponent>();
	if (!Inventory || Inventory->bSuppressNotifications || Inventory->GetItemCount(ItemId) < Count) {
		return nullptr;
	}
	AUTWorldItem* Item = PrepareDrop();
	if (!Item) {
		return nullptr;
	}
	// Construction may run game code, so validate consumption again after preparation.
	FUTItemPayload Payload;
	Payload.Stack.ItemId = ItemId;
	Payload.Stack.Count = Count;
	{
		// Do not call gameplay observers until both sides of the transfer are committed.
		TGuardValue<bool> Notifications(Inventory->bSuppressNotifications, true);
		if (!Inventory->ConsumeItems(ItemId, Count)) {
			Item->Destroy();
			return nullptr;
		}
		if (!Item->InitializeItem(Payload)) {
			Inventory->AddItem(ItemId, Count);
			Item->Destroy();
			return nullptr;
		}
	}
	Inventory->BroadcastChange(&ItemId, false, Count);
	return Item;
}
AUTWorldItem* UUTItemDropComponent::DropInstance(FGuid InstanceId)
{
	if (bTransacting || !GetOwner() || !GetOwner()->HasAuthority()) {
		return nullptr;
	}
	TGuardValue<bool> Guard(bTransacting, true);
	UUTInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UUTInventoryComponent>();
	FUTItemInstance Instance;
	if (!Inventory || Inventory->bSuppressNotifications || !Inventory->GetInstance(InstanceId, Instance)) {
		return nullptr;
	}
	AUTWorldItem* Item = PrepareDrop();
	if (!Item) {
		return nullptr;
	}
	FUTItemPayload Payload;
	Payload.bUnique = true;
	{
		TGuardValue<bool> Notifications(Inventory->bSuppressNotifications, true);
		if (!Inventory->RemoveInstance(InstanceId, Instance)) {
			Item->Destroy();
			return nullptr;
		}
		Payload.Instance = Instance;
		if (!Item->InitializeItem(Payload)) {
			Inventory->AddInstance(Instance);
			Item->Destroy();
			return nullptr;
		}
	}
	Inventory->OnRep_InventoryChanged();
	return Item;
}
void UUTItemDropComponent::ServerDropStack_Implementation(FName ItemId, int32 Count)
{
	DropStack(ItemId, Count);
}
void UUTItemDropComponent::ServerDropInstance_Implementation(FGuid InstanceId)
{
	DropInstance(InstanceId);
}
