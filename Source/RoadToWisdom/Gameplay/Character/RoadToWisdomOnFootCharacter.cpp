// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomOnFootCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "RoadToWisdom.h"

#include "UTInteractorComponent.h"
#include "UTInventoryComponent.h"
#include "../RoadToWisdomExplorerController.h"

ARoadToWisdomOnFootCharacter::ARoadToWisdomOnFootCharacter()
{
	// Invisible capsule stand-in for the explorer (GDD 7.1: no character art in MVP).
	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanJump = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// UTInteraction: trace-based focus + prompts; input-agnostic, E is bound below.
	Interactor = CreateDefaultSubobject<UUTInteractorComponent>(TEXT("Interactor"));
	Interactor->InteractionDistance = 300.f;
	Interactor->PrimaryComponentTick.bCanEverTick = false; // refreshed from Tick() below

	// UTItems: mission items (e.g. the relay fuse) land here.
	Inventory = CreateDefaultSubobject<UUTInventoryComponent>(TEXT("Inventory"));
	Inventory->MaxSlots = 8;

	PrimaryActorTick.bCanEverTick = true;
}

void ARoadToWisdomOnFootCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogRoadToWisdom, Error, TEXT("OnFootCharacter: EnhancedInputComponent expected."));
		return;
	}

	// The input action assets are created by Tools/Editor scripts into
	// /Game/QuietRoad/Input; missing assets degrade to a logged error, not a crash.
	ConstructorHelpers::FObjectFinder<UInputAction> MoveActionFinder(TEXT("/Game/QuietRoad/Input/Actions/IA_QR_Move.IA_QR_Move"));
	if (MoveActionFinder.Succeeded())
	{
		Input->BindAction(MoveActionFinder.Object, ETriggerEvent::Triggered, this, &ARoadToWisdomOnFootCharacter::Move);
	}
	else
	{
		UE_LOG(LogRoadToWisdom, Warning, TEXT("OnFootCharacter: IA_QR_Move not found (run the editor input script)."));
	}

	ConstructorHelpers::FObjectFinder<UInputAction> LookActionFinder(TEXT("/Game/QuietRoad/Input/Actions/IA_QR_Look.IA_QR_Look"));
	if (LookActionFinder.Succeeded())
	{
		Input->BindAction(LookActionFinder.Object, ETriggerEvent::Triggered, this, &ARoadToWisdomOnFootCharacter::Look);
	}

	ConstructorHelpers::FObjectFinder<UInputAction> InteractActionFinder(TEXT("/Game/QuietRoad/Input/Actions/IA_QR_Interact.IA_QR_Interact"));
	if (InteractActionFinder.Succeeded())
	{
		// Deliberately input-agnostic per UTInteraction: the action just triggers the interactor.
		Input->BindAction(InteractActionFinder.Object, ETriggerEvent::Started, this, &ARoadToWisdomOnFootCharacter::HandleInteract);
	}

	// F is intentionally NOT bound here: the explorer controller owns it so that the
	// same action toggles exit (driving) and enter (on foot) without duplicate handlers.
}

void ARoadToWisdomOnFootCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Interactor)
	{
		// Prompt re-evaluation on view movement; the component itself ticks only for holds.
		Interactor->UpdateFocus();
	}
}

void ARoadToWisdomOnFootCharacter::HandleInteract()
{
	if (Interactor)
	{
		Interactor->TryInteract();
	}
}

void ARoadToWisdomOnFootCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero() || !Controller)
	{
		return;
	}

	const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(YawOnly.RotateVector(FVector::ForwardVector), Axis.Y);
	AddMovementInput(YawOnly.RotateVector(FVector::RightVector), Axis.X);
}

void ARoadToWisdomOnFootCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ARoadToWisdomOnFootCharacter::RequestVehicleToggle()
{
	if (ARoadToWisdomExplorerController* Explorer = Cast<ARoadToWisdomExplorerController>(GetController()))
	{
		Explorer->RequestVehicleToggle();
	}
}

void ARoadToWisdomOnFootCharacter::SetVehicleStowed(bool bStowed)
{
	SetActorHiddenInGame(bStowed);
	SetActorEnableCollision(!bStowed);
	SetActorTickEnabled(!bStowed);
}
