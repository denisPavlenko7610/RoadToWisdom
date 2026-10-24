// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/Player/RoadToWisdomExplorerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/EngineTypes.h"

#include "UTInteractorComponent.h"
#include "UTInventoryComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RoadToWisdom.h"

#include "VehicleTemplate/Core/RoadToWisdomPawn.h"
#include "Gameplay/Character/RoadToWisdomOnFootCharacter.h"
#include "Gameplay/UI/RoadToWisdomHUDWidget.h"
#include "Gameplay/Save/RoadToWisdomSaveCoordinator.h"
#include "Gameplay/Missions/RoadToWisdomMissionManager.h"

ARoadToWisdomExplorerController::ARoadToWisdomExplorerController()
{
	// Driver door first (vehicle left side is -Y), then the other side and the rear.
	ExitOffsets = {
		FVector(120.f, -190.f, 0.f),
		FVector(120.f, 190.f, 0.f),
		FVector(-285.f, -190.f, 0.f),
		FVector(-285.f, 190.f, 0.f)
	};

	OnFootCharacterClass = ARoadToWisdomOnFootCharacter::StaticClass();
	HUDWidgetClass = URoadToWisdomHUDWidget::StaticClass();

	// Asset references degrade gracefully when the editor input script has not run yet.
	ConstructorHelpers::FObjectFinder<UInputMappingContext> TemplateVehicleIMC(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (TemplateVehicleIMC.Succeeded())
	{
		VehicleMappingContexts.Add(TemplateVehicleIMC.Object);
	}
	ConstructorHelpers::FObjectFinder<UInputMappingContext> VehicleActionsIMC(TEXT("/Game/QuietRoad/Input/IMC_QR_VehicleActions.IMC_QR_VehicleActions"));
	if (VehicleActionsIMC.Succeeded())
	{
		VehicleMappingContexts.Add(VehicleActionsIMC.Object);
	}
	ConstructorHelpers::FObjectFinder<UInputMappingContext> OnFootIMC(TEXT("/Game/QuietRoad/Input/IMC_QR_OnFoot.IMC_QR_OnFoot"));
	if (OnFootIMC.Succeeded())
	{
		OnFootMappingContexts.Add(OnFootIMC.Object);
	}
	ConstructorHelpers::FObjectFinder<UInputAction> ToggleAction(TEXT("/Game/QuietRoad/Input/Actions/IA_QR_ToggleVehicle.IA_QR_ToggleVehicle"));
	if (ToggleAction.Succeeded())
	{
		VehicleToggleAction = ToggleAction.Object;
	}
}

void ARoadToWisdomExplorerController::BeginPlay()
{
	// Template BeginPlay only sets bAttachToPawn (world partition streaming) - keep it.
	ARoadToWisdomPlayerController::BeginPlay();

	if (IsLocalPlayerController() && !HUD)
	{
		HUD = CreateWidget<URoadToWisdomHUDWidget>(this, HUDWidgetClass);
		if (HUD)
		{
			HUD->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("ExplorerController: could not create the HUD widget."));
		}
	}

	// The game starts on foot, so the placed van never passes through OnPossess here:
	// register it once from the world (tagged actor), independent of the possession
	// order, then let the HUD show the current mission state immediately.
	RegisterVanFromWorld();
	SyncHudFromMission();
	UpdateHudMode();

	TryRestoreSession();
}

void ARoadToWisdomExplorerController::RegisterVanFromWorld()
{
	if (bVanRegistrationAttempted || IsValid(VanPawn))
	{
		return;
	}
	bVanRegistrationAttempted = true;

	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(this, VanTag, Tagged);

	if (Tagged.Num() == 0)
	{
		// Template maps have no tagged van; the slice map must have exactly one.
		UE_LOG(LogRoadToWisdom, Warning, TEXT("RegisterVanFromWorld: no actor tagged '%s' in this map."), *VanTag.ToString());
		return;
	}
	if (Tagged.Num() > 1)
	{
		// Ambiguity blocks registration entirely (GDD v0.3 28.1): picking an arbitrary
		// van would leave the radio/controller disagreeing about which actor is real.
		UE_LOG(LogRoadToWisdom, Error, TEXT("RegisterVanFromWorld: %d actors tagged '%s', exactly one expected. Van stays unregistered; F enter/exit disabled until fixed."), Tagged.Num(), *VanTag.ToString());
		return;
	}

	VanPawn = Cast<ARoadToWisdomPawn>(Tagged[0]);
	if (!IsValid(VanPawn))
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("RegisterVanFromWorld: tagged actor '%s' is not an ARoadToWisdomPawn."), *Tagged[0]->GetName());
	}
}

