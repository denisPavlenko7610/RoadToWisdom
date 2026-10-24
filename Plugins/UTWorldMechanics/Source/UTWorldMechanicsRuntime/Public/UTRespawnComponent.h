// UT Templates - respawn component.
#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UTRespawnComponent.generated.h"

class UUTHealthComponent;
struct FUTCheckpointReachedMessage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTRespawnedSignature);

/**
 * Player-side half of the checkpoint loop. Add to the player pawn next to a
 * UTHealthComponent: death starts RespawnDelay, then the owner is teleported to the
 * last checkpoint (or the spawn transform as fallback), revived, and OnRespawned fires.
 *
 * The component does not hide the pawn or release input - presentation of death is a
 * game concern. Do not destroy the pawn on death; the health component stays alive-ready
 * for the revive.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTWORLDMECHANICSRUNTIME_API UUTRespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Respawn")
	FUTRespawnedSignature OnRespawned;

	/** Seconds between death and respawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Respawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 2.f;

	/** Health fraction restored on respawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Respawn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RespawnHealthFraction = 1.f;

	/** Message-bus tag announcing checkpoints (must match AUTCheckpoint::CheckpointTag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Respawn")
	FGameplayTag CheckpointTag;

	/** Respawn point when no checkpoint has been reached yet; defaults to the owner's spawn transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Respawn")
	FTransform FallbackRespawnTransform;

	UUTRespawnComponent();

	/** Override the current respawn point from code (scripted saves, teleport-to-base). */
	UFUNCTION(BlueprintCallable, Category = "UT|Respawn")
	void SetRespawnPoint(const FTransform& NewRespawnTransform);

	/** Respawn immediately (skips the delay). */
	UFUNCTION(BlueprintCallable, Category = "UT|Respawn")
	void RespawnNow();

	UFUNCTION(BlueprintPure, Category = "UT|Respawn")
	FTransform GetRespawnPoint() const { return RespawnTransform; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Replicated)
	FTransform RespawnTransform;
	FDelegateHandle CheckpointListener;
	FTimerHandle RespawnTimer;
	TWeakObjectPtr<UUTHealthComponent> CachedHealth;

	void HandleCheckpointEvent(const FUTCheckpointReachedMessage& Message);

	/**
	 * True when Candidate is this component's owner or the pawn it owns. The bus broadcasts
	 * checkpoints to the whole world, so a respawn component must ignore another player's
	 * checkpoint instead of adopting its position.
	 */
	bool IsOwningPlayer(const AActor* Candidate) const;

	UFUNCTION()
	void HandleDied();
};
