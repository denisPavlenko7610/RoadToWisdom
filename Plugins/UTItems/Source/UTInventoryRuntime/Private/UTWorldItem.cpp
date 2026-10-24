#include "UTWorldItem.h"
#include "UTInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AUTWorldItem::AUTWorldItem()
{
	bReplicates = true;
	SetReplicateMovement(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
void AUTWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTWorldItem, Content);
}
bool AUTWorldItem::InitializeItem(const FUTItemPayload& Item)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || Content.IsValid() || !Item.IsValid()) {
		return false;
	}
	Content = Item;
	// Strip unused data; a stack pickup does not carry an unrelated instance payload.
	if (Content.bUnique) {
		Content.Stack = FUTItemStack();
	}
	else {
		Content.Instance = FUTItemInstance();
	}
	ForceNetUpdate();
	return true;
}
bool AUTWorldItem::CanInteract_Implementation(const FUTInteractionInfo& Info) const
{
	return !bTransacting && !IsActorBeingDestroyed() && Content.IsValid() && Info.Interactor.IsValid() && Info.Interactor->GetWorld() == GetWorld() && FVector::DistSquared(GetActorLocation(), Info.Interactor->GetActorLocation()) <= FMath::Square(PickupDistance) && Info.Interactor->FindComponentByClass<UUTInventoryComponent>();
}
void AUTWorldItem::Interact_Implementation(const FUTInteractionInfo& Info)
{
	TryPickup(Info.Interactor.Get());
}
FText AUTWorldItem::GetInteractionPrompt_Implementation(const FUTInteractionInfo&) const
{
	return FText::FromString(FString::Printf(TEXT("Pick Up %s x%d"), *Content.GetItemId().ToString(), Content.GetCount()));
}
int32 AUTWorldItem::TryPickup(AActor* Collector)
{
	FUTInteractionInfo Info;
	Info.Interactor = Collector;
	if (!HasAuthority() || !CanInteract_Implementation(Info)) {
		return 0;
	}
	TGuardValue<bool> Guard(bTransacting, true);
	UUTInventoryComponent* Inventory = Collector->FindComponentByClass<UUTInventoryComponent>();
	const int32 Added = Content.bUnique ? (Inventory->AddInstance(Content.Instance) ? 1 : 0)
										: Inventory->AddItem(Content.Stack.ItemId, Content.Stack.Count);
	if (Added <= 0) {
		return 0;
	}
	if (Content.bUnique || Added == Content.Stack.Count) {
		Content = FUTItemPayload();
		Destroy();
	}
	else {
		Content.Stack.Count -= Added;
		ForceNetUpdate();
	}
	return Added;
}

void AUTWorldItem::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority() || Content.IsValid() || InitialItem.GetItemId().IsNone()) {
		return;
	}
	FUTItemPayload Item = InitialItem;
	if (Item.bUnique && !Item.Instance.InstanceId.IsValid()) {
		Item.Instance.InstanceId = FGuid::NewGuid();
	}
	if (!InitializeItem(Item)) {
		UE_LOG(LogTemp, Warning, TEXT("World item '%s' has invalid InitialItem configuration."), *GetPathName());
	}
}
