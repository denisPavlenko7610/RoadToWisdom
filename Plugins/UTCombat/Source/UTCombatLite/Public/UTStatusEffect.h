// UT Templates - status effect definition.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UTStatsTypes.h"
#include "UTHealthTypes.h"
#include "UObject/NoExportTypes.h"
#include "UTStatusEffect.generated.h"

UENUM(BlueprintType)
enum class EUTStatusEffectStacking : uint8
{
	/** Re-arm the duration of the already-active effect; modifiers stay as they were. */
	Refresh,
	/** Add another independent instance of the effect. */
	Stack,
	/** Do nothing while an instance of this effect is active. */
	Ignore
};

/** One stat change applied while the effect runs (mirrors UTStats modifiers). */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTStatusStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName StatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	EUTStatModifierOp Op = EUTStatModifierOp::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Magnitude = 0.f;
};

/**
 * Data for one status effect (buff or debuff). Instances are shared definitions:
 * ApplyEffect registers an active instance per target, so one UUTStatusEffect asset
 * can burn any number of actors at once.
 *
 * Session state by design - effects are never serialized; a restore re-applies what
 * the loading code decides is still relevant.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class UTCOMBATLITE_API UUTStatusEffect : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName EffectName;

	/** Meaning of the effect (Status.Buff.Speed, Status.Debuff.Burn) - used for filtering and immunity rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTagContainer EffectTags;

	/** Seconds until automatic removal; 0 runs until RemoveEffect/RemoveEffectsFromSource. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT", meta = (ClampMin = "0.0"))
	float Duration = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	EUTStatusEffectStacking Stacking = EUTStatusEffectStacking::Refresh;

	/** Stat changes applied on start and reverted on removal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TArray<FUTStatusStatModifier> StatModifiers;

	/** > 0: deals this damage every DamagePeriod through UT Health (burn, poison). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FUTDamage PeriodicDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT", meta = (ClampMin = "0.05", EditCondition = "PeriodicDamage.Amount > 0"))
	float DamagePeriod = 1.f;
};
