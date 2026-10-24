// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomSaveCoordinator.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "RoadToWisdom.h"

#include "../RoadToWisdomExplorerController.h"
#include "../RoadToWisdomPawn.h"
#include "../Character/RoadToWisdomOnFootCharacter.h"
#include "RoadToWisdomMissionManager.h"
#include "RoadToWisdomPlacedPickup.h"
#include "EngineUtils.h"

#include "UTSaveSubsystem.h"
#include "UTWorldStateSubsystem.h"
#include "UTInventoryComponent.h"
#include "UTLever.h"

namespace
{
	constexpr int32 SectionVersion = 1;
}

void URoadToWisdomSaveCoordinator::NotifyPickupCollected(FName StableId)
{
	if (!StableId.IsNone() && !CollectedPickupIds.Contains(StableId))
	{
		CollectedPickupIds.Add(StableId);
	}
}

void URoadToWisdomSaveCoordinator::DestroyCollectedPickups() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || CollectedPickupIds.Num() == 0)
	{
		return;
	}

	TArray<ARoadToWisdomPlacedPickup*> Pickups;
	ARoadToWisdomPlacedPickup::GetAllPlacedPickups(World, Pickups);
	for (ARoadToWisdomPlacedPickup* Pickup : Pickups)
	{
		if (Pickup && CollectedPickupIds.Contains(Pickup->SaveStableId))
		{
			Pickup->Destroy();
			UE_LOG(LogRoadToWisdom, Log, TEXT("Restore: removed collected pickup '%s'."), *Pickup->SaveStableId.ToString());
		}
	}
}

bool URoadToWisdomSaveCoordinator::SaveSession(ARoadToWisdomMissionManager* Mission)
{
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World || !Mission)
	{
		return false;
	}

	ARoadToWisdomExplorerController* Controller = Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (!Controller)
	{
		UE_LOG(LogRoadToWisdom, Warning, TEXT("SaveSession: no explorer controller, skipping."));
		return false;
	}

	UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	if (!Save)
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: UTSaveSubsystem unavailable."));
		return false;
	}

	if (!CaptureAndWriteSections(Mission, Controller))
	{
		// A failed capture/write must not produce a partially updated slot.
		UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: capture/write failed, slot not written."));
		return false;
	}

	const FUTResult Result = Save->SaveToSlot(FName(SlotName));
	UE_LOG(LogRoadToWisdom, Log, TEXT("SaveSession: %s"), *Result.Message.ToString());
	return Result.IsSuccess();
}

bool URoadToWisdomSaveCoordinator::CaptureAndWriteSections(ARoadToWisdomMissionManager* Mission, ARoadToWisdomExplorerController* Controller)
{
	UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	UWorld* World = GetGameInstance()->GetWorld();
	FUTResult Result;

	// Mission stage + lever state.
	FQRMissionSection MissionSection;
	MissionSection.Stage = static_cast<int32>(Mission->GetStage());
	if (Mission->Lever)
	{
		MissionSection.bLeverOn = Mission->Lever->bIsOn;
	}
	Result = Save->WriteSection(TEXT("Mission"), SectionVersion, MissionSection);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: Mission section: %s"), *Result.Message.ToString());
		return false;
	}

	// Player inventory.
	if (Controller->OnFootCharacter && Controller->OnFootCharacter->GetInventory())
	{
		Result = Save->WriteSection(TEXT("Inventory"), SectionVersion, Controller->OnFootCharacter->GetInventory()->MakeSnapshot());
		if (!Result.IsSuccess())
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: Inventory section: %s"), *Result.Message.ToString());
			return false;
		}
	}

	// World saveables (fuse box etc.) via the UT world-state layer. Duplicate
	// stable ids fail the capture here instead of silently corrupting the save.
	FUTWorldStateSnapshot Snapshot;
	if (UUTWorldStateSubsystem* WorldState = World->GetSubsystem<UUTWorldStateSubsystem>())
	{
		Result = WorldState->CaptureWorldState(Snapshot);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: world capture: %s"), *Result.Message.ToString());
			return false;
		}
		Result = Save->WriteSection(TEXT("World"), SectionVersion, Snapshot);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: World section: %s"), *Result.Message.ToString());
			return false;
		}
	}

	// Collected placed pickups (kept out of the world snapshot: AUTWorldItem has no
	// save contract and destroys itself, so collection state lives here).
	FQRPickupsSection PickupsSection;
	PickupsSection.CollectedIds = CollectedPickupIds;
	Result = Save->WriteSection(TEXT("Pickups"), SectionVersion, PickupsSection);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: Pickups section: %s"), *Result.Message.ToString());
		return false;
	}

	// Vehicle placement.
	if (Controller->VanPawn)
	{
		FQRVehicleSection VehicleSection;
		VehicleSection.Transform = Controller->VanPawn->GetActorTransform();
		VehicleSection.bValid = true;
		Result = Save->WriteSection(TEXT("Vehicle"), SectionVersion, VehicleSection);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: Vehicle section: %s"), *Result.Message.ToString());
			return false;
		}
	}

	// Player placement + control mode.
	FQRPlayerSection PlayerSection;
	PlayerSection.bOnFoot = Controller->IsDriving() ? false : true;
	const APawn* CurrentPawn = Controller->GetPawn();
	if (CurrentPawn)
	{
		PlayerSection.Transform = CurrentPawn->GetActorTransform();
		PlayerSection.bValid = true;
	}
	Result = Save->WriteSection(TEXT("Player"), SectionVersion, PlayerSection);
	if (!Result.IsSuccess())
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("SaveSession: Player section: %s"), *Result.Message.ToString());
		return false;
	}
	return true;
}

