#pragma once
#include "UObject/Object.h"
#include "UTActionCost.generated.h"
/** Small optional resource-spend contract. CanPay is pure; Pay must be server-only,
 * all-or-nothing and revalidate resources. Domains supply adapters; no inventory knowledge here. */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class UTFOUNDATIONRUNTIME_API UUTActionCost : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = "UT|Cost")
	bool CanPay(AActor* User) const;
	virtual bool CanPay_Implementation(AActor* User) const { return false; }
	UFUNCTION(BlueprintNativeEvent, Category = "UT|Cost")
	bool Pay(AActor* User);
	virtual bool Pay_Implementation(AActor* User) { return false; }
};
