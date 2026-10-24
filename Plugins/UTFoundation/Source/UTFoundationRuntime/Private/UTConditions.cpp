// UT Templates - built-in condition predicates implementation.
#include "UTConditions.h"

#include "GameplayTagAssetInterface.h"

bool UUTConditionAnd::Evaluate_Implementation(const FUTConditionContext& Context) const
{
	for (const TObjectPtr<UUTCondition>& Condition : Conditions) {
		if (Condition && !Condition->Evaluate(Context)) {
			return false;
		}
	}
	return true;
}

bool UUTConditionOr::Evaluate_Implementation(const FUTConditionContext& Context) const
{
	for (const TObjectPtr<UUTCondition>& Condition : Conditions) {
		if (Condition && Condition->Evaluate(Context)) {
			return true;
		}
	}
	return false;
}

bool UUTConditionNot::Evaluate_Implementation(const FUTConditionContext& Context) const
{
	return !Condition || !Condition->Evaluate(Context);
}

bool UUTConditionActorHasTag::Evaluate_Implementation(const FUTConditionContext& Context) const
{
	const AActor* Subject = Context.Subject.Get();
	if (!Subject || RequiredTags.IsEmpty()) {
		return Subject != nullptr;
	}

	FGameplayTagContainer OwnedTags;
	if (const IGameplayTagAssetInterface* TagAsset = Cast<IGameplayTagAssetInterface>(Subject)) {
		TagAsset->GetOwnedGameplayTags(OwnedTags);
	}
	return bRequireAllTags ? OwnedTags.HasAll(RequiredTags) : OwnedTags.HasAny(RequiredTags);
}
