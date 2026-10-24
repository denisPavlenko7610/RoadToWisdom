// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/Missions/RoadToWisdomFuseBox.h"

#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "StructUtils/InstancedStruct.h"
#include "RoadToWisdom.h"

#include "UTInventoryComponent.h"
#include "UTItemRegistry.h"

ARoadToWisdomFuseBox::ARoadToWisdomFuseBox()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	SetRootComponent(BoxMesh);

	FuseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh"));
	FuseMesh->SetupAttachment(BoxMesh);
	FuseMesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	FuseMesh->SetRelativeScale3D(FVector(0.25f));
	FuseMesh->SetVisibility(false);
}

void ARoadToWisdomFuseBox::BeginPlay()
{
	Super::BeginPlay();

	if (SaveStableId.IsNone())
	{
		UE_LOG(LogRoadToWisdom, Warning, TEXT("FuseBox %s: SaveStableId is not configured; save capture will skip it."), *GetName());
	}
	OnRep_FuseInstalled(); // sync visuals with the deserialized/initial state
}

void ARoadToWisdomFuseBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARoadToWisdomFuseBox, bFuseInstalled);
}

void ARoadToWisdomFuseBox::OnRep_FuseInstalled()
{
	if (FuseMesh)
	{
		FuseMesh->SetVisibility(bFuseInstalled);
	}
}

bool ARoadToWisdomFuseBox::CanInteract_Implementation(const FUTInteractionInfo& Info) const
{
	if (bFuseInstalled)
	{
		return false; // nothing offered anymore; prompts disappear via empty options
	}

	// Installing requires the fuse in the interacting pawn's inventory.
	const AActor* Interactor = Info.Interactor.Get();
	const UUTInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UUTInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasItems(FuseItemId, 1);
}

void ARoadToWisdomFuseBox::Interact_Implementation(const FUTInteractionInfo& Info)
{
	if (bFuseInstalled || !HasAuthority())
	{
		return;
	}

	AActor* Interactor = Info.Interactor.Get();
	UUTInventoryComponent* Inventory = Interactor ? Interactor->FindComponentByClass<UUTInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->ConsumeItems(FuseItemId, 1))
	{
		return;
	}

	bFuseInstalled = true;
	OnRep_FuseInstalled();
	OnFuseInstalled.Broadcast();

	UE_LOG(LogRoadToWisdom, Log, TEXT("FuseBox %s: fuse installed."), *GetSaveStableId_Implementation().ToString());
}

FText ARoadToWisdomFuseBox::GetInteractionPrompt_Implementation(const FUTInteractionInfo& Info) const
{
	if (bFuseInstalled)
	{
		return NSLOCTEXT("QuietRoad", "FuseBoxPowered", "Питание подключено");
	}
	return NSLOCTEXT("QuietRoad", "FuseBoxEmpty", "В щитке нет предохранителя");
}

void ARoadToWisdomFuseBox::GetInteractionOptions_Implementation(const FUTInteractionInfo& Info, TArray<FUTInteractionOption>& OutOptions) const
{
	if (bFuseInstalled)
	{
		return; // no options -> interactor hides the prompt
	}

	FUTInteractionOption Install;
	Install.OptionId = TEXT("InstallFuse");
	Install.Prompt = NSLOCTEXT("QuietRoad", "FuseBoxInstall", "E — установить предохранитель");
	OutOptions.Add(Install);
}

int32 ARoadToWisdomFuseBox::GetInteractionPriority_Implementation() const
{
	// Mission target beats plain world items in candidate scoring.
	return 10;
}

FName ARoadToWisdomFuseBox::GetSaveStableId_Implementation() const
{
	return SaveStableId;
}

FInstancedStruct ARoadToWisdomFuseBox::CaptureState_Implementation() const
{
	FQRFuseBoxState State;
	State.bFuseInstalled = bFuseInstalled;
	return FInstancedStruct::Make(State);
}

bool ARoadToWisdomFuseBox::RestoreState_Implementation(const FInstancedStruct& State)
{
	// Silent restore per the UT save contract: no OnFuseInstalled broadcast.
	const FQRFuseBoxState* Loaded = State.GetPtr<FQRFuseBoxState>();
	if (!Loaded)
	{
		return false;
	}
	bFuseInstalled = Loaded->bFuseInstalled;
	OnRep_FuseInstalled();
	return true;
}
