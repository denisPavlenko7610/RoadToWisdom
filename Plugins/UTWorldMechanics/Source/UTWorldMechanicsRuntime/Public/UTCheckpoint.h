// UT Templates - checkpoint actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "GameplayTagContainer.h"
#include "UTCheckpoint.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTCheckpointReachedSignature, AUTCheckpoint*, Checkpoint, AActor*, Player);

/**
 * Respawn point: pawn overlap announces the checkpoint on the UT event bus under
 * CheckpointTag (default "Checkpoint.Reached", payload: Instigator = player,
 * Target = this checkpoint) and through the local OnCheckpointReached delegate.
 *
 * It deliberately knows nothing about the respawn side - a UUTRespawnComponent (or
 * game code) listens and adopts this transform. Persistence of the active checkpoint
 * belongs to the save orchestrator.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Checkpoint")
	FUTCheckpointReachedSignature OnCheckpointReached;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Checkpoint")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Bus tag announced on activation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Checkpoint")
	FGameplayTag CheckpointTag;

	/** Arrival offset in the checkpoint's local frame (e.g. a step to the side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Checkpoint")
	FVector RespawnOffset = FVector::ZeroVector;

	/** Fire once per player; false lets triggers re-fire on every entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Checkpoint")
	bool bTriggerOncePerPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Checkpoint")
	bool bEnabled = true;

	AUTCheckpoint();

	FTransform GetRespawnTransform() const;

protected:
	virtual void BeginPlay() override;

private:
	TSet<TWeakObjectPtr<AActor>> ActivatedPlayers;

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
