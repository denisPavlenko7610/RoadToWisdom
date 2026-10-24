// UT Templates - condition types and built-in predicates.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/NoExportTypes.h"
#include "UTConditions.generated.h"

class UUTCondition;

/** Everything a condition may look at. Conditions are pure predicates - no side effects. */
USTRUCT(BlueprintType)
struct UTFOUNDATIONRUNTIME_API FUTConditionContext
{
	GENERATED_BODY()

	/** Actor being tested (the door user, the quest owner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Subject;

	/** Actor driving the test (the interacting player), when it differs from the subject. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Instigator;
};

/**
 * One pure predicate. Evaluate must not mutate game state - gates and quests rely on
 * conditions being safely re-checkable at any moment. Compose with UUTConditionAnd/Or/Not
 * or subclass (Blueprint: override Evaluate) for custom logic.
 *
 * Deliberately no random gate: a repeated Evaluate must return the same answer, and a
 * condition cannot cache a roll (it is shared and stateless). Roll randomness where it
 * belongs - in the action that consumes the check - or pass the roll result through
 * your own context.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class UTFOUNDATIONRUNTIME_API UUTCondition : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "UT|Conditions")
	bool Evaluate(const FUTConditionContext& Context) const;
	virtual bool Evaluate_Implementation(const FUTConditionContext& Context) const { return true; }
};

/** True when every child passes; empty list passes. */
UCLASS(Blueprintable, EditInlineNew)
class UTFOUNDATIONRUNTIME_API UUTConditionAnd : public UUTCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, Category = "UT")
	TArray<TObjectPtr<UUTCondition>> Conditions;

	virtual bool Evaluate_Implementation(const FUTConditionContext& Context) const override;
};

/** True when at least one child passes; empty list fails. */
UCLASS(Blueprintable, EditInlineNew)
class UTFOUNDATIONRUNTIME_API UUTConditionOr : public UUTCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, Category = "UT")
	TArray<TObjectPtr<UUTCondition>> Conditions;

	virtual bool Evaluate_Implementation(const FUTConditionContext& Context) const override;
};

/** True when the child fails. */
UCLASS(Blueprintable, EditInlineNew)
class UTFOUNDATIONRUNTIME_API UUTConditionNot : public UUTCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, Category = "UT")
	TObjectPtr<UUTCondition> Condition;

	virtual bool Evaluate_Implementation(const FUTConditionContext& Context) const override;
};

/** True when the subject owns the tags (via IGameplayTagAssetInterface). */
UCLASS(Blueprintable, EditInlineNew)
class UTFOUNDATIONRUNTIME_API UUTConditionActorHasTag : public UUTCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bRequireAllTags = false;

	virtual bool Evaluate_Implementation(const FUTConditionContext& Context) const override;
};
