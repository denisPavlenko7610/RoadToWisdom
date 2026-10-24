// UT Templates - respawn component implementation.
#include "UTRespawnComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UTMessageBusSubsystem.h"
#include "UTTags.h"
#include "UTWorldMessages.h"
#include "UTHealthComponent.h"
#include "Net/UnrealNetwork.h"

UUTRespawnComponent::UUTRespawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	CheckpointTag = UTTags::TAG_UT_Message_CheckpointReached;
}

void UUTRespawnComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}

	if (AActor* Owner = GetOwner()) {
		RespawnTransform = Owner->GetActorTransform();
		FallbackRespawnTransform = RespawnTransform;
	}

	CachedHealth = GetOwner() ? GetOwner()->FindComponentByClass<UUTHealthComponent>() : nullptr;
	if (UUTHealthComponent* Health = CachedHealth.Get()) {
		Health->OnDied.AddDynamic(this, &UUTRespawnComponent::HandleDied);
	}

	// Dynamic binding: the binding self-skips if this component dies before the bus does.
	if (UUTMessageBusSubsystem* Bus = UUTMessageBusSubsystem::Get(this)) {
		CheckpointListener = Bus->ListenTyped<FUTCheckpointReachedMessage>(CheckpointTag,
																		   [WeakThis = TWeakObjectPtr<UUTRespawnComponent>(this)](const FUTCheckpointReachedMessage& Message) {
																			   if (UUTRespawnComponent* StrongThis = WeakThis.Get()) {
																				   StrongThis->HandleCheckpointEvent(Message);
																			   }
																		   });
	}
}

void UUTRespawnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UUTRespawnComponent, RespawnTransform, COND_OwnerOnly);
}

void UUTRespawnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	if (auto* Health = CachedHealth.Get()) {
		Health->OnDied.RemoveDynamic(this, &UUTRespawnComponent::HandleDied);
	}
	if (auto* Bus = UUTMessageBusSubsystem::Get(this)) {
		Bus->RemoveListener(CheckpointListener);
	}
	Super::EndPlay(EndPlayReason);
}

void UUTRespawnComponent::SetRespawnPoint(const FTransform& NewRespawnTransform)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}
	RespawnTransform = NewRespawnTransform;
}

void UUTRespawnComponent::RespawnNow()
{
	// Server gameplay: teleports, timers and revives never run client-side.
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}

	UWorld* World = GetWorld();
	if (!World) {
		return;
	}

	World->GetTimerManager().ClearTimer(RespawnTimer);

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->TeleportTo(RespawnTransform.GetLocation(), RespawnTransform.Rotator())) {
		return;
	}

	if (UUTHealthComponent* Health = CachedHealth.Get()) {
		Health->Revive(RespawnHealthFraction);
	}

	FUTRespawnedSignature RespawnedEvent = OnRespawned;
	RespawnedEvent.Broadcast();
}

void UUTRespawnComponent::HandleDied()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) {
		return;
	}
	UWorld* World = GetWorld();
	if (!World || RespawnDelay <= 0.f) {
		RespawnNow();
		return;
	}

	World->GetTimerManager().SetTimer(RespawnTimer, this, &UUTRespawnComponent::RespawnNow, RespawnDelay, false);
}

void UUTRespawnComponent::HandleCheckpointEvent(const FUTCheckpointReachedMessage& Message)
{
	// The bus is a world-wide channel, so every respawn component in the game receives this
	// broadcast. Without this check the last player to touch any checkpoint rewrites every
	// other player's respawn transform.
	if (!IsOwningPlayer(Message.Player.Get())) {
		return;
	}

	SetRespawnPoint(Message.RespawnTransform);
}

bool UUTRespawnComponent::IsOwningPlayer(const AActor* Candidate) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Candidate) {
		return false;
	}
	if (Candidate == Owner) {
		return true;
	}

	// The component is documented on the pawn, but a controller-owned component must still
	// respond to its own pawn's checkpoint: compare pawn identity, not just the owner.
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn) {
		if (const AController* OwnerController = Cast<AController>(Owner)) {
			OwnerPawn = OwnerController->GetPawn();
		}
	}
	return OwnerPawn && Candidate == OwnerPawn;
}