void ARoadToWisdomExplorerController::SyncHudFromMission()
{
	if (!HUD)
	{
		return;
	}

	TArray<AActor*> Managers;
	UGameplayStatics::GetAllActorsOfClass(this, ARoadToWisdomMissionManager::StaticClass(), Managers);
	if (Managers.Num() == 0)
	{
		return; // template maps: no mission, no objective
	}
	if (Managers.Num() > 1)
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("SyncHudFromMission: %d mission managers in the map, exactly one expected."), Managers.Num());
	}

	if (const ARoadToWisdomMissionManager* Manager = Cast<ARoadToWisdomMissionManager>(Managers[0]))
	{
		HUD->SetObjective(Manager->GetCurrentObjective());
	}
}

void ARoadToWisdomExplorerController::UpdateHudMode()
{
	if (HUD)
	{
		HUD->SetMode(IsDriving()
			? NSLOCTEXT("QuietRoad", "ModeDriving", "В фургоне")
			: NSLOCTEXT("QuietRoad", "ModeOnFoot", "Пешком"));
	}
}

void ARoadToWisdomExplorerController::SetupInputComponent()
{
	// Intentionally NOT calling the template implementation: it adds the template
	// mapping contexts and spawns the template vehicle UI, both of which belong to
	// the vehicle-only flow this controller replaces.
	APlayerController::SetupInputComponent();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (Input && VehicleToggleAction)
	{
		Input->BindAction(VehicleToggleAction, ETriggerEvent::Started, this, &ARoadToWisdomExplorerController::RequestVehicleToggle);
	}
}

void ARoadToWisdomExplorerController::OnPossess(APawn* InPawn)
{
	// Template OnPossess CastChecks the pawn into ARoadToWisdomPawn and would assert
	// when possessing the on-foot character, so the base implementation is used and
	// the vehicle bookkeeping is redone here.
	APlayerController::OnPossess(InPawn);

	if (ARoadToWisdomPawn* Vehicle = Cast<ARoadToWisdomPawn>(InPawn))
	{
		// The template's vehicle respawn path (VehiclePawnClass) is not configured here:
		// the van is a persistent placed actor for the slice, not a respawnable default pawn.
		VanPawn = Vehicle;
		ControlMode = EQRControlMode::Driving;
		ApplyMappingContexts(EQRControlMode::Driving);
		UpdateHudMode();
	}
	else if (ARoadToWisdomOnFootCharacter* Explorer = Cast<ARoadToWisdomOnFootCharacter>(InPawn))
	{
		OnFootCharacter = Explorer;
		Explorer->SetVehicleStowed(false);
		ControlMode = EQRControlMode::OnFoot;
		ApplyMappingContexts(EQRControlMode::OnFoot);
		UpdateHudMode();

		if (bHasPendingRestore)
		{
			// Apply outside the possession stack: the restore may switch possession
			// to the saved van, and nested Possess calls inside OnPossess are fragile.
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				ApplyPendingSessionRestore();
			}));
		}
	}
}

void ARoadToWisdomExplorerController::OnUnPossess()
{
	APlayerController::OnUnPossess();
}

void ARoadToWisdomExplorerController::Tick(float DeltaSeconds)
{
	// Template Tick guards both pointers internally (vehicle speedometer updates when
	// a vehicle UI class is configured; ours runs HUD prompt refresh instead).
	ARoadToWisdomPlayerController::Tick(DeltaSeconds);

	if (HUD)
	{
		if (ControlMode == EQRControlMode::OnFoot && OnFootCharacter && OnFootCharacter->GetInteractor())
		{
			HUD->SetPrompt(OnFootCharacter->GetInteractor()->GetFocusedPrompt());
		}
		else if (ControlMode == EQRControlMode::Driving && VanPawn)
		{
			const bool bCanExit = VanPawn->GetVelocity().Size() <= ExitMaxSpeed;
			HUD->SetPrompt(bCanExit ? NSLOCTEXT("QuietRoad", "ExitHint", "F — выйти из фургона") : FText::GetEmpty());
		}
	}
}

