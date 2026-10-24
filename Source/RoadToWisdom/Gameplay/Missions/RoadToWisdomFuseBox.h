// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UTInteractable.h"
#include "UTSaveable.h"
#include "RoadToWisdomFuseBox.generated.h"

class UStaticMeshComponent;
struct FUTInteractionInfo;
struct FUTInteractionOption;
struct FInstancedStruct;

/** Persisted state of one fuse box (IUTSaveable payload). */
USTRUCT()
struct FQRFuseBoxState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bFuseInstalled = false;
};

/**
 *  Relay-station fuse box for mission 1 («Молчаливый ретранслятор»).
 *
 *  Offers "install fuse" while the socket is empty and the interacting pawn carries
 *  the fuse item (UTItems inventory). Installing consumes the item and flips the box
 *  into the powered state; the switch (AUTLever) that actually energizes the station
 *  is wired in game code via the mission manager, keeping the box a single-purpose
 *  object. WorldMechanics' lever has no IUTSaveable contract yet, so this box is the
 *  mission's saveable anchor and the mission section records the lever separately.
 */
UCLASS()
class ARoadToWisdomFuseBox : public AActor, public IUTInteractable, public IUTSaveable
{
	GENERATED_BODY()

public:
	ARoadToWisdomFuseBox();

	/** Fired on the installing client path only when authority commits the fuse. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQRFuseInstalledSignature);

	UPROPERTY(BlueprintAssignable, Category = "QuietRoad|FuseBox")
	FQRFuseInstalledSignature OnFuseInstalled;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuietRoad")
	TObjectPtr<UStaticMeshComponent> BoxMesh;

	/** Small indicator shown only after the fuse is seated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuietRoad")
	TObjectPtr<UStaticMeshComponent> FuseMesh;

	/** Whether the fuse is seated (replicated; the gameplay state of the box). */
	UPROPERTY(ReplicatedUsing = OnRep_FuseInstalled, BlueprintReadOnly, Category = "QuietRoad")
	bool bFuseInstalled = false;

	/** Item id consumed on install; matches the item definition data asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuietRoad")
	FName FuseItemId = TEXT("Item.Fuse");

	// -- IUTInteractable -------------------------------------------------------
	virtual bool CanInteract_Implementation(const FUTInteractionInfo& Info) const override;
	virtual void Interact_Implementation(const FUTInteractionInfo& Info) override;
	virtual FText GetInteractionPrompt_Implementation(const FUTInteractionInfo& Info) const override;
	virtual void GetInteractionOptions_Implementation(const FUTInteractionInfo& Info, TArray<FUTInteractionOption>& OutOptions) const override;
	virtual int32 GetInteractionPriority_Implementation() const override;

	// -- IUTSaveable -----------------------------------------------------------
	virtual FName GetSaveStableId_Implementation() const override;
	virtual FInstancedStruct CaptureState_Implementation() const override;
	virtual bool RestoreState_Implementation(const FInstancedStruct& State) override;

protected:
	UFUNCTION()
	void OnRep_FuseInstalled();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Configured identity for the save system (never derived from runtime names). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuietRoad|Save")
	FName SaveStableId = TEXT("QuietRoad.Relay.FuseBox01");
};
