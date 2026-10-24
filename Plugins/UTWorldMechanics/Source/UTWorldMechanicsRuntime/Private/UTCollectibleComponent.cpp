// UT Templates - collectible component implementation.
#include "UTCollectibleComponent.h"

#include "Components/SphereComponent.h"
#include "GameplayTagAssetInterface.h"

UUTCollectibleComponent::UUTCollectibleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	CollectTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("CollectTrigger"));
	CollectTrigger->InitSphereRadius(CollectRadius);
	CollectTrigger->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
	CollectTrigger->SetGenerateOverlapEvents(true);
}

bool UUTCollectibleComponent::CanCollect_Implementation(AActor* Collector) const
{
	if (!Collector) {
		return false;
	}
	if (RequiredTags.IsEmpty()) {
		return true;
	}

	FGameplayTagContainer OwnedTags;
	if (const IGameplayTagAssetInterface* TagAsset = Cast<IGameplayTagAssetInterface>(Collector)) {
		TagAsset->GetOwnedGameplayTags(OwnedTags);
	}
	return bRequireAllTags ? OwnedTags.HasAll(RequiredTags) : OwnedTags.HasAny(RequiredTags);
}

bool UUTCollectibleComponent::Collect(AActor* Collector)
{
	// Server-authoritative pickup: the grant (and destruction) happens on the server;
	// clients learn the object disappeared through replication.
	if (!CanCollect(Collector) || !GetOwner() || !GetOwner()->HasAuthority()) {
		return false;
	}

	FUTCollectedSignature CollectedEvent = OnCollected;
	CollectedEvent.Broadcast(Collector);

	if (bDestroyOnCollect) {
		GetOwner()->Destroy();
	}
	return true;
}

void UUTCollectibleComponent::BeginPlay()
{
	Super::BeginPlay();

	// The radius may have been edited on the component after construction.
	CollectTrigger->SetSphereRadius(CollectRadius);
	CollectTrigger->OnComponentBeginOverlap.AddDynamic(this, &UUTCollectibleComponent::HandleOverlapBegin);
}

void UUTCollectibleComponent::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
												 UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	Collect(OtherActor);
}
