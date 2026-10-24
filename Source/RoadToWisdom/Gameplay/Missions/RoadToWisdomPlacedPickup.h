// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTWorldItem.h"
#include "RoadToWisdomPlacedPickup.generated.h"

/**
 *  Level-placed quest pickup with a stable id (GDD §12: quest items must not
 *  respawn after a load).
 *
 *  AUTWorldItem destroys itself on a full pickup and carries no save contract —
 *  per the UT REUSE_GUIDE ownership table, persisting collection state is the
 *  project's job. This subclass reports its stable id to the save coordinator
 *  the moment it is picked up; the coordinator stores the collected-id set in
 *  its own section and destroys matching placed pickups on restore. No plugin
 *  code is duplicated: the pickup transaction itself still runs in AUTWorldItem.
 */
UCLASS()
class ARoadToWisdomPlacedPickup : public AUTWorldItem
{
	GENERATED_BODY()

public:
	/** Persistent identity for the save system (never derived from runtime names). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuietRoad|Save")
	FName SaveStableId = TEXT("QuietRoad.Pickup.Default");

	virtual void Interact_Implementation(const FUTInteractionInfo& Info) override;

	/** All placed pickups currently in the world (restore-time sweep helper). */
	static void GetAllPlacedPickups(UWorld* World, TArray<ARoadToWisdomPlacedPickup*>& OutPickups);
};
