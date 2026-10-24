// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomPlacedPickup.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "RoadToWisdom.h"

#include "RoadToWisdomSaveCoordinator.h"

void ARoadToWisdomPlacedPickup::Interact_Implementation(const FUTInteractionInfo& Info)
{
	if (SaveStableId.IsNone()) {
		UE_LOG(LogRoadToWisdom, Error, TEXT("Pickup %s needs a unique SaveStableId."), *GetName());
		return;
	}
	// TryPickup owns validation and transfer. Destroy marks the actor pending kill;
	// capture the coordinator/id before calling it, then use its accepted quantity.
	URoadToWisdomSaveCoordinator* Coordinator = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>()
		: nullptr;
	const FName Id = SaveStableId;
	const int32 Before = GetItem().GetCount();
	const int32 Accepted = TryPickup(Info.Interactor.Get());
	if (Coordinator && Accepted > 0 && Accepted == Before) {
		Coordinator->NotifyPickupCollected(Id);
	}
}

void ARoadToWisdomPlacedPickup::GetAllPlacedPickups(UWorld* World, TArray<ARoadToWisdomPlacedPickup*>& OutPickups)
{
	OutPickups.Reset();
	if (!World) {
		return;
	}
	for (TActorIterator<ARoadToWisdomPlacedPickup> It(World); It; ++It) {
		OutPickups.Add(*It);
	}
}
