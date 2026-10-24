// UT Templates - teleporter actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "UTTeleporter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTTeleportedSignature, AUTTeleporter*, Teleporter, AActor*, TeleportedActor);

/**
 * Overlap-triggered teleporter. Actors are moved to DestinationActor (or to this
 * actor's own transform when unset), offset in the destination's local frame.
 *
 * Re-entry protection has two layers: the actor is marked on the portal it left *and* on
 * the portal it arrived at (which owns the trigger it is now standing in), plus a short
 * per-actor guard shared by every teleporter. One local set alone is not enough - a
 * destination placed inside another portal's trigger would bounce the actor forever.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTTeleporter : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Teleporter")
	FUTTeleportedSignature OnTeleported;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Teleporter")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Where teleported actors arrive; unset means this actor's own transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Teleporter")
	TObjectPtr<AActor> DestinationActor;

	/** Offset in the destination's local frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Teleporter")
	FVector DestinationOffset = FVector::ZeroVector;

	/** Rotate the actor to the destination's rotation instead of keeping its current one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Teleporter")
	bool bUseDestinationRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Teleporter")
	bool bEnabled = true;

	AUTTeleporter();

	/** Teleport any actor here from code; also used by the overlap trigger. */
	UFUNCTION(BlueprintCallable, Category = "UT|Teleporter")
	bool TeleportActor(AActor* Actor);

	/**
	 * Mark an actor as just-arrived so this portal's trigger ignores it.
	 *
	 * Marks only this portal. Following the destination chain from here would not
	 * terminate on a cycle, which is the normal case for a paired portal setup.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Teleporter")
	void SuppressActor(AActor* Actor);

protected:
	virtual void BeginPlay() override;

private:
	/** Actors teleported recently; entries clear when they leave the trigger. */
	TSet<TWeakObjectPtr<AActor>> RecentlyTeleported;

	/** Record a completed teleport: local set, destination portal and shared guard. */
	void MarkTeleported(AActor* Actor);

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
						  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
