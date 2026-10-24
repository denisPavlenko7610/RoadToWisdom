// UT Templates - pressure plate actor implementation.
#include "UTPressurePlate.h"

#include "Components/BoxComponent.h"
#include "GameplayTagAssetInterface.h"

AUTPressurePlate::AUTPressurePlate()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(60.f, 60.f, 20.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AUTPressurePlate::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AUTPressurePlate::HandleOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AUTPressurePlate::HandleOverlapEnd);
}

bool AUTPressurePlate::IsValidActivator(AActor* Actor) const
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

bool AUTPressurePlate::IsPressed() const
{
	// Weak entries can outlive a destroyed actor that never reported EndOverlap; treat
	// them as gone so the plate does not read pressed forever.
	for (const TPair<TWeakObjectPtr<AActor>, int32>& Pair : ActivatorComponentCounts) {
		if (Pair.Key.IsValid() && Pair.Value > 0) {
			return true;
		}
	}
	return false;
}

void AUTPressurePlate::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
										  UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!IsValidActivator(OtherActor)) {
		return;
	}

	const bool bWasPressed = IsPressed();

	int32& Count = ActivatorComponentCounts.FindOrAdd(OtherActor);
	++Count;
	if (Count > 1) {
		// Another component of this activator entered; the plate state cannot change.
		return;
	}

	FUTPressurePlateActivatorSignature EnteredEvent = OnActivatorEntered;
	EnteredEvent.Broadcast(OtherActor);

	if (!bWasPressed) {
		FUTPressurePlatePressedSignature PressedEvent = OnPlatePressed;
		PressedEvent.Broadcast(OtherActor);
	}
}

void AUTPressurePlate::HandleOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
										UPrimitiveComponent*, int32)
{
	if (!OtherActor) {
		return;
	}

	int32* Count = ActivatorComponentCounts.Find(OtherActor);
	if (!Count || *Count <= 0) {
		// No tracked overlap to close. Do not re-check IsValidActivator: the actor's tags
		// may have changed while it stood here, and the exit must pair with the enter.
		return;
	}
	if (--(*Count) > 0) {
		return;
	}
	ActivatorComponentCounts.Remove(OtherActor);

	const bool bStillPressed = IsPressed();

	FUTPressurePlateActivatorSignature ExitedEvent = OnActivatorExited;
	ExitedEvent.Broadcast(OtherActor);

	if (!bStillPressed) {
		FUTPressurePlateReleasedSignature ReleasedEvent = OnPlateReleased;
		ReleasedEvent.Broadcast(OtherActor);
	}
}
