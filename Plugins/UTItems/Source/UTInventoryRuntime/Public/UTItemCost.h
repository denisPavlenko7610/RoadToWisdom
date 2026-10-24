#pragma once
#include "UTActionCost.h"
#include "UTItemCost.generated.h"
/** Inventory adapter for an all-or-nothing stack requirement/cost. No lock/door dependency. */
UCLASS(Blueprintable, EditInlineNew)
class UTINVENTORYRUNTIME_API UUTItemCost : public UUTActionCost
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Item Cost")
	FName ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Item Cost", meta = (ClampMin = "1"))
	int32 Count = 1;
	virtual bool CanPay_Implementation(AActor* User) const override;
	virtual bool Pay_Implementation(AActor* User) override;
};
