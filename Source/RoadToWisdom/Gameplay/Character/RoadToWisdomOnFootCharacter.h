// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RoadToWisdomOnFootCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
struct FInputActionValue;
class UUTInteractorComponent;
class UUTInventoryComponent;

/**
 *  On-foot explorer pawn for the Quiet Road vertical slice.
 *
 *  A deliberately minimal third-person ACharacter: walk + look + trace-based
 *  interaction (UTInteraction) + item inventory (UTItems). No skeletal mesh yet —
 *  the pawn is an invisible capsule by design (GDD §7.1 allows a temporary capsule).
 *
 *  The Enhanced Input actions live in /Game/QuietRoad/Input and are wired via
 *  FObjectFinder; the mapping contexts themselves are owned by the explorer
 *  controller, which switches between vehicle and on-foot sets.
 */
UCLASS()
class ARoadToWisdomOnFootCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARoadToWisdomOnFootCharacter();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Interactor component (E key focus/interact, UTInteractionRuntime). */
	UFUNCTION(BlueprintPure, Category = "QuietRoad")
	UUTInteractorComponent* GetInteractor() const { return Interactor; }

	/** Inventory component (UTInventoryRuntime); receives picked-up items. */
	UFUNCTION(BlueprintPure, Category = "QuietRoad")
	UUTInventoryComponent* GetInventory() const { return Inventory; }

	/** Stow/unstow the pawn while the player is driving (hidden, no collision, no ticking). */
	void SetVehicleStowed(bool bStowed);

protected:
	/** Move input (IA_QR_Move, Axis2D WASD). */
	void Move(const FInputActionValue& Value);

	/** Look input (IA_QR_Look, Axis2D mouse). */
	void Look(const FInputActionValue& Value);

	/** F pressed while on foot: routed to the explorer controller. */
	void RequestVehicleToggle();

	/** E pressed while on foot: interacts with the focused target (UTInteraction). */
	void HandleInteract();

	/** Third-person camera rig. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FollowCamera;

	/** Trace-based interaction (UTInteraction). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UUTInteractorComponent* Interactor;

	/** Slot inventory for picked-up mission items (UTItems). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UUTInventoryComponent* Inventory;

	/** Walking speed; running/jumping are intentionally out of the first slice. */
	UPROPERTY(EditAnywhere, Category = "QuietRoad|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 300.f;
};
