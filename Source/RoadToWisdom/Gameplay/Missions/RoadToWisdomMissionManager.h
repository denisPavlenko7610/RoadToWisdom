// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadToWisdomMissionManager.generated.h"

class ARoadToWisdomFuseBox;
class AUTLever;
class URoadToWisdomSaveCoordinator;

/** Linear stage machine of mission 1 «Молчаливый ретранслятор» (GDD §10). */
UENUM(BlueprintType)
enum class EQRRelayMissionStage : uint8
{
	/** Drive to the relay station (no interaction needed yet). */
	WaitForArrival = 0,
	/** The fuse box is reachable; pick up the fuse from the crate and install it. */
	FindFuse = 1,
	/** Fuse seated; energize the station with the lever. */
	PullLever = 2,
	/** Station transmits again. */
	Completed = 3
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQRObjectiveChangedSignature, FText, NewObjective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FQRMissionMessageSignature, FText, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQRMissionCompletedSignature);

/**
 *  Minimal journal for the first mission: stages, objective text and the wiring
 *  between the fuse box (UTInteraction + UTItems) and the lever (UTWorldMechanics).
 *
 *  Deliberately not a quest framework — one enum, one actor, explicit stage guards.
 *  Each advance asks the save coordinator to persist the session, so a reload
 *  never replays a completed step (GDD §12: no duplicate rewards).
 */
UCLASS()
class ARoadToWisdomMissionManager : public AActor
{
	GENERATED_BODY()

public:
	ARoadToWisdomMissionManager();

	UPROPERTY(BlueprintAssignable, Category = "QuietRoad|Mission")
	FQRObjectiveChangedSignature OnObjectiveChanged;

	UPROPERTY(BlueprintAssignable, Category = "QuietRoad|Mission")
	FQRMissionMessageSignature OnMissionMessage;

	UPROPERTY(BlueprintAssignable, Category = "QuietRoad|Mission")
	FQRMissionCompletedSignature OnMissionCompleted;

	/** Current stage (persisted via the save coordinator). */
	UFUNCTION(BlueprintPure, Category = "QuietRoad|Mission")
	EQRRelayMissionStage GetStage() const { return Stage; }

	/** Last objective text published by SetObjectiveText (safe before any change). */
	const FText& GetCurrentObjective() const { return CurrentObjective; }

	/** Short radio line for the van panel (GDD §23.1), stage-dependent. */
	UFUNCTION(BlueprintPure, Category = "QuietRoad|Mission")
	FText GetRadioMessage() const;

	/** Restore path used by the save coordinator; silently rebuilds stage state. */
	void ApplyState(EQRRelayMissionStage InStage, bool bLeverWasOn);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Advances one stage, updates the objective and persists the session. */
	void AdvanceStage();

	void SetObjectiveText();

	UFUNCTION()
	void HandleFuseInstalled();

	UFUNCTION()
	void HandleLeverSwitched(AUTLever* Lever, bool bIsOn);

	void CheckArrival();

	/** The relay fuse box (placed with the station). */
	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	TObjectPtr<ARoadToWisdomFuseBox> FuseBox;

	/** The station power switch (UTWorldMechanics). */
	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	TObjectPtr<AUTLever> Lever;

	/** Distance from the player pawn to the fuse box that counts as "arrived" (cm). */
	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission", meta = (ClampMin = "0.0"))
	float ArriveRadius = 1500.f;

	/** Objective strings per stage; keyed by the enum value. */
	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	FText ObjectiveWaitForArrival;

	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	FText ObjectiveFindFuse;

	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	FText ObjectivePullLever;

	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	FText ObjectiveCompleted;

	UPROPERTY(EditAnywhere, Category = "QuietRoad|Mission")
	FText CompletionMessage;

	EQRRelayMissionStage Stage = EQRRelayMissionStage::WaitForArrival;

	FText CurrentObjective;

	FTimerHandle ArrivalTimer;

	friend class URoadToWisdomSaveCoordinator;
};
