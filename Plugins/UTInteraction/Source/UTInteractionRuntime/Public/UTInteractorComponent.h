// UT Templates - trace-based interactor component.
#pragma once

#include "Components/ActorComponent.h"
#include "UTInteractionTypes.h"
#include "UTInteractorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTFocusChangedSignature, AActor*, FocusedActor, FText, Prompt);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTHoldStartedSignature, AActor*, Target, FName, OptionId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTHoldProgressSignature, float, NormalizedProgress, float, RemainingSeconds);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTHoldEndedSignature, AActor*, Target, FName, OptionId);

/**
 * The "using" side of interactions: traces along the owner's view, scores the candidate
 * set (priority, then angle + distance) and drives the best IUTInteractable.
 *
 * Focus/prompt refresh: the prompt is re-evaluated on every UpdateFocus even when the
 * focused actor is unchanged - the same door legitimately changes prompt between
 * Open/Close and Locked/Unlock. OnFocusChanged fires whenever the actor OR the prompt
 * moves, so UI never shows a stale verb.
 *
 * Options: TryInteract triggers the best target's first offered option; TryInteractOption
 * targets one by id. Options with HoldDuration > 0 run the built-in hold state machine
 * (OnHoldStarted / OnHoldProgress / OnHoldCompleted / OnHoldCancelled).
 *
 * Multiplayer: focus tracing is local and only feeds UI. Taps are requested through a
 * server RPC; holds run a server-side hold contract - the client reports hold
 * start/cancel, the server verifies the option is currently offered, takes the
 * option's HoldDuration itself (a client cannot shorten it) and executes on its own
 * timer. The server re-validates target, range and CanInteract at both start and
 * execution. Client traces are never trusted for gameplay. The component ticks only
 * while the local hold runs.
 *
 * Deliberately input-agnostic: bind your Enhanced Input action to TryInteract.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTINTERACTIONRUNTIME_API UUTInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Interaction")
	FUTFocusChangedSignature OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "UT|Interaction")
	FUTHoldStartedSignature OnHoldStarted;

	/** NormalizedProgress 0..1, RemainingSeconds in server-style seconds until completion. */
	UPROPERTY(BlueprintAssignable, Category = "UT|Interaction")
	FUTHoldProgressSignature OnHoldProgress;

	/** Fired when a hold runs to completion (the interaction executes as part of this). */
	UPROPERTY(BlueprintAssignable, Category = "UT|Interaction")
	FUTHoldEndedSignature OnHoldCompleted;

	/** Fired when a hold is aborted early (focus moved, released, target invalidated). */
	UPROPERTY(BlueprintAssignable, Category = "UT|Interaction")
	FUTHoldEndedSignature OnHoldCancelled;

	UUTInteractorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "0.0"))
	float InteractionDistance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

	/** Sphere sweep radius; 0 uses a line trace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "0.0"))
	float InteractionRadius = 20.f;

	/**
	 * Server-side range check multiplier for requested interactions: the server accepts
	 * requests up to InteractionDistance * this (+ slack) to absorb client/server latency.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "1.0"))
	float ServerRangeValidationScale = 1.5f;

	/** Re-trace and broadcast OnFocusChanged whenever the actor or the prompt changed. Call on tick or when the view moves. */
	UFUNCTION(BlueprintCallable, Category = "UT|Interaction")
	void UpdateFocus();

	/**
	 * Interact with the best candidate's first offered option (starts a hold when that
	 * option asks for one). Returns false when nothing usable was found.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Interaction")
	bool TryInteract();

	/** Like TryInteract, but for a specific option id on the best candidate. */
	UFUNCTION(BlueprintCallable, Category = "UT|Interaction")
	bool TryInteractOption(FName OptionId);

	/** Server-only explicit target intent for AI/adapters. Reuses offered-option, range and CanInteract checks.
	 * Returns acceptance, not completion. Holds finish through OnHoldCompleted/Cancelled. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UT|Interaction")
	bool InteractWithTarget(AActor* Target, FName OptionId = NAME_None);

	/** Server-only cancellation scoped to this target; adapters cannot cancel another activity by mistake. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UT|Interaction")
	bool CancelInteractionWithTarget(AActor* Target);

	/** Current focused interactable without re-tracing. */
	UFUNCTION(BlueprintPure, Category = "UT|Interaction")
	AActor* GetFocusedActor() const
	{
		return FocusedActor.Get();
	}

	/** Cached prompt selected from the currently offered options. Local UI does not need another query. */
	UFUNCTION(BlueprintPure, Category = "UT|Interaction")
	FText GetFocusedPrompt() const { return FocusedPrompt; }
	/** True while a hold interaction is running. */
	UFUNCTION(BlueprintPure, Category = "UT|Interaction")
	bool IsHoldingInteract() const
	{
		return bHolding;
	}

	/** Abort a running hold (fires OnHoldCancelled). */
	UFUNCTION(BlueprintCallable, Category = "UT|Interaction")
	void CancelHoldInteract();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

private:
	friend class FUTInteractionAuthorityTest;
	/** Server acknowledges gameplay completion; local UI timing never completes gameplay. */
	UFUNCTION(Client, Reliable)
	void ClientHoldEnded(AActor* Target, FName OptionId, bool bCompleted);

	/** One candidate found by the trace, scored for selection. */
	struct FUTCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		float Distance = 0.f;
		float AngleCos = 1.f;
		int32 Priority = 0;
	};

	AActor* FindBestInteractable() const;

	void ViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Request execution on the interactor's authority path (local exec or server RPC). */
	void RequestInteract(AActor* Target, FName OptionId);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerInteract(AActor* Target, FName OptionId);

	/** Client reports a hold start; the server owns the duration and executes itself. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerHoldStart(AActor* Target, FName OptionId);

	UFUNCTION(Server, Reliable)
	void ServerHoldCancel();

	/** Option lookup against the target's currently offered set. None -> first option. */
	bool FindOfferedOption(AActor* Target, FName OptionId, FUTInteractionOption& OutOption) const;

	bool ServerValidateRequest(AActor* Target, FName OptionId) const;

	void ExecuteOption(AActor* Target, FName OptionId);

	/** Authority-side hold state; the local (client) hold is cosmetic timing only. */
	void ServerStartHold(AActor* Target, const FUTInteractionOption& Option);

	void ServerCancelHold();

	void ServerFinishHold();

	FTimerHandle ServerHoldTimer;
	TWeakObjectPtr<AActor> ServerHoldTarget;
	FName ServerHoldOptionId;

	void SetFocus(AActor* NewFocus, const FText& NewPrompt);

	void StartHold(AActor* Target, const FUTInteractionOption& Option);

	void FinishHold(bool bCompleted);

	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedPrompt;

	bool bHolding = false;
	float HoldElapsed = 0.f;
	FUTInteractionOption HoldOption;
	TWeakObjectPtr<AActor> HoldTarget;
};
