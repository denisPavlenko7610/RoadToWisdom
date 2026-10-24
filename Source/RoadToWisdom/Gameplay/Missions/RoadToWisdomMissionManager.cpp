// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/Missions/RoadToWisdomMissionManager.h"

#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "RoadToWisdom.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"

#include "Gameplay/Player/RoadToWisdomExplorerController.h"
#include "Gameplay/Missions/RoadToWisdomFuseBox.h"
#include "Gameplay/Save/RoadToWisdomSaveCoordinator.h"

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

	if (FuseBox) {
		FuseBox->OnFuseInstalled.AddDynamic(this, &ARoadToWisdomMissionManager::HandleFuseInstalled);
	}
	else {
		UE_LOG(LogRoadToWisdom, Error, TEXT("MissionManager: FuseBox reference is not set."));
	}

	if (Lever) {
		Lever->OnSwitched.AddDynamic(this, &ARoadToWisdomMissionManager::HandleLeverSwitched);
	}
	else {
		UE_LOG(LogRoadToWisdom, Error, TEXT("MissionManager: Lever reference is not set."));
	}

	if (Stage == EQRRelayMissionStage::WaitForArrival) {
		GetWorldTimerManager().SetTimer(ArrivalTimer, this, &ARoadToWisdomMissionManager::CheckArrival, 1.0f, true);
	}
	SetObjectiveText();
}

void ARoadToWisdomMissionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ArrivalTimer);
	Super::EndPlay(EndPlayReason);
}

FText ARoadToWisdomMissionManager::GetRadioMessage() const
{
	switch (Stage) {
		case EQRRelayMissionStage::WaitForArrival: return NSLOCTEXT(
				"QuietRoad", "RadioStage0", "Тишина в эфире. Координаты ретранслятора помечены: восток, у дороги."
			);
		case EQRRelayMissionStage::FindFuse: return NSLOCTEXT(
				"QuietRoad", "RadioStage1", "Ретранслятор молчит из-за питания. Найдите предохранитель и вставьте его в щиток."
			);
		case EQRRelayMissionStage::PullLever: return NSLOCTEXT(
				"QuietRoad", "RadioStage2", "Питание почти восстановлено. Включите рубильник станции."
			);
		case EQRRelayMissionStage::Completed:
		default: return NSLOCTEXT("QuietRoad", "RadioStage3", "Ретранслятор в эфире. Радиосканер обновил карту района.");
	}
}

void ARoadToWisdomMissionManager::AdvanceStage()
{
	if (Stage >= EQRRelayMissionStage::Completed) {
		return;
	}

	Stage = static_cast<EQRRelayMissionStage>(static_cast<uint8>(Stage) + 1);
	SetObjectiveText();

	if (Stage == EQRRelayMissionStage::Completed) {
		OnMissionMessage.Broadcast(CompletionMessage);
		OnMissionCompleted.Broadcast();
	}

	// Arrival can occur while driving. Persist at the safe exit event instead of
	// reporting an expected moving-vehicle refusal as a disk failure.
	if (auto* Explorer = Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		if (Explorer->GetVanPawn() && Explorer->GetVanPawn()->GetVelocity().Size() > 50.f)
		{
			return;
		}
	}
	// Persist after every committed advance so reloads cannot replay a step.
	if (URoadToWisdomSaveCoordinator* Coordinator = GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>()) {
		if (!Coordinator->SaveSession(this)) {
			UE_LOG(LogRoadToWisdom, Error, TEXT("AdvanceStage: session save failed, progress may not persist."));
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) {
				if (ARoadToWisdomExplorerController* Explorer = Cast<ARoadToWisdomExplorerController>(PC)) {
					Explorer->ShowHUDHint(NSLOCTEXT("QuietRoad", "SaveFailed", "Не удалось сохранить прогресс"));
				}
			}
		}
	}
}

void ARoadToWisdomMissionManager::SetObjectiveText()
{
	FText Objective;
	switch (Stage) {
		case EQRRelayMissionStage::WaitForArrival: Objective = ObjectiveWaitForArrival;
			break;
		case EQRRelayMissionStage::FindFuse: Objective = ObjectiveFindFuse;
			break;
		case EQRRelayMissionStage::PullLever: Objective = ObjectivePullLever;
			break;
		case EQRRelayMissionStage::Completed: Objective = ObjectiveCompleted;
			break;
	}
	if (Lever) Lever->bUsable = Stage == EQRRelayMissionStage::PullLever;
	CurrentObjective = Objective;

	OnObjectiveChanged.Broadcast(Objective);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) {
		if (ARoadToWisdomExplorerController* Explorer = Cast<ARoadToWisdomExplorerController>(PC)) {
			Explorer->SetHUDObjective(Objective);
		}
	}
}

void ARoadToWisdomMissionManager::HandleFuseInstalled()
{
	if (Stage == EQRRelayMissionStage::FindFuse) {
		AdvanceStage(); // FindFuse -> PullLever
	}
}

void ARoadToWisdomMissionManager::HandleLeverSwitched(AUTLever* InLever, bool bIsOn)
{
	// Stage guard keeps replays of the lever event (including a save restore) idempotent.
	if (bIsOn && Stage == EQRRelayMissionStage::PullLever) {
		AdvanceStage(); // PullLever -> Completed
	}
}

void ARoadToWisdomMissionManager::CheckArrival()
{
	if (Stage != EQRRelayMissionStage::WaitForArrival || !FuseBox) {
		GetWorldTimerManager().ClearTimer(ArrivalTimer);
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn && FVector::Dist(PlayerPawn->GetActorLocation(), FuseBox->GetActorLocation()) <= ArriveRadius) {
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
	if (Lever) {
		Lever->bIsOn = bLeverWasOn;
		if (Lever->HandleMesh) Lever->HandleMesh->SetRelativeRotation(bLeverWasOn ? Lever->HandleOnRotation : FRotator::ZeroRotator);
	}

	SetObjectiveText();

	if (Stage != EQRRelayMissionStage::WaitForArrival) {
		GetWorldTimerManager().ClearTimer(ArrivalTimer);
	}
}
