// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomVanRadioPanel.h"

#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "RoadToWisdom.h"

#include "../RoadToWisdomExplorerController.h"
#include "RoadToWisdomMissionManager.h"
#include "RoadToWisdomSaveCoordinator.h"

ARoadToWisdomVanRadioPanel::ARoadToWisdomVanRadioPanel()
{
	PrimaryActorTick.bCanEverTick = false;

	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PanelMesh"));
	SetRootComponent(PanelMesh);
}

void ARoadToWisdomVanRadioPanel::BeginPlay()
{
	Super::BeginPlay();

	// Attach to the one tagged van so the panel rides along. Same registration
	// contract as the controller: exactly one match is the accepted state.
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(this, VanTag, Tagged);
	if (Tagged.Num() == 1)
	{
		AttachToActor(Tagged[0], FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("VanRadioPanel: %d actors tagged '%s', expected exactly 1; panel stays placed."), Tagged.Num(), *VanTag.ToString());
	}
}

bool ARoadToWisdomVanRadioPanel::CanInteract_Implementation(const FUTInteractionInfo& Info) const
{
	return true;
}

void ARoadToWisdomVanRadioPanel::Interact_Implementation(const FUTInteractionInfo& Info)
{
	UWorld* World = GetWorld();
	if (!World || (Info.OptionId != TEXT("Radio") && Info.OptionId != TEXT("Save")))
	{
		return;
	}

	ARoadToWisdomExplorerController* Explorer = Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(World, 0));
	if (!Explorer)
	{
		return;
	}

	TArray<AActor*> Managers;
	UGameplayStatics::GetAllActorsOfClass(World, ARoadToWisdomMissionManager::StaticClass(), Managers);
	ARoadToWisdomMissionManager* Mission = Managers.Num() > 0 ? Cast<ARoadToWisdomMissionManager>(Managers[0]) : nullptr;

	if (Info.OptionId == TEXT("Save"))
	{
		if (!Mission)
		{
			ShowHudMessage(NSLOCTEXT("QuietRoad", "SaveNoMission", "Сохранение недоступно: нет активной миссии"), 3.f);
			return;
		}
		if (URoadToWisdomSaveCoordinator* Coordinator = Explorer->GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>())
		{
			// Show success only on the real FUTResult outcome (GDD §23.1).
			if (Coordinator->SaveSession(Mission))
			{
				ShowHudMessage(NSLOCTEXT("QuietRoad", "SaveDone", "Прогресс сохранён"), 3.f);
			}
			else
			{
				ShowHudMessage(NSLOCTEXT("QuietRoad", "SaveFailed", "Не удалось сохранить прогресс"), 4.f);
			}
		}
		return;
	}

	// Radio: objective plus a stage-dependent line (GDD §23.1 — plain text, no simulation).
	if (Mission)
	{
		ShowHudMessage(Mission->GetRadioMessage(), 8.f);
	}
	else
	{
		ShowHudMessage(NSLOCTEXT("QuietRoad", "RadioQuiet", "В эфире только шум ветра…"), 5.f);
	}
}

FText ARoadToWisdomVanRadioPanel::GetInteractionPrompt_Implementation(const FUTInteractionInfo& Info) const
{
	return NSLOCTEXT("QuietRoad", "RadioPrompt", "E — радио, удерживать — сохранить");
}

void ARoadToWisdomVanRadioPanel::GetInteractionOptions_Implementation(const FUTInteractionInfo& Info, TArray<FUTInteractionOption>& OutOptions) const
{
	FUTInteractionOption Radio;
	Radio.OptionId = TEXT("Radio");
	Radio.Prompt = NSLOCTEXT("QuietRoad", "RadioOption", "E — радио");
	OutOptions.Add(Radio);

	FUTInteractionOption Save;
	Save.OptionId = TEXT("Save");
	Save.Prompt = NSLOCTEXT("QuietRoad", "SaveOption", "Удерживайте E — сохранить прогресс");
	Save.HoldDuration = SaveHoldSeconds;
	OutOptions.Add(Save);
}

int32 ARoadToWisdomVanRadioPanel::GetInteractionPriority_Implementation() const
{
	// Below mission targets (fuse box = 10) so it never steals focus in front of them.
	return 5;
}

void ARoadToWisdomVanRadioPanel::ShowHudMessage(const FText& Message, float Seconds) const
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ARoadToWisdomExplorerController* Explorer = Cast<ARoadToWisdomExplorerController>(PC))
		{
			Explorer->ShowHUDHint(Message, Seconds);
		}
	}
}
