// UT Templates - teleporter actor implementation.
#include "UTTeleporter.h"

#include "Components/BoxComponent.h"
#include "UTTeleporterSubsystem.h"

AUTTeleporter::AUTTeleporter()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(50.f, 50.f, 60.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AUTTeleporter::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AUTTeleporter::HandleOverlapBegin);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AUTTeleporter::HandleOverlapEnd);
}

bool AUTTeleporter::TeleportActor(AActor* Actor)
{
	// Teleporting is gameplay state: overlap handlers on clients never teleport.
	if (!bEnabled || !Actor || !HasAuthority()) {
		return false;
	}

	const AActor* Destination = DestinationActor ? DestinationActor : this;
	const FTransform DestinationTransform = Destination->GetActorTransform();

	const FVector NewLocation = DestinationTransform.TransformPosition(DestinationOffset);
	const FRotator NewRotation = bUseDestinationRotation
									 ? DestinationTransform.GetRotation().Rotator()
									 : Actor->GetActorRotation();

	// Claim the shared per-world guard before moving. TeleportTo can fire the
	// destination portal's overlap begin synchronously, and with no token in place yet
	// that arrival sends the actor straight back. A failed move keeps the short-lived
	// token, which is harmless: it only delays the next attempt by the grace period.
	if (UUTTeleporterSubsystem* Guard = UUTTeleporterSubsystem::Get(this)) {
		Guard->Grant(Actor);
	}

	if (!Actor->TeleportTo(NewLocation, NewRotation)) {
		return false;
	}

	MarkTeleported(Actor);
	FUTTeleportedSignature TeleportedEvent = OnTeleported;
	TeleportedEvent.Broadcast(this, Actor);
	return true;
}

void AUTTeleporter::SuppressActor(AActor* Actor)
{
	if (!Actor) {
		return;
	}

	RecentlyTeleported.Add(Actor);

	if (UUTTeleporterSubsystem* Guard = UUTTeleporterSubsystem::Get(this)) {
		Guard->Grant(Actor);
	}
}

void AUTTeleporter::MarkTeleported(AActor* Actor)
{
	if (!Actor) {
		return;
	}

	RecentlyTeleported.Add(Actor);

	// The arrival point is exactly where the ping-pong starts, and it is usually inside the
	// destination portal's own trigger. Marking only this portal leaves that portal free to
	// send the actor straight back. One level only: the destination marks itself, so
	// recursing through SuppressActor would run forever on a paired portal setup.
	if (AUTTeleporter* DestinationPortal = Cast<AUTTeleporter>(DestinationActor.Get())) {
		DestinationPortal->RecentlyTeleported.Add(Actor);
	}

	if (UUTTeleporterSubsystem* Guard = UUTTeleporterSubsystem::Get(this)) {
		Guard->Grant(Actor);
	}
}

void AUTTeleporter::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
									   UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	// Clients simulate nothing here: the server teleports and the position change
	// reaches clients through movement replication.
	if (!HasAuthority() || !OtherActor || RecentlyTeleported.Contains(OtherActor)) {
		return;
	}

	if (const UUTTeleporterSubsystem* Guard = UUTTeleporterSubsystem::Get(this)) {
		if (Guard->IsSuppressed(OtherActor)) {
			return;
		}
	}

	TeleportActor(OtherActor);
}

void AUTTeleporter::HandleOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
									 UPrimitiveComponent*, int32)
{
	if (OtherActor) {
		RecentlyTeleported.Remove(OtherActor);
	}
}
