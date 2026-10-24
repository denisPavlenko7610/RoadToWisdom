// UT Templates - combat-domain conditions implementation.
#include "UTCombatConditions.h"

#include "UTStatsComponent.h"

bool UUTConditionStatValue::Evaluate_Implementation(const FUTConditionContext& Context) const
{
	const AActor* Subject = Context.Subject.Get();
	const UUTStatsComponent* Stats = Subject ? Subject->FindComponentByClass<UUTStatsComponent>() : nullptr;
	if (!Stats) {
		return false;
	}

	const float Value = Stats->GetStatValue(StatId);
	switch (Comparison) {
	case EUTStatComparison::LessThan:
		return Value < Threshold;
	case EUTStatComparison::LessOrEqual:
		return Value <= Threshold;
	case EUTStatComparison::Greater:
		return Value > Threshold;
	case EUTStatComparison::GreaterOrEqual:
		return Value >= Threshold;
	case EUTStatComparison::Equal:
		return FMath::IsNearlyEqual(Value, Threshold);
	default:
		return false;
	}
}
