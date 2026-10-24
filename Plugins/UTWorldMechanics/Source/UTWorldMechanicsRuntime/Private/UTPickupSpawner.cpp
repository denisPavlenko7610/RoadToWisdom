// UT Templates - pickup spawner implementation.
#include "UTPickupSpawner.h"

#include "Engine/World.h"
#include "TimerManager.h"

AUTPickupSpawner::AUTPickupSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnPoint"));
	RootComponent = SpawnPoint;
}

void AUTPickupSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay) {
		SpawnNow();
	}
}

void AUTPickupSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(RespawnTimer);
	}
	// The spawned actor holds a delegate bound to this spawner. Clearing the binding keeps
	// it from firing into an actor that is already being torn down.
	if (AActor* Spawned = CurrentSpawned.Get()) {
		Spawned->OnDestroyed.RemoveDynamic(this, &AUTPickupSpawner::HandleSpawnedDestroyed);
	}
	Super::EndPlay(EndPlayReason);
}

AActor* AUTPickupSpawner::SpawnNow()
{
	// Spawns are server state; clients mirror the spawned actor via replication.
	if (!HasAuthority()) {
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World || CurrentSpawned.IsValid() || !ActorToSpawn) {
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(ActorToSpawn, GetActorTransform(), Params);
	if (!Spawned) {
		return nullptr;
	}

	CurrentSpawned = Spawned;
	Spawned->OnDestroyed.AddDynamic(this, &AUTPickupSpawner::HandleSpawnedDestroyed);

	FUTSpawnedSignature SpawnedEvent = OnSpawned;
	SpawnedEvent.Broadcast(this, Spawned);
	return Spawned;
}

void AUTPickupSpawner::ClearSpawned()
{
	if (!HasAuthority()) {
		return;
	}
	if (AActor* Spawned = CurrentSpawned.Get()) {
		// Destroy() routes through OnDestroyed, which clears the slot and starts the respawn
		// delay - the normal cycle, as documented.
		Spawned->Destroy();
		return;
	}

	// Nothing to destroy (already gone or never spawned): drop the stale handle so the
	// slot can be used again.
	CurrentSpawned = nullptr;
}

void AUTPickupSpawner::HandleSpawnedDestroyed(AActor* DestroyedActor)
{
	CurrentSpawned = nullptr;

	// Zero delay means no automatic cycle - the slot stays free for a manual SpawnNow.
	if (!HasAuthority() || RespawnDelay <= 0.f || IsActorBeingDestroyed()) {
		return;
	}
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().SetTimer(RespawnTimer, [WeakThis = TWeakObjectPtr<AUTPickupSpawner>(this)]() {
			if (AUTPickupSpawner* Spawner = WeakThis.Get())
			{
				Spawner->SpawnNow();
			} }, RespawnDelay, /*bLoop*/ false);
	}
}
