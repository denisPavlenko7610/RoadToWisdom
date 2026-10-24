// UT Templates - combat-domain conditions (depend on UTStats, live in UTCombatLite).
#pragma once

#include "CoreMinimal.h"
#include "UTConditions.h"
#include "UTCombatConditions.generated.h"

UENUM(BlueprintType)
enum class EUTStatComparison : uint8
{
	LessThan,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
	Equal
};

/**
 * Compares one UTStats value on the subject against a threshold.
 *
 * This condition lives in UTCombatLite rather than the foundation conditions module:
 * the base condition set must stay free of domain dependencies so every consumer
 * (doors, quests, dialogue) can use conditions without pulling in stats.
 */
UCLASS(Blueprintable, EditInlineNew)
class UTCOMBATLITE_API UUTConditionStatValue : public UUTCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName StatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	EUTStatComparison Comparison = EUTStatComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Threshold = 0.f;

	virtual bool Evaluate_Implementation(const FUTConditionContext& Context) const override;
};
