// UT Templates - jump pad actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "UTJumpPad.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTLaunchedSignature, AUTJumpPad*, JumpPad, ACharacter*, LaunchedCharacter);

/** Overlap-triggered launch for characters (UT-style pads). Non-characters are ignored. */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTJumpPad : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|JumpPad")
	FUTLaunchedSignature OnLaunched;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|JumpPad")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** World-space launch velocity; a strong +Z with a little forward tilt is typical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|JumpPad")
	FVector LaunchVelocity = FVector(0.f, 0.f, 1200.f);

	/** Replace the current velocity instead of adding to it (true is the predictable default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|JumpPad")
	bool bOverrideVelocity = true;

	/** Cooldown per character (seconds); 0 disables. Uses UUTCooldownComponent-like timing via TimerManager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|JumpPad", meta = (ClampMin = "0.0"))
	float PerCharacterCooldown = 0.5f;

	AUTJumpPad();

protected:
	virtual void BeginPlay() override;

private:
	/** Character -> time when it may be launched again. */
	TMap<TWeakObjectPtr<ACharacter>, double> CooldownUntil;

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
							UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
