// UT Templates - checkpoint actor implementation.
#include "UTCheckpoint.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UTMessageBusSubsystem.h"
#include "UTTags.h"
#include "UTWorldMessages.h"

AUTCheckpoint::AUTCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));

	CheckpointTag = UTTags::TAG_UT_Message_CheckpointReached;
}

void AUTCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AUTCheckpoint::HandleOverlapBegin);
}

FTransform AUTCheckpoint::GetRespawnTransform() const
{
	const FTransform ActorTransform = GetActorTransform();
	FTransform Respawn = ActorTransform;
	Respawn.AddToTranslation(ActorTransform.TransformVector(RespawnOffset));
	return Respawn;
}

void AUTCheckpoint::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
									   UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!bEnabled || !OtherActor || !Cast<APawn>(OtherActor)) {
		return;
	}

	if (bTriggerOncePerPlayer && ActivatedPlayers.Contains(OtherActor)) {
		return;
	}
	// Per-player state and the bus announcement are server-side; clients receive the
	// respawn outcome through replicated gameplay state, not by replaying overlaps.
	if (HasAuthority()) {
		ActivatedPlayers.Add(OtherActor);
	}

	FUTCheckpointReachedSignature ReachedEvent = OnCheckpointReached;
	ReachedEvent.Broadcast(this, OtherActor);

	// Respawn adoption is server gameplay: clients never announce checkpoints.
	if (HasAuthority()) {
		if (UUTMessageBusSubsystem* Bus = UUTMessageBusSubsystem::Get(this)) {
			FUTCheckpointReachedMessage Message;
			Message.RespawnTransform = GetRespawnTransform();
			Message.Checkpoint = this;
			Message.Player = OtherActor;
			Bus->BroadcastMessage(CheckpointTag, Message);
		}
	}
}
