// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadToWisdomPlayerController.h"
#include "UTInventoryTypes.h"
#include "Missions/RoadToWisdomSaveCoordinator.h"
#include "RoadToWisdomExplorerController.generated.h"

class UInputAction;
class UInputMappingContext;
class URoadToWisdomHUDWidget;
class ARoadToWisdomOnFootCharacter;
class ARoadToWisdomPawn;

/**
 *  Quiet Road player controller: owns the driving <-> on-foot possession switch.
 *
 *  One pawn set, no respawning: the van is a placed template vehicle pawn that is
 *  never recreated on exit (GDD 7.1), the on-foot character is spawned once and
 *  stowed (hidden, no collision) while driving. F is handled here for both modes so
 *  that rapid presses cannot duplicate pawns or race the possession change.
 *
 *  Inherits the stock template controller so the vehicle camera/UI behavior stays
 *  intact, but overrides the parts that assume the player is always a vehicle
 *  (template OnPossess CastChecks the pawn).
 */
UCLASS(Config = Game)
class ARoadToWisdomExplorerController : public ARoadToWisdomPlayerController
{
	GENERATED_BODY()

public:
	ARoadToWisdomExplorerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	/** F handler for both control modes (exit while driving, enter while on foot). */
	UFUNCTION(BlueprintCallable, Category = "QuietRoad")
	void RequestVehicleToggle();

	/** Current control mode. */
	UFUNCTION(BlueprintPure, Category = "QuietRoad")
	bool IsDriving() const { return ControlMode == EQRControlMode::Driving; }

	/** The placed/stored van pawn (null in template-only maps). */
	UFUNCTION(BlueprintPure, Category = "QuietRoad")
	ARoadToWisdomPawn* GetVanPawn() const { return VanPawn; }

	/** HUD passthroughs (HUD exists only for local players). */
	void SetHUDObjective(const FText& Objective);
	void SetHUDPrompt(const FText& Prompt);
	void ShowHUDHint(const FText& Hint, float Seconds = 2.5f);

protected:
	/** Control modes of the explorer loop. */
	enum class EQRControlMode : uint8
	{
		Driving,
		OnFoot
	};

	/** Try to leave the van (speed + obstruction checks, GDD 7.1). */
	bool TryExitVehicle();

	/** Try to re-enter the van from on foot (distance check, stows the character). */
	bool TryEnterVehicle();

	/** Adds/removes the mapping contexts that belong to the given mode. */
	void ApplyMappingContexts(EQRControlMode Mode);

	/** True when a capsule of the on-foot pawn fits at WorldTransform (vehicle ignored). */
	bool IsExitSpotClear(const FTransform& WorldTransform) const;

	/** Loads the persisted session if a save slot exists; called once from BeginPlay. */
	void TryRestoreSession();

	/** Applies the deferred session restore on the first on-foot possession. */
	void ApplyPendingSessionRestore();

	/**
	 * Finds the one tagged placed van in the world, once, independent of possession
	 * (the game starts on foot, so OnPossess never sees the vehicle). Exactly one
	 * match is the only accepted state; zero logs a warning, several log an error
	 * and the first one wins.
	 */
	void RegisterVanFromWorld();

	/** Pushes the mission manager's current objective (if any) into the HUD. */
	void SyncHudFromMission();

	/** Reflects the current control mode in the HUD (called on every switch). */
	void UpdateHudMode();

	// -- Configuration -------------------------------------------------------

	/** On-foot pawn spawned on first exit and reused afterwards. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession")
	TSubclassOf<ARoadToWisdomOnFootCharacter> OnFootCharacterClass;

	/** Local offsets (vehicle space) tried in order for the exit spot; driver door first. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession")
	TArray<FVector> ExitOffsets;

	/** Van speed (cm/s) at or below which exiting is allowed. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession", meta = (ClampMin = "0.0"))
	float ExitMaxSpeed = 50.f;

	/** Max distance from the van to enter it again (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession", meta = (ClampMin = "0.0"))
	float EnterMaxDistance = 350.f;

	/** Local offset (vehicle space) of the driver door used for the enter distance check. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession")
	FVector EnterProbeOffset = FVector(60.f, -110.f, 0.f);

	/** Radius of the occupancy sphere tested at each exit candidate (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession", meta = (ClampMin = "0.0"))
	float ExitSpotRadius = 45.f;

	/** Tag marking the one persistent placed van in a map. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Possession")
	FName VanTag = TEXT("QuietRoad.Van");

	/** Driving contexts: template vehicle IMC plus this project's F-exit context. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Input")
	TArray<TObjectPtr<UInputMappingContext>> VehicleMappingContexts;

	/** Contexts active while on foot. */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Input")
	TArray<TObjectPtr<UInputMappingContext>> OnFootMappingContexts;

	/** F action shared by both modes (mapped in the contexts above). */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|Input")
	TObjectPtr<UInputAction> VehicleToggleAction;

	/** Programmatic HUD class (no Blueprint asset needed for the slice). */
	UPROPERTY(EditDefaultsOnly, Category = "QuietRoad|UI")
	TSubclassOf<URoadToWisdomHUDWidget> HUDWidgetClass;

	// -- Runtime state ---------------------------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<ARoadToWisdomPawn> VanPawn;

	UPROPERTY(Transient)
	TObjectPtr<ARoadToWisdomOnFootCharacter> OnFootCharacter;

	UPROPERTY(Transient)
	TObjectPtr<URoadToWisdomHUDWidget> HUD;

	EQRControlMode ControlMode = EQRControlMode::Driving;

	/** Reentrancy guard for the F action during a possession switch. */
	bool bVehicleTransitionRunning = false;

	/** Set once the session restore has run (success or fresh game). */
	bool bSessionRestoreAttempted = false;

	/** Set once the placed-van registration has run. */
	bool bVanRegistrationAttempted = false;

	/** Deferred restore payload (set by the save coordinator, applied on first possess). */
	UPROPERTY(Transient)
	bool bHasPendingRestore = false;

	FQRPlayerSection PendingPlayerSection;
	bool bHasPendingInventory = false;
	FUTInventorySnapshot PendingInventory;

	friend class URoadToWisdomSaveCoordinator;
};