void ARoadToWisdomExplorerController::RequestVehicleToggle()
{
	if (bVehicleTransitionRunning)
	{
		return;
	}

	bVehicleTransitionRunning = true;
	bool bChanged = false;
	if (ControlMode == EQRControlMode::Driving)
	{
		bChanged = TryExitVehicle();
	}
	else
	{
		bChanged = TryEnterVehicle();
	}
	bVehicleTransitionRunning = false;

	if (!bChanged)
	{
		// Failure reasons are already shown by the Try* functions as HUD hints.
	}
}

bool ARoadToWisdomExplorerController::TryExitVehicle()
{
	if (!VanPawn)
	{
		UE_LOG(LogRoadToWisdom, Warning, TEXT("TryExitVehicle: no van pawn (template-only map?)."));
		return false;
	}

	if (VanPawn->GetVelocity().Size() > ExitMaxSpeed)
	{
		ShowHUDHint(NSLOCTEXT("QuietRoad", "StopFirst", "Остановите фургон, чтобы выйти"));
		return false;
	}

	const FRotator VehicleRotation = VanPawn->GetActorRotation();
	FVector ExitLocation = FVector::ZeroVector;
	bool bFoundSpot = false;

	for (const FVector& Offset : ExitOffsets)
	{
		const FVector Candidate = VanPawn->GetActorTransform().TransformPosition(Offset);

		// Drop the candidate to the ground so the character never spawns in the air.
		FHitResult GroundHit;
		const FVector TraceStart(Candidate.X, Candidate.Y, Candidate.Z + 300.f);
		const FVector TraceEnd(Candidate.X, Candidate.Y, Candidate.Z - 300.f);
		if (!UKismetSystemLibrary::LineTraceSingle(this, TraceStart, TraceEnd, UEngineTypes::ConvertToTraceType(ECC_Visibility), false,
			TArray<AActor*>{VanPawn}, EDrawDebugTrace::None, GroundHit, true))
		{
			continue;
		}

		const FVector Grounded = GroundHit.Location + FVector(0.f, 0.f, 90.f + 5.f);
		if (!IsExitSpotClear(FTransform(VehicleRotation, Grounded)))
		{
			continue;
		}

		ExitLocation = Grounded;
		bFoundSpot = true;
		break;
	}

	if (!bFoundSpot)
	{
		ShowHUDHint(NSLOCTEXT("QuietRoad", "NoExitRoom", "Вокруг фургона нет места для выхода"));
		return false;
	}

	if (!OnFootCharacter)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		OnFootCharacter = GetWorld()->SpawnActor<ARoadToWisdomOnFootCharacter>(OnFootCharacterClass, FTransform(VehicleRotation, ExitLocation), Params);
		if (!OnFootCharacter)
		{
			ShowHUDHint(NSLOCTEXT("QuietRoad", "SpawnFailed", "Не удалось создать персонажа"));
			return false;
		}
	}
	else
	{
		OnFootCharacter->SetActorLocationAndRotation(ExitLocation, VehicleRotation, false, nullptr, ETeleportType::ResetPhysics);
	}

	// Park the van: no input leaks into the physics while the player walks away.
	VanPawn->DoThrottle(0.f);
	VanPawn->DoBrakeStop();
	VanPawn->DoHandbrakeStart();

	Possess(OnFootCharacter); // OnPossess applies mode + mapping contexts

	// Save the safe parked state after deferred arrival progress, using the same
	// coordinator as the radio (no timer/polling save path).
	for (TActorIterator<ARoadToWisdomMissionManager> It(GetWorld()); It; ++It)
	{
		if (auto* Save = GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>())
			Save->SaveSession(*It);
		break;
	}

	ShowHUDHint(NSLOCTEXT("QuietRoad", "OnFootHint", "E — действие у объекта, F — сесть обратно"));
	return true;
}

bool ARoadToWisdomExplorerController::TryEnterVehicle()
{
	if (!VanPawn)
	{
		return false;
	}

	// Measure from the driver door, not from the vehicle origin: a long camper van
	// would otherwise accept an F from the middle of its body.
	const FVector DoorLocation = VanPawn->GetActorTransform().TransformPosition(EnterProbeOffset);
	if (!OnFootCharacter || FVector::Dist(OnFootCharacter->GetActorLocation(), DoorLocation) > EnterMaxDistance)
	{
		ShowHUDHint(NSLOCTEXT("QuietRoad", "ComeCloser", "Подойдите к водительской двери, чтобы сесть"));
		return false;
	}

	Possess(VanPawn); // OnPossess applies mode + mapping contexts
	VanPawn->DoHandbrakeStop();

	if (OnFootCharacter)
	{
		OnFootCharacter->SetVehicleStowed(true);
	}
	return true;
}