bool URoadToWisdomSaveCoordinator::TryRestoreSession(ARoadToWisdomExplorerController* Controller)
{
	UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	UWorld* World = GetGameInstance()->GetWorld();
	if (!Save || !World || !Controller)
	{
		return false;
	}

	const FUTResult Load = Save->LoadFromSlot(FName(SlotName));
	if (!Load.IsSuccess())
	{
		UE_LOG(LogRoadToWisdom, Log, TEXT("RestoreSession: no usable save (%s), starting fresh."), *Load.Code.ToString());
		return false;
	}

	// World saveables first (fuse box), then stage, then placements/inventory.
	if (UUTWorldStateSubsystem* WorldState = World->GetSubsystem<UUTWorldStateSubsystem>())
	{
		FUTWorldStateSnapshot Snapshot;
		const FUTResult Read = Save->ReadSection(TEXT("World"), SectionVersion, Snapshot);
		if (Read.IsSuccess())
		{
			FUTResult ApplyResult;
			const int32 Applied = WorldState->ApplyWorldState(Snapshot, ApplyResult);
			UE_LOG(LogRoadToWisdom, Log, TEXT("RestoreSession: applied %d world-state entries."), Applied);
		}
	}

	FQRPickupsSection PickupsSection;
	if (Save->ReadSection(TEXT("Pickups"), SectionVersion, PickupsSection).IsSuccess())
	{
		CollectedPickupIds = PickupsSection.CollectedIds;
	}
	DestroyCollectedPickups();

	bool bMissionFound = false;
	const FQRMissionSection MissionSection = ReadMissionSection(bMissionFound);
	if (bMissionFound)
	{
		TArray<AActor*> Managers;
		UGameplayStatics::GetAllActorsOfClass(World, ARoadToWisdomMissionManager::StaticClass(), Managers);
		if (Managers.Num() > 0)
		{
			Cast<ARoadToWisdomMissionManager>(Managers[0])->ApplyState(static_cast<EQRRelayMissionStage>(MissionSection.Stage), MissionSection.bLeverOn);
		}
	}

	if (Controller->VanPawn)
	{
		const FQRVehicleSection VehicleSection = ReadVehicleSection();
		if (VehicleSection.bValid)
		{
			Controller->VanPawn->SetActorLocationAndRotation(VehicleSection.Transform.GetLocation(), VehicleSection.Transform.Rotator(),
				false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	const FQRPlayerSection PlayerSection = ReadPlayerSection();
	FUTInventorySnapshot InventorySnapshot;
	const bool bHasInventory = ReadInventorySection(InventorySnapshot);

	// The default on-foot pawn is not possessed yet at BeginPlay time, so placement
	// and inventory application are deferred to the first possession (next tick past
	// the possession stack), then possession is switched to the saved mode.
	Controller->bHasPendingRestore = true;
	Controller->PendingPlayerSection = PlayerSection;
	Controller->PendingInventory = bHasInventory ? InventorySnapshot : FUTInventorySnapshot();
	Controller->bHasPendingInventory = bHasInventory;

	UE_LOG(LogRoadToWisdom, Log, TEXT("RestoreSession: state prepared (on-foot=%s)."), PlayerSection.bOnFoot ? TEXT("true") : TEXT("false"));
	return true;
}

FQRMissionSection URoadToWisdomSaveCoordinator::ReadMissionSection(bool& bOutFound)
{
	bOutFound = false;
	FQRMissionSection Section;
	UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	if (Save && Save->ReadSection(TEXT("Mission"), SectionVersion, Section).IsSuccess())
	{
		bOutFound = true;
	}
	return Section;
}

FQRVehicleSection URoadToWisdomSaveCoordinator::ReadVehicleSection()
{
	FQRVehicleSection Section;
	if (UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>())
	{
		Save->ReadSection(TEXT("Vehicle"), SectionVersion, Section);
	}
	return Section;
}

FQRPlayerSection URoadToWisdomSaveCoordinator::ReadPlayerSection()
{
	FQRPlayerSection Section;
	if (UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>())
	{
		Save->ReadSection(TEXT("Player"), SectionVersion, Section);
	}
	return Section;
}

bool URoadToWisdomSaveCoordinator::ReadInventorySection(FUTInventorySnapshot& OutSnapshot)
{
	if (UUTSaveSubsystem* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>())
	{
		return Save->ReadSection(TEXT("Inventory"), SectionVersion, OutSnapshot).IsSuccess();
	}
	return false;
}
