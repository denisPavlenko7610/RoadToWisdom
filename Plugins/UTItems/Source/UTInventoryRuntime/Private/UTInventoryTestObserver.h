#pragma once
#include "UObject/Object.h"
#include "UTInventoryComponent.h"
#include "UTInventoryTestObserver.generated.h"

/** Private automation fixture; no gameplay API. */
UCLASS()
class UUTInventoryTestObserver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UUTInventoryComponent> Inventory;
	int32 Notifications = 0;
	int32 ObservedCount = -1;
	UFUNCTION()
	void Changed()
	{
		++Notifications;
		ObservedCount = Inventory->GetItemCount("Test.Gem");
	}
};
