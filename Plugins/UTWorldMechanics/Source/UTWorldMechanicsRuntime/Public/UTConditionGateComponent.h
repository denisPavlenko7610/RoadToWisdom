// UT Templates - condition gate component.
#pragma once

#include "Components/ActorComponent.h"
#include "UTConditionGateComponent.generated.h"

class UUTCondition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTGateStateChangedSignature, bool, bOpen);

/**
 * The "lock" half of ConditionLock: an editable UTConditions tree that other systems
 * poll. Typical composition - a door Blueprint's CanInteract returns Gate->Passes(User),
 * and UI binds OnGateStateChanged for the locked hint.
 *
 * Conditions are pure, so Passes is safe to call every frame.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTWORLDMECHANICSRUNTIME_API UUTConditionGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Gate")
	FUTGateStateChangedSignature OnGateStateChanged;

	/** The condition tree deciding the gate. Empty means always open. */
	UPROPERTY(EditAnywhere, Instanced, Category = "UT|Gate")
	TObjectPtr<UUTCondition> Condition;

	/** Evaluate the tree. True (open) when no condition is set. */
	UFUNCTION(BlueprintPure, Category = "UT|Gate")
	bool Passes(AActor* Subject, AActor* Instigator = nullptr) const;

	/** Evaluate and broadcast OnGateStateChanged when the result changed. Call on tick or when relevant state moves. */
	UFUNCTION(BlueprintCallable, Category = "UT|Gate")
	bool UpdateGateState(AActor* Subject, AActor* Instigator = nullptr);

	UFUNCTION(BlueprintPure, Category = "UT|Gate")
	bool IsOpen() const { return bOpen; }

private:
	bool bOpen = true;
};
