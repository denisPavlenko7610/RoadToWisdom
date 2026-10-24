// UT Templates - named cooldowns component.
#pragma once

#include "Components/ActorComponent.h"
#include "UTCooldownComponent.generated.h"

/**
 * Named cooldowns on one component, tickless - backed by the engine Timer Manager.
 *
 * Session state by design: cooldowns are not part of any save snapshot (a restore
 * should decide fresh what is ready). One-shot or repeated use cases share the same
 * API; calling StartCooldown again restarts that id's timer.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTCOMBATLITE_API UUTCooldownComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTCooldownStartedSignature, FName, CooldownId, float, Duration);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTCooldownEndedSignature, FName, CooldownId);

	UPROPERTY(BlueprintAssignable, Category = "UT|Cooldowns")
	FUTCooldownStartedSignature OnCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "UT|Cooldowns")
	FUTCooldownEndedSignature OnCooldownEnded;

	/** (Re)start a named cooldown. Duration must be positive; a running cooldown restarts. */
	UFUNCTION(BlueprintCallable, Category = "UT|Cooldowns")
	void StartCooldown(FName CooldownId, float Duration);

	/** End a cooldown early and publish OnCooldownEnded. */
	UFUNCTION(BlueprintCallable, Category = "UT|Cooldowns")
	void StopCooldown(FName CooldownId);

	UFUNCTION(BlueprintCallable, Category = "UT|Cooldowns")
	void StopAllCooldowns();

	/** True while the cooldown is running. An unregistered or never-started id is ready. */
	UFUNCTION(BlueprintPure, Category = "UT|Cooldowns")
	bool IsCoolingDown(FName CooldownId) const;

	/** Ready means not cooling down. */
	UFUNCTION(BlueprintPure, Category = "UT|Cooldowns")
	bool IsReady(FName CooldownId) const { return !IsCoolingDown(CooldownId); }

	UFUNCTION(BlueprintPure, Category = "UT|Cooldowns")
	float GetRemaining(FName CooldownId) const;

	/** Total duration the cooldown was started with; 0 when not running. */
	UFUNCTION(BlueprintPure, Category = "UT|Cooldowns")
	float GetDuration(FName CooldownId) const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FUTCooldownEntry
	{
		float Duration = 0.f;
		FTimerHandle Handle;
	};

	void HandleCooldownEnded(FName CooldownId);

	TMap<FName, FUTCooldownEntry> Active;
};
