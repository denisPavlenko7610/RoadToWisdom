// UT Templates - condition gate implementation.
#include "UTConditionGateComponent.h"

#include "UTConditions.h"

bool UUTConditionGateComponent::Passes(AActor* Subject, AActor* Instigator) const
{
	if (!Condition) {
		return true;
	}

	FUTConditionContext Context;
	Context.Subject = Subject;
	Context.Instigator = Instigator;
	return Condition->Evaluate(Context);
}

bool UUTConditionGateComponent::UpdateGateState(AActor* Subject, AActor* Instigator)
{
	const bool bNewOpen = Passes(Subject, Instigator);
	if (bNewOpen != bOpen) {
		bOpen = bNewOpen;
		FUTGateStateChangedSignature StateEvent = OnGateStateChanged;
		StateEvent.Broadcast(bOpen);
	}
	return bOpen;
}
