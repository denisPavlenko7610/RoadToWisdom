#include "UTItemCost.h"
#include "UTInventoryComponent.h"
#include "GameFramework/Actor.h"
bool UUTItemCost::CanPay_Implementation(AActor* User) const
{
	const auto* Inventory = IsValid(User) ? User->FindComponentByClass<UUTInventoryComponent>() : nullptr;
	return Inventory && Count > 0 && !ItemId.IsNone() && Inventory->HasItems(ItemId, Count);
}
bool UUTItemCost::Pay_Implementation(AActor* User)
{
	auto* Inventory = IsValid(User) && User->HasAuthority() ? User->FindComponentByClass<UUTInventoryComponent>() : nullptr;
	return Inventory && Count > 0 && !ItemId.IsNone() && Inventory->ConsumeItems(ItemId, Count);
}
