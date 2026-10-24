#pragma once
#include "GameFramework/Actor.h"
#include "UTInteractable.h"
#include "UTInteractionTestActor.generated.h"

UCLASS(NotBlueprintable, Transient)
class AUTInteractionTestActor : public AActor, public IUTInteractable
{
	GENERATED_BODY()
public:
	/** Hold seconds the fixture option requires; tests tick the timer past this to fire it. */
	static constexpr float HoldDurationSeconds = 2.f;

	int32 Executions = 0;
	virtual void Interact_Implementation(const FUTInteractionInfo&) override { ++Executions; }
	virtual void GetInteractionOptions_Implementation(const FUTInteractionInfo&, TArray<FUTInteractionOption>& Options) const override
	{
		FUTInteractionOption Option;
		Option.OptionId = "Hold";
		Option.HoldDuration = HoldDurationSeconds;
		Options.Add(Option);
	}
};
