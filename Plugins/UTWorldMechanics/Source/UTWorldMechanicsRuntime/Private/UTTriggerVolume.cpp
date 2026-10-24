// UT Templates - trigger volume implementation.
#include "UTTriggerVolume.h"

#include "GameplayTagAssetInterface.h"

AUTTriggerVolume::AUTTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AUTTriggerVolume::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AUTTriggerVolume::HandleOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AUTTriggerVolume::HandleOverlapEnd);
}

bool AUTTriggerVolume::IsValidActivator(AActor* Actor) const
{
	if (!Actor || RequiredTags.IsEmpty()) {
		return Actor != nullptr;
	}

	FGameplayTagContainer OwnedTags;
	if (const IGameplayTagAssetInterface* TagAsset = Cast<IGameplayTagAssetInterface>(Actor)) {
		TagAsset->GetOwnedGameplayTags(OwnedTags);
	}
	return bRequireAllTags ? OwnedTags.HasAll(RequiredTags) : OwnedTags.HasAny(RequiredTags);
}

void AUTTriggerVolume::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
										  UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!bEnabled || !IsValidActivator(OtherActor)) {
		return;
	}

	int32& Count = OverlappingComponentCounts.FindOrAdd(OtherActor);
	++Count;
	if (Count > 1) {
		// Another of this actor's components entered; the actor is already inside.
		return;
	}

	if (bTriggerOncePerActor && TriggeredOnce.Contains(OtherActor)) {
		return;
	}
	TriggeredOnce.Add(OtherActor);

	FUTTriggerEnteredSignature EnteredEvent = OnEntered;
	EnteredEvent.Broadcast(this, OtherActor);
}

void AUTTriggerVolume::HandleOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
										UPrimitiveComponent*, int32)
{
	if (!OtherActor) {
		return;
	}

	int32* Count = OverlappingComponentCounts.Find(OtherActor);
	if (!Count || *Count <= 0) {
		return;
	}
	if (--(*Count) > 0) {
		// Still other components of this actor inside.
		return;
	}
	OverlappingComponentCounts.Remove(OtherActor);

	// Deliberately not re-running IsValidActivator here: tags may have been added or
	// removed while the actor was inside, and the exit must pair with the enter.
	FUTTriggerExitedSignature ExitedEvent = OnExited;
	ExitedEvent.Broadcast(this, OtherActor);
}