void ARoadToWisdomExplorerController::ApplyMappingContexts(EQRControlMode Mode)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	const TArray<TObjectPtr<UInputMappingContext>>& ToAdd = (Mode == EQRControlMode::Driving) ? VehicleMappingContexts : OnFootMappingContexts;
	const TArray<TObjectPtr<UInputMappingContext>>& ToRemove = (Mode == EQRControlMode::Driving) ? OnFootMappingContexts : VehicleMappingContexts;

	for (const TObjectPtr<UInputMappingContext>& Context : ToRemove)
	{
		if (Context)
		{
			Subsystem->RemoveMappingContext(Context);
		}
	}
	for (const TObjectPtr<UInputMappingContext>& Context : ToAdd)
	{
		if (Context)
		{
			Subsystem->AddMappingContext(Context, 0);
		}
	}
}

bool ARoadToWisdomExplorerController::IsExitSpotClear(const FTransform& WorldTransform) const
{
	FHitResult Hit;
	TArray<AActor*> Ignored;
	if (VanPawn)
	{
		Ignored.Add(VanPawn);
	}
	if (OnFootCharacter)
	{
		Ignored.Add(OnFootCharacter);
	}

	// Full capsule volume of the on-foot pawn. WorldTransform is the pawn's origin
	// (capsule center); lift the trace 2 cm so the ground the candidate stands on
	// does not count as an obstruction while walls/trees still do.
	const FVector Center = WorldTransform.GetLocation() + FVector(0.f, 0.f, 2.f);
	return !UKismetSystemLibrary::CapsuleTraceSingle(this, Center, Center,
		ExitSpotRadius, 90.f, UEngineTypes::ConvertToTraceType(ECC_Pawn), false, Ignored, EDrawDebugTrace::None, Hit, true);
}

void ARoadToWisdomExplorerController::ApplyPendingSessionRestore()
{
	if (!bHasPendingRestore)
	{
		return;
	}
	bHasPendingRestore = false;

	if (!OnFootCharacter)
	{
		return;
	}

	if (bHasPendingInventory && OnFootCharacter->GetInventory())
	{
		OnFootCharacter->GetInventory()->RestoreFromSnapshot(PendingInventory); // silent, no events
		bHasPendingInventory = false;
	}

	if (!PendingPlayerSection.bValid)
	{
		return;
	}

	if (PendingPlayerSection.bOnFoot)
	{
		OnFootCharacter->SetActorLocationAndRotation(PendingPlayerSection.Transform.GetLocation(), PendingPlayerSection.Transform.Rotator(),
			false, nullptr, ETeleportType::ResetPhysics);
	}
	else if (VanPawn)
	{
		// Saved while driving: hand the van back instead of spawning at the start spot.
		Possess(VanPawn);
		VanPawn->DoHandbrakeStop();
		if (OnFootCharacter)
		{
			OnFootCharacter->SetVehicleStowed(true);
		}
	}
}

void ARoadToWisdomExplorerController::TryRestoreSession()
{
	if (bSessionRestoreAttempted)
	{
		return;
	}
	bSessionRestoreAttempted = true;

	if (URoadToWisdomSaveCoordinator* Coordinator = GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>())
	{
		Coordinator->TryRestoreSession(this);
	}

	// Possession may have happened before this BeginPlay ran (auto-possessed local
	// players); OnPossess's next-tick hook will never fire for it. Apply now — this
	// path is outside any possession stack. The guard inside keeps it one-shot.
	if (bHasPendingRestore && OnFootCharacter)
	{
		ApplyPendingSessionRestore();
	}
}

void ARoadToWisdomExplorerController::SetHUDObjective(const FText& Objective)
{
	if (HUD)
	{
		HUD->SetObjective(Objective);
	}
}

void ARoadToWisdomExplorerController::SetHUDPrompt(const FText& Prompt)
{
	if (HUD)
	{
		HUD->SetPrompt(Prompt);
	}
}

void ARoadToWisdomExplorerController::ShowHUDHint(const FText& Hint, float Seconds)
{
	if (HUD)
	{
		HUD->ShowHint(Hint, Seconds);
	}
}
