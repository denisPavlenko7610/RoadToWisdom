// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UTInventoryTypes.h"
#include "RoadToWisdomSaveCoordinator.generated.h"

class ARoadToWisdomExplorerController;
class ARoadToWisdomMissionManager;
struct FUTWorldStateSnapshot;

/** Mission stage journal section. */
USTRUCT()
struct FQRMissionSection
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Stage = 0;

	/** WorldMechanics levers have no IUTSaveable contract yet, so the lever state
	 *  rides here (see Docs/ToolkitIntegrationMatrix.md). */
	UPROPERTY()
	bool bLeverOn = false;
};

/** Vehicle placement section. */
USTRUCT()
struct FQRVehicleSection
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	bool bValid = false;
};

/** Player placement + control mode section. */
USTRUCT()
struct FQRPlayerSection
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	bool bOnFoot = false;

	UPROPERTY()
	bool bValid = false;
};

/**
 *  Session orchestrator over the UT save domain (REUSE_GUIDE ownership table:
 *  world capture/restore on the authority, slot orchestration in game code).
 *
 *  Sections: "Mission" (stage + lever), "Inventory" (UT inventory snapshot),
 *  "World" (UT world-state snapshot of all IUTSaveable actors), "Vehicle" and
 *  "Player" (placement + mode). Saved at mission stage commits; restored once from
 *  BeginPlay of the explorer controller. Single-player slice: direct synchronous
 *  calls on the game thread.
 */
UCLASS()
class URoadToWisdomSaveCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr TCHAR SlotName[] = TEXT("QuietRoad01");

	/**
	 * Capture mission stage, inventory, world state and placements, then flush to
	 * the slot. Every section write and the world capture must succeed; a partial
	 * snapshot is never flushed and never reported as success. Returns the real
	 * outcome for UI.
	 */
	bool SaveSession(ARoadToWisdomMissionManager* Mission);

	/**
	 * Load the slot (if any) and rebuild: world saveables, inventory, vehicle and
	 * player placement, mission stage. Returns false when there is no valid save
	 * (fresh game) or when the load failed validation.
	 */
	bool TryRestoreSession(ARoadToWisdomExplorerController* Controller);

protected:
	FQRMissionSection ReadMissionSection(bool& bOutFound);
	FQRVehicleSection ReadVehicleSection();
	FQRPlayerSection ReadPlayerSection();
	bool ReadInventorySection(FUTInventorySnapshot& OutSnapshot);

	bool CaptureAndWriteSections(ARoadToWisdomMissionManager* Mission, ARoadToWisdomExplorerController* Controller);
};
