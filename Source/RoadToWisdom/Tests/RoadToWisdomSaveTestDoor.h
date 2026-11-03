#pragma once

#include "UTDoor.h"
#include "RoadToWisdomSaveTestDoor.generated.h"

/** Transient failure injection for persistence automation, never placed in content. */
UCLASS(Transient, NotBlueprintable)
class ARoadToWisdomSaveTestDoor : public AUTDoor
{
	GENERATED_BODY()

public:
	bool bRejectNextRestore = false;
	bool bRejectEveryRestore = false;

	virtual bool RestoreState_Implementation(const FInstancedStruct& State) override
	{
		const bool bAccepted = Super::RestoreState_Implementation(State);
		if (bRejectNextRestore || bRejectEveryRestore) {
			bRejectNextRestore = false;
			return false; // Deliberately mutates before refusal to exercise compensation.
		}
		return bAccepted;
	}
};
