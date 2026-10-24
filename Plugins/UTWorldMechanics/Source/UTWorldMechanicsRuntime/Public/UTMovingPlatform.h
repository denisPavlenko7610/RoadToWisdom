// UT Templates - moving platform actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "UTMovingPlatform.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTPlatformReachedEndSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTPlatformReachedStartSignature);

/**
 * Platform easing between two local-space offsets from its placement point.
 *
 * The root StaticMeshComponent is the movement base: characters standing on it ride
 * along through the engine's base-movement tracking - no attachment bookkeeping here.
 *
 * Motion phases: Move (start->end), Pause (PauseAtEnds), Move (end->start when
 * bPingPong), or jump back to start for one-way loops.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTMovingPlatform : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|MovingPlatform")
	FUTPlatformReachedEndSignature OnReachedEnd;

	UPROPERTY(BlueprintAssignable, Category = "UT|MovingPlatform")
	FUTPlatformReachedStartSignature OnReachedStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|MovingPlatform")
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	/** Local-space offset of the far end relative to the placement point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|MovingPlatform")
	FVector EndOffset = FVector(0.f, 0.f, 400.f);

	/** Seconds for one leg of the trip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|MovingPlatform", meta = (ClampMin = "0.05"))
	float MoveTime = 4.f;

	/** Dwell at each endpoint before turning around (or before the jump back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|MovingPlatform", meta = (ClampMin = "0.0"))
	float PauseAtEnds = 0.f;

	/** Back-and-forth motion; when false the platform jumps back to start after reaching the end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|MovingPlatform")
	bool bPingPong = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|MovingPlatform")
	bool bActive = true;

	UFUNCTION(BlueprintCallable, Category = "UT|MovingPlatform")
	void SetActive(bool bNewActive);

	/** 0 = at start, 1 = at end; useful for progress UI. */
	UFUNCTION(BlueprintPure, Category = "UT|MovingPlatform")
	float GetProgress() const { return Alpha; }

	AUTMovingPlatform();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	FVector BaseLocation;
	float Alpha = 0.f;
	int32 Direction = 1;
	bool bPaused = false;
	float PauseRemaining = 0.f;

	FVector GetLocationForAlpha(float InAlpha) const;
};
