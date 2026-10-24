// UT Templates - pickup spawner actor.
#pragma once

#include "GameFramework/Actor.h"
#include "UTPickupSpawner.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTSpawnedSignature, AUTPickupSpawner*, Spawner, AActor*, SpawnedActor);

/**
 * Keeps one actor instance alive at its location and respawns it after a delay when
 * it disappears (collected, broken, destroyed). Pair with UTCollectible pickups via
 * game configuration: set the pickup actor class here, the collectible destroys the
 * instance, the spawner brings it back.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTPickupSpawner : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|PickupSpawner")
	FUTSpawnedSignature OnSpawned;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|PickupSpawner")
	TObjectPtr<USceneComponent> SpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|PickupSpawner")
	TSubclassOf<AActor> ActorToSpawn;

	/** Seconds before respawning after the current actor disappears; 0 means manual only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|PickupSpawner", meta = (ClampMin = "0.0"))
	float RespawnDelay = 5.f;

	/** Spawn the first actor in BeginPlay; independent of RespawnDelay (0 only disables the respawn cycle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|PickupSpawner")
	bool bSpawnOnBeginPlay = true;

	AUTPickupSpawner();

	/** Spawn immediately if the slot is free; returns the spawned actor or null. */
	UFUNCTION(BlueprintCallable, Category = "UT|PickupSpawner")
	AActor* SpawnNow();

	/** Remove the current actor and let the normal respawn cycle continue. */
	UFUNCTION(BlueprintCallable, Category = "UT|PickupSpawner")
	void ClearSpawned();

	UFUNCTION(BlueprintPure, Category = "UT|PickupSpawner")
	AActor* GetSpawnedActor() const { return CurrentSpawned.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<AActor> CurrentSpawned;
	FTimerHandle RespawnTimer;

	/**
	 * Respawn is event-driven: the spawned actor's OnDestroyed is the single trigger, so
	 * the delay always starts when the pickup actually disappears. Polling cannot tell a
	 * collected pickup from one still sitting there.
	 */
	UFUNCTION()
	void HandleSpawnedDestroyed(AActor* DestroyedActor);
};
