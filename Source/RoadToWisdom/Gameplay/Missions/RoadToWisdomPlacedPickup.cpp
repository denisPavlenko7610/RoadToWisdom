// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomPlacedPickup.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "RoadToWisdom.h"

#include "RoadToWisdomSaveCoordinator.h"

void ARoadToWisdomPlacedPickup::Interact_Implementation(const FUTInteractionInfo& Info)
{
	// Record the collection BEFORE the super transaction: AUTWorldItem destroys the
	// actor on a full pickup, and the record must outlive it in the coordinator.
	if (URoadToWisdomSaveCoordinator* Coordinator = GetGameInstance() ? GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>() : nullptr)
	{
		Coordinator->NotifyPickupCollected(SaveStableId);
	}
	Super::Interact_Implementation(Info);
}

void ARoadToWisdomPlacedPickup::GetAllPlacedPickups(UWorld* World, TArray<ARoadToWisdomPlacedPickup*>& OutPickups)
{
	OutPickups.Reset();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ARoadToWisdomPlacedPickup> It(World); It; ++It)
	{
		OutPickups.Add(*It);
	}
}
