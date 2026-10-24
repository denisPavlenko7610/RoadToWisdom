// UT Templates - damage contract: the boundary between damage sources and targets.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "UTDamageContract.generated.h"

/**
 * One proposed damage instance. This is the type damage sources (hitscan, projectile,
 * melee, explosion, damage zones) hand to a target; sources never know which health
 * implementation answers it.
 *
 * BaseAmount is what the *server* decided the hit is worth. It flows through the
 * target's own rules (armor, resistance) before being applied - receivers are free to
 * transform or reject it. Never trust a client-supplied amount: build this context
 * server-side.
 */
USTRUCT(BlueprintType)
struct UTCOMBATCORE_API FUTDamageContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float BaseAmount = 0.f;

	/** Meaning of the damage (Damage.Type.Fire, Damage.Type.Kinetic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTagContainer DamageTypes;

	/** Actor responsible for the damage (weapon owner, zone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Instigator;

	/** Actor that physically delivered it (the projectile, the melee capsule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Causer;

	/** World-space point the hit landed on, when known (impact point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FVector HitLocation = FVector::ZeroVector;
};

/** What a receiver did with one damage instance. */
USTRUCT(BlueprintType)
struct UTCOMBATCORE_API FUTDamageResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bApplied = false;

	/** True when the application killed the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bFatal = false;

	/** Damage actually applied after receiver-side rules; 0 when rejected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float AmountApplied = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float HealthBefore = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float HealthAfter = 0.f;
};

UINTERFACE(BlueprintType)
class UUTDamageReceiver : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything that can take damage. The single dependency of every damage source:
 * a source calls Execute_ApplyDamage on whatever actor it hit and stays agnostic of
 * whether the target runs UUTHealthComponent, a GAS attribute set, or a game type.
 *
 * UUTHealthComponent is the reference implementation; a GAS adapter is expected to be
 * a second one. Implementations must be authority-only (apply on the server, ignore or
 * replicate the outcome on clients).
 */
class UTCOMBATCORE_API IUTDamageReceiver
{
	GENERATED_BODY()

public:
	/** Apply one damage instance. Implementations run their own rules and validation. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Combat")
	FUTDamageResult ApplyDamage(const FUTDamageContext& Context);
	virtual FUTDamageResult ApplyDamage_Implementation(const FUTDamageContext& Context) { return FUTDamageResult(); }

	/** Cheap eligibility gate for trace/overlap filtering before building a full context. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Combat")
	bool CanReceiveDamage() const;
	virtual bool CanReceiveDamage_Implementation() const { return true; }
};
