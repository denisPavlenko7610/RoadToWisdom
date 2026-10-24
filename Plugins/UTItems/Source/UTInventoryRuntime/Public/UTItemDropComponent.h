#pragma once
#include "Components/ActorComponent.h"
#include "UTWorldItem.h"
#include "UTItemDropComponent.generated.h"

/** Owned-player intent endpoint for drop. Server derives location and reads actual
 * inventory state. Spawn class/transform/content cannot be supplied by a client.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTINVENTORYRUNTIME_API UUTItemDropComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UUTItemDropComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Items")
	TSubclassOf<AUTWorldItem> WorldItemClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Items")
	FVector DropOffset = FVector(120, 0, 30);
	/** Server-only synchronous API. Failure leaves inventory unchanged. */
	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	AUTWorldItem* DropStack(FName ItemId, int32 Count);
	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	AUTWorldItem* DropInstance(FGuid InstanceId);
	/** Client intents; quantity is validated against actual server inventory. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "UT|Items")
	void ServerDropStack(FName ItemId, int32 Count);
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "UT|Items")
	void ServerDropInstance(FGuid InstanceId);

private:
	AUTWorldItem* PrepareDrop();
	bool bTransacting = false;
};
