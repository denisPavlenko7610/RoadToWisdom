// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomMissionManager.h"

#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "RoadToWisdom.h"

#include "../RoadToWisdomExplorerController.h"
#include "RoadToWisdomFuseBox.h"
#include "RoadToWisdomSaveCoordinator.h"

#include "UTLever.h"

ARoadToWisdomMissionManager::ARoadToWisdomMissionManager()
{
	PrimaryActorTick.bCanEverTick = false;

	ObjectiveWaitForArrival = NSLOCTEXT("QuietRoad", "ObjArrive", "Доехать до ретранслятора");
	ObjectiveFindFuse = NSLOCTEXT("QuietRoad", "ObjFuse", "Найти предохранитель и установить его в щиток");
	ObjectivePullLever = NSLOCTEXT("QuietRoad", "ObjLever", "Включить рубильник станции");
	ObjectiveCompleted = NSLOCTEXT("QuietRoad", "ObjDone", "Сигнал восстановлен");
	CompletionMessage = NSLOCTEXT("QuietRoad", "Mission1Done", "Ретранслятор снова в эфире. Радиосканер обновил карту района.");
}

void ARoadToWisdomMissionManager::BeginPlay()
{
	Super::BeginPlay();

	if (FuseBox)
	{
		FuseBox->OnFuseInstalled.AddDynamic(this, &ARoadToWisdomMissionManager::HandleFuseInstalled);
	}
	else
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("MissionManager: FuseBox reference is not set."));
	}

	if (Lever)
	{
		Lever->OnSwitched.AddDynamic(this, &ARoadToWisdomMissionManager::HandleLeverSwitched);
	}
	else
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("MissionManager: Lever reference is not set."));
	}

	if (Stage == EQRRelayMissionStage::WaitForArrival)
	{
		GetWorldTimerManager().SetTimer(ArrivalTimer, this, &ARoadToWisdomMissionManager::CheckArrival, 1.0f, true);
	}
	SetObjectiveText();
}

void ARoadToWisdomMissionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ArrivalTimer);
	Super::EndPlay(EndPlayReason);
}

void ARoadToWisdomMissionManager::AdvanceStage()
{
	if (Stage >= EQRRelayMissionStage::Completed)
	{
		return;
	}

	Stage = static_cast<EQRRelayMissionStage>(static_cast<uint8>(Stage) + 1);
	SetObjectiveText();

	if (Stage == EQRRelayMissionStage::Completed)
	{
		OnMissionMessage.Broadcast(CompletionMessage);
		OnMissionCompleted.Broadcast();
	}

	// Persist after every committed advance so reloads cannot replay a step.
	if (URoadToWisdomSaveCoordinator* Coordinator = GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>())
	{
		Coordinator->SaveSession(this);
	}
}

void ARoadToWisdomMissionManager::SetObjectiveText()
{
	FText Objective;
	switch (Stage)
	{
	case EQRRelayMissionStage::WaitForArrival: Objective = ObjectiveWaitForArrival; break;
	case EQRRelayMissionStage::FindFuse:       Objective = ObjectiveFindFuse; break;
	case EQRRelayMissionStage::PullLever:      Objective = ObjectivePullLever; break;
	case EQRRelayMissionStage::Completed:      Objective = ObjectiveCompleted; break;
	}

	OnObjectiveChanged.Broadcast(Objective);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ARoadToWisdomExplorerController* Explorer = Cast<ARoadToWisdomExplorerController>(PC))
		{
			Explorer->SetHUDObjective(Objective);
		}
	}
}

void ARoadToWisdomMissionManager::HandleFuseInstalled()
{
	if (Stage == EQRRelayMissionStage::FindFuse)
	{
		AdvanceStage(); // FindFuse -> PullLever
	}
}

void ARoadToWisdomMissionManager::HandleLeverSwitched(AUTLever* InLever, bool bIsOn)
{
	// Stage guard keeps replays of the lever event (including a save restore) idempotent.
	if (bIsOn && Stage == EQRRelayMissionStage::PullLever)
	{
		AdvanceStage(); // PullLever -> Completed
	}
}

void ARoadToWisdomMissionManager::CheckArrival()
{
	if (Stage != EQRRelayMissionStage::WaitForArrival || !FuseBox)
	{
		GetWorldTimerManager().ClearTimer(ArrivalTimer);
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn && FVector::Dist(PlayerPawn->GetActorLocation(), FuseBox->GetActorLocation()) <= ArriveRadius)
	{
		GetWorldTimerManager().ClearTimer(ArrivalTimer);
		AdvanceStage(); // WaitForArrival -> FindFuse
		OnMissionMessage.Broadcast(NSLOCTEXT("QuietRoad", "ArrivedMsg", "Ретранслятор рядом. Осмотрите щиток у опоры."));
	}
}

void ARoadToWisdomMissionManager::ApplyState(EQRRelayMissionStage InStage, bool bLeverWasOn)
{
	Stage = InStage;

	// The WorldMechanics lever has no IUTSaveable contract yet (see the toolkit
	// integration matrix), so its state rides in the mission section. SetOn broadcasts
	// OnSwitched, but HandleLeverSwitched is stage-guarded and no-ops after a restore.
	if (Lever && bLeverWasOn && !Lever->bIsOn)
	{
		Lever->SetOn(true);
	}

	SetObjectiveText();

	if (Stage != EQRRelayMissionStage::WaitForArrival)
	{
		GetWorldTimerManager().ClearTimer(ArrivalTimer);
	}
}
