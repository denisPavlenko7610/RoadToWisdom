#include "UTLootContainer.h"
#include "UTLootTable.h"
#include "UTConditions.h"
#include "UTInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AUTLootContainer::AUTLootContainer()
{
	bReplicates = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}
void AUTLootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTLootContainer, State);
}
bool AUTLootContainer::CanUse(AActor* User) const
{
	FUTConditionContext Context;
	Context.Subject = User;
	Context.Instigator = User;
	return (!UseCondition || UseCondition->Evaluate(Context)) && !bTransacting && !IsActorBeingDestroyed() && IsValid(User) && User->GetWorld() == GetWorld() && FVector::DistSquared(User->GetActorLocation(), GetActorLocation()) <= FMath::Square(FMath::Max(0.f, UseDistance));
}
bool AUTLootContainer::Open(AActor* User)
{
	if (!HasAuthority() || !CanUse(User)) {
		return false;
	}
	if (State.Phase != EUTLootContainerPhase::Closed) {
		return true;
	}
	TGuardValue<bool> Guard(bTransacting, true);
	if (LootTable) {
		for (const FUTLootItem& Item : LootTable->RollWithSeed(Seed, Rolls).Items) {
			FUTItemPayload Payload;
			Payload.Stack.ItemId = Item.ItemId;
			Payload.Stack.Count = Item.Count;
			State.Contents.Add(Payload);
		}
	}
	else {
		TSet<FGuid> Ids;
		for (FUTItemPayload Payload : InitialContents) {
			if (Payload.bUnique && !Payload.Instance.InstanceId.IsValid()) {
				Payload.Instance.InstanceId = FGuid::NewGuid();
			}
			if (!Payload.IsValid() || (Payload.bUnique && Ids.Contains(Payload.Instance.InstanceId))) {
				continue;
			}
			if (Payload.bUnique) {
				Ids.Add(Payload.Instance.InstanceId);
				Payload.Stack = FUTItemStack();
			}
			else {
				Payload.Instance = FUTItemInstance();
			}
			State.Contents.Add(Payload);
		}
	}
	State.Phase = State.Contents.IsEmpty() ? EUTLootContainerPhase::Looted : EUTLootContainerPhase::Opened;
	ForceNetUpdate();
	OnRep_State();
	return true;
}
int32 AUTLootContainer::TakeItem(AActor* User, int32 Index, int32 Count)
{
	if (!HasAuthority() || !CanUse(User) || State.Phase != EUTLootContainerPhase::Opened || !State.Contents.IsValidIndex(Index) || Count <= 0) {
		return 0;
	}
	UUTInventoryComponent* Inventory = User->FindComponentByClass<UUTInventoryComponent>();
	if (!Inventory) {
		return 0;
	}
	TGuardValue<bool> Guard(bTransacting, true);
	FUTItemPayload& Payload = State.Contents[Index];
	const int32 Added = Payload.bUnique ? (Inventory->AddInstance(Payload.Instance) ? 1 : 0)
										: Inventory->AddItem(Payload.Stack.ItemId, FMath::Min(Count, Payload.Stack.Count));
	if (Added <= 0) {
		return 0;
	}
	if (Payload.bUnique || Payload.Stack.Count == Added) {
		State.Contents.RemoveAt(Index);
	}
	else {
		Payload.Stack.Count -= Added;
	}
	if (State.Contents.IsEmpty()) {
		State.Phase = EUTLootContainerPhase::Looted;
	}
	ForceNetUpdate();
	OnRep_State();
	return Added;
}
int32 AUTLootContainer::TakeAll(AActor* User)
{
	if (!HasAuthority() || !CanUse(User) || State.Phase != EUTLootContainerPhase::Opened) {
		return 0;
	}
	int64 Total = 0;
	for (int32 Index = State.Contents.Num() - 1; Index >= 0; --Index) {
		Total += TakeItem(User, Index, MAX_int32);
	}
	return int32(FMath::Min<int64>(Total, MAX_int32));
}
bool AUTLootContainer::CanInteract_Implementation(const FUTInteractionInfo& Info) const
{
	return CanUse(Info.Interactor.Get()) && State.Phase != EUTLootContainerPhase::Looted;
}
void AUTLootContainer::Interact_Implementation(const FUTInteractionInfo& Info)
{
	if (State.Phase == EUTLootContainerPhase::Closed) {
		Open(Info.Interactor.Get());
	}
	else {
		TakeAll(Info.Interactor.Get());
	}
}
FText AUTLootContainer::GetInteractionPrompt_Implementation(const FUTInteractionInfo&) const
{
	return FText::FromString(State.Phase == EUTLootContainerPhase::Closed   ? TEXT("Open")
							 : State.Phase == EUTLootContainerPhase::Opened ? TEXT("Take All")
																			: TEXT("Looted"));
}
void AUTLootContainer::OnRep_State()
{
	OnContentsChanged.Broadcast();
}
FInstancedStruct AUTLootContainer::CaptureState_Implementation() const
{
	return FInstancedStruct::Make<FUTLootContainerState>(State);
}
bool AUTLootContainer::RestoreState_Implementation(const FInstancedStruct& Saved)
{
	const FUTLootContainerState* Data = Saved.GetPtr<FUTLootContainerState>();
	if (!HasAuthority() || bTransacting || !Data || uint8(Data->Phase) > uint8(EUTLootContainerPhase::Looted)) {
		return false;
	}
	if ((Data->Phase != EUTLootContainerPhase::Opened && !Data->Contents.IsEmpty()) || (Data->Phase == EUTLootContainerPhase::Opened && Data->Contents.IsEmpty())) {
		return false;
	}
	TSet<FGuid> Ids;
	for (const FUTItemPayload& Item : Data->Contents) {
		if (!Item.IsValid() || (Item.bUnique && Ids.Contains(Item.Instance.InstanceId))) {
			return false;
		}
		if (Item.bUnique) {
			Ids.Add(Item.Instance.InstanceId);
		}
	}
	State = *Data;
	ForceNetUpdate();
	return true; // Silent restore: no gameplay events and no new roll.
}
