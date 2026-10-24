// UT Templates - health types: damage, change records, snapshots, damage rules.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/NoExportTypes.h"
#include "UTHealthTypes.generated.h"

class AActor;
class UUTHealthComponent;

/** A proposed damage instance. Amount is transformed by the target's damage rules before it is applied. */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTDamage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Amount = 0.f;

	/** Meaning of the damage (e.g. Damage.Type.Fire) - used by rules to decide how to transform it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTagContainer DamageTypes;

	/** Actor responsible for the damage (owner of the weapon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Instigator;

	/** Actor that physically delivered it (the projectile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Causer;
};

/** Record of one committed health change. */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTHealthChange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bApplied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bWasDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float OldHealth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float NewHealth = 0.f;

	/** Negative for damage, positive for healing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Delta = 0.f;

	/** Valid when bWasDamage; the damage after rule transformation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FUTDamage Damage;

	bool WasFatal() const { return bApplied && bWasDamage && NewHealth <= 0.f; }
};

/** Plain data snapshot for save/load orchestration. */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTHealthSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Current = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Maximum = 0.f;
};

/**
 * One damage rule. Rules are consulted in order and may only transform the proposed
 * amount (armor, resistance, shield-first); they must not mutate the health component.
 * Returning <= 0 blocks the damage entirely.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class UTCOMBATLITE_API UUTDamageRule : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "UT")
	float ModifyDamage(const FUTDamage& Damage, const UUTHealthComponent* Health);
	virtual float ModifyDamage_Implementation(const FUTDamage& Damage, const UUTHealthComponent* Health) { return Damage.Amount; }
};
