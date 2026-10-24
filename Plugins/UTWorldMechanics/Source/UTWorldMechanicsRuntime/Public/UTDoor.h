// UT Templates - door actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "UTSaveable.h"
#include "UTDoor.generated.h"

/** Persisted door state (see IUTSaveable). */
USTRUCT(BlueprintType)
struct UTWORLDMECHANICSRUNTIME_API FUTDoorSaveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bOpen = false;
};

/** One initial-only pose snapshot. Existing clients still animate replicated target changes. */
USTRUCT()
struct FUTDoorInitialPose
{
	GENERATED_BODY()
	UPROPERTY()
	bool bOpen = false;
	UPROPERTY()
	float Alpha = 0.f;
};

UENUM(BlueprintType)
enum class EUTDoorMoveMode : uint8
{
	/** Rotate the mesh around the hinge (actor origin) by OpenAngle. */
	Rotate,
	/** Translate the mesh by SlideOffset (local space). */
	Slide
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTDoorOpenedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTDoorClosedSignature);

/**
 * Door actor with a hinge root and a door mesh child. The actor origin IS the hinge:
 * place the pivot where the hinge should be, offset the mesh in the child.
 *
 * Movement interpolates between the closed and open transforms with smooth-step easing;
 * OnOpened/OnClosed fire when the motion finishes, not when it is requested.
 *
 * Composability rule: no interaction or lock logic in here. Implement IUTInteractable
 * on a Blueprint child (or a small bridge component) and call Open/Close/Toggle from it.
 *
 * Multiplayer: server-authoritative. Open/Close/SetOpen run on the authority; the open
 * state is replicated and clients run the same eased animation locally, so late joiners
 * see the door already open and everyone sees it move. OnOpened/OnClosed fire when the
 * local motion finishes (server and clients).
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTDoor : public AActor, public IUTSaveable
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Door")
	FUTDoorOpenedSignature OnOpened;

	UPROPERTY(BlueprintAssignable, Category = "UT|Door")
	FUTDoorClosedSignature OnClosed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Door")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** Rotation (deg) applied around the hinge when opening; negative swings the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Door", meta = (EditCondition = "MoveMode == EUTDoorMoveMode::Rotate"))
	float OpenAngle = -90.f;

	/** Local-space translation applied when opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Door", meta = (EditCondition = "MoveMode == EUTDoorMoveMode::Slide"))
	FVector SlideOffset = FVector(0.f, 100.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Door")
	EUTDoorMoveMode MoveMode = EUTDoorMoveMode::Rotate;

	/** Seconds for a full open or close motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Door", meta = (ClampMin = "0.01"))
	float OpenTime = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Door")
	bool bStartOpen = false;

	/** Persistent identity for the world-state save layer; None means not saved. Use level-scoped ids ("Farm.Level.Chest01" style). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Door", meta = (EditCondition = "true"))
	FName StableId;

	UFUNCTION(BlueprintCallable, Category = "UT|Door")
	void Open();

	UFUNCTION(BlueprintCallable, Category = "UT|Door")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "UT|Door")
	void Toggle() { SetOpen(!bIsOpen); }

	UFUNCTION(BlueprintCallable, Category = "UT|Door")
	void SetOpen(bool bNewOpen);

	UFUNCTION(BlueprintPure, Category = "UT|Door")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "UT|Door")
	bool IsMoving() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- IUTSaveable -----------------------------------------------------------
	/** Configured persistent id; NAME_None excludes the door from world-state saves. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Door")
	FName GetSaveStableId() const;
	virtual FName GetSaveStableId_Implementation() const override { return StableId; }

	virtual FInstancedStruct CaptureState_Implementation() const override;
	virtual bool RestoreState_Implementation(const FInstancedStruct& State) override;

	AUTDoor();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_bIsOpen, VisibleAnywhere, Category = "UT|Door")
	bool bIsOpen = false;
	/** Sent only when a connection first sees this actor, never a per-frame transform stream. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_InitialPose)
	FUTDoorInitialPose InitialPose;
	UFUNCTION()
	void OnRep_InitialPose();
	void ApplyInitialPose();

	UFUNCTION()
	void OnRep_bIsOpen();

	/** 0 = closed, 1 = open; drives the eased interpolation. */
	float Alpha = 0.f;
	float TargetAlpha = 0.f;
	FTransform ClosedRelative;
	FTransform OpenRelative;

	void ApplyDoorTransform();
	void UpdateMotionTargets();
};
