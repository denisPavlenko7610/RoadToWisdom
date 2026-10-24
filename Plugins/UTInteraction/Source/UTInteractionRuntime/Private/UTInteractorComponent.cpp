// UT Templates - interactor component implementation.
#include "UTInteractorComponent.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
static TAutoConsoleVariable<int32> CVarUTDebugInteraction(TEXT("ut.Debug.Interaction"), 0, TEXT("Enable toolkit interaction diagnostics."), ECVF_Cheat);
#endif

#include "UTInteractable.h"
#include "UTInteractionLog.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
	// cm of range beyond InteractionDistance * ServerRangeValidationScale the server still
	// accepts, absorbing client/server latency.
	constexpr float ServerRangeSlackCm = 100.f;

	// How strongly angular misalignment (1 - cos) counts against normalized distance when
	// ranking candidates; higher weights keep look-at targets ahead of closer off-axis ones.
	constexpr float AngularMissWeight = 2.f;
}

UUTInteractorComponent::UUTInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UUTInteractorComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
}

void UUTInteractorComponent::ViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const AActor* Owner = GetOwner();
	OutLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	OutRotation = Owner ? Owner->GetActorRotation() : FRotator::ZeroRotator;
	if (!Owner) {
		return;
	}

	// Pawn view gives the camera point of view; other actors use eyes/forward.
	if (const APawn* Pawn = Cast<APawn>(Owner)) {
		if (const AController* Controller = Pawn->GetController()) {
			Controller->GetPlayerViewPoint(OutLocation, OutRotation);
			return;
		}
	}
	Owner->GetActorEyesViewPoint(OutLocation, OutRotation);
}

void UUTInteractorComponent::UpdateFocus()
{
	AActor* Best = FindBestInteractable();

	// A same-actor focus can still need a refresh: Open -> Close, Locked -> Unlock,
	// Hold -> Cancel are all prompt changes on an unchanged target.
	FText Prompt;
	if (Best) {
		FUTInteractionInfo Info;
		Info.Interactor = GetOwner();

		TArray<FUTInteractionOption> Options;
		IUTInteractable::Execute_GetInteractionOptions(Best, Info, Options);
		for (const FUTInteractionOption& Option : Options) {
			if (!Option.Prompt.IsEmpty()) {
				Prompt = Option.Prompt;
				break;
			}
		}
		if (Prompt.IsEmpty()) {
			Prompt = IUTInteractable::Execute_GetInteractionPrompt(Best, Info);
		}
	}

	// A running hold is aborted as soon as the view moves to a different target.
	if (bHolding && Best != HoldTarget.Get()) {
		CancelHoldInteract();
	}

	SetFocus(Best, Prompt);
}

bool UUTInteractorComponent::TryInteract()
{
	return TryInteractOption(NAME_None);
}

bool UUTInteractorComponent::TryInteractOption(FName OptionId)
{
	UpdateFocus();
	AActor* Best = FocusedActor.Get();
	if (!Best || bHolding) {
		return false;
	}

	FUTInteractionInfo Info;
	Info.Interactor = GetOwner();

	TArray<FUTInteractionOption> Options;
	IUTInteractable::Execute_GetInteractionOptions(Best, Info, Options);

	const FUTInteractionOption* Chosen = nullptr;
	for (const FUTInteractionOption& Option : Options) {
		if (OptionId.IsNone() || Option.OptionId == OptionId) {
			Chosen = &Option;
			break;
		}
	}

	if (!Chosen) {
		return false;
	}

	if (Chosen->HoldDuration > 0.f) {
		StartHold(Best, *Chosen);
		return true;
	}

	RequestInteract(Best, Chosen->OptionId);
	return true;
}

bool UUTInteractorComponent::FindOfferedOption(AActor* Target, FName OptionId, FUTInteractionOption& OutOption) const
{
	if (!IsValid(Target) || !Target->Implements<UUTInteractable>()) {
		return false;
	}

	FUTInteractionInfo Info;
	Info.Interactor = GetOwner();
	Info.OptionId = OptionId;

	TArray<FUTInteractionOption> Options;
	IUTInteractable::Execute_GetInteractionOptions(Target, Info, Options);
	for (const FUTInteractionOption& Option : Options) {
		if (OptionId.IsNone() || Option.OptionId == OptionId) {
			OutOption = Option;
			return true;
		}
	}
	return false;
}

bool UUTInteractorComponent::CancelInteractionWithTarget(AActor* Target)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Target || ServerHoldTarget.Get() != Target) {
		return false;
	}
	const FName Option = ServerHoldOptionId;
	if (bHolding && HoldTarget.Get() == Target) {
		FinishHold(false);
	}
	else {
		ServerCancelHold();
	}
	ClientHoldEnded(Target, Option, false);
	return true;
}

bool UUTInteractorComponent::InteractWithTarget(AActor* Target, FName OptionId)
{
	FUTInteractionOption Offered;
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || ServerHoldTarget.IsValid() || !FindOfferedOption(Target, OptionId, Offered) || !ServerValidateRequest(Target, Offered.OptionId)) {
		return false;
	}
	if (Offered.HoldDuration > 0.f) {
		ServerHoldStart_Implementation(Target, Offered.OptionId);
	}
	else {
		ExecuteOption(Target, Offered.OptionId);
	}
	return true;
}

void UUTInteractorComponent::RequestInteract(AActor* Target, FName OptionId)
{
	if (!Target) {
		return;
	}

	// Client: send the intent; the server re-validates target, range and CanInteract.
	if (!GetOwner()->HasAuthority()) {
		ServerInteract(Target, OptionId);
		return;
	}

	ServerInteract_Implementation(Target, OptionId);
}

bool UUTInteractorComponent::ServerInteract_Validate(AActor* Target, FName OptionId)
{
	// A valid request can race with target destruction. Reject in implementation, not by disconnecting the sender.
	return true;
}

bool UUTInteractorComponent::ServerValidateRequest(AActor* Target, FName OptionId) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(Target) || !Owner->HasAuthority() || Target->GetWorld() != GetWorld() || !Target->Implements<UUTInteractable>()) {
		return false;
	}

	// The requested behavior must actually be offered right now - a client cannot
	// invoke an option the target stopped offering (e.g. after the lock opened).
	FUTInteractionOption Offered;
	if (!FindOfferedOption(Target, OptionId, Offered)) {
		return false;
	}

	// The client trace is not trusted: re-check the range from the server's view.
	const float AllowedRange = InteractionDistance * ServerRangeValidationScale + ServerRangeSlackCm;
	if (FVector::Dist(Owner->GetActorLocation(), Target->GetActorLocation()) > AllowedRange) {
		UE_LOG(
			LogUTInteraction, Warning,
			TEXT("ServerInteract rejected '%s': out of range."), *Target->GetName());
		return false;
	}

	FUTInteractionInfo Info;
	Info.Interactor = const_cast<AActor*>(Owner);
	Info.OptionId = OptionId;
	return IUTInteractable::Execute_CanInteract(Target, Info);
}

void UUTInteractorComponent::ServerInteract_Implementation(AActor* Target, FName OptionId)
{
	FUTInteractionOption Offered;
	if (FindOfferedOption(Target, OptionId, Offered) && Offered.HoldDuration <= 0.f && ServerValidateRequest(Target, Offered.OptionId)) {
		ExecuteOption(Target, Offered.OptionId);
	}
}

bool UUTInteractorComponent::ServerHoldStart_Validate(AActor* Target, FName OptionId)
{
	// A valid request can race with target destruction. Reject in implementation, not by disconnecting the sender.
	return true;
}

void UUTInteractorComponent::ServerHoldStart_Implementation(AActor* Target, FName OptionId)
{
	// The server runs the hold: option validity, duration and re-validation all happen
	// here, so a client cannot call the completion early (it does not even exist).
	FUTInteractionOption Offered;
	if (ServerHoldTarget.IsValid() || !FindOfferedOption(Target, OptionId, Offered) || Offered.HoldDuration <= 0.f || !ServerValidateRequest(Target, OptionId)) {
		ClientHoldEnded(Target, OptionId, false);
		return;
	}

	ServerHoldTarget = Target;
	ServerHoldOptionId = Offered.OptionId;

	FUTHoldStartedSignature StartedEvent = OnHoldStarted;
	StartedEvent.Broadcast(Target, Offered.OptionId);

	GetWorld()->GetTimerManager().SetTimer(
		ServerHoldTimer,
		FTimerDelegate::CreateUObject(this, &UUTInteractorComponent::ServerFinishHold),
		Offered.HoldDuration, false);
}

void UUTInteractorComponent::ServerHoldCancel_Implementation()
{
	ServerCancelHold();
}

void UUTInteractorComponent::ServerStartHold(AActor* Target, const FUTInteractionOption& Option)
{
	// Authority path (listen server / standalone): the same contract runs locally.
	if (!ServerValidateRequest(Target, Option.OptionId)) {
		return;
	}
	ServerHoldTarget = Target;
	ServerHoldOptionId = Option.OptionId;
	GetWorld()->GetTimerManager().SetTimer(
		ServerHoldTimer,
		FTimerDelegate::CreateUObject(this, &UUTInteractorComponent::ServerFinishHold),
		Option.HoldDuration, false);
}

void UUTInteractorComponent::ServerCancelHold()
{
	if (GetWorld()) {
		GetWorld()->GetTimerManager().ClearTimer(ServerHoldTimer);
	}
	const bool bWasHolding = ServerHoldTarget.IsValid();
	AActor* Target = ServerHoldTarget.Get();
	const FName OptionId = ServerHoldOptionId;
	ServerHoldTarget = nullptr;
	ServerHoldOptionId = NAME_None;
	if (bWasHolding) {
		FUTHoldEndedSignature CancelledEvent = OnHoldCancelled;
		CancelledEvent.Broadcast(Target, OptionId);
	}
}

void UUTInteractorComponent::ServerFinishHold()
{
	if (!ServerHoldTarget.IsValid()) {
		ServerCancelHold();
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(ServerHoldTimer);
	AActor* Target = ServerHoldTarget.Get();
	const FName OptionId = ServerHoldOptionId;
	ServerHoldTarget = nullptr;
	ServerHoldOptionId = NAME_None;

	const bool bCompleted = Target && ServerValidateRequest(Target, OptionId);
	if (bCompleted) {
		ExecuteOption(Target, OptionId);
	}
	if (bHolding) {
		FinishHold(bCompleted);
	}
	else {
		if (bCompleted) {
			OnHoldCompleted.Broadcast(Target, OptionId);
		}
		else {
			OnHoldCancelled.Broadcast(Target, OptionId);
		}
	}
	ClientHoldEnded(Target, OptionId, bCompleted);
}

void UUTInteractorComponent::ClientHoldEnded_Implementation(AActor* Target, FName OptionId, bool bCompleted)
{
	if (bHolding && HoldTarget.Get() == Target && HoldOption.OptionId == OptionId) {
		FinishHold(bCompleted);
	}
}

void UUTInteractorComponent::ExecuteOption(AActor* Target, FName OptionId)
{
	if (!IsValid(Target) || !Target->Implements<UUTInteractable>()) {
		return;
	}

	FUTInteractionInfo Info;
	Info.Interactor = GetOwner();
	Info.OptionId = OptionId;

	if (!IUTInteractable::Execute_CanInteract(Target, Info)) {
		return;
	}
	IUTInteractable::Execute_Interact(Target, Info);
}

void UUTInteractorComponent::StartHold(AActor* Target, const FUTInteractionOption& Option)
{
	bHolding = true;
	HoldElapsed = 0.f;
	HoldOption = Option;
	HoldTarget = Target;
	SetComponentTickEnabled(true);

	// The timing UI is local; the authoritative hold lives on the server (or locally on
	// a listen server, where the same server contract runs in this component).
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		ServerHoldStart(Target, Option.OptionId);
	}
	else {
		ServerStartHold(Target, Option);
	}

	FUTHoldStartedSignature StartedEvent = OnHoldStarted;
	StartedEvent.Broadcast(Target, Option.OptionId);
}

void UUTInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bHolding) {
		SetComponentTickEnabled(false);
		return;
	}

	AActor* Target = HoldTarget.Get();
	if (!Target) {
		CancelHoldInteract();
		return;
	}

	HoldElapsed += DeltaTime;
	const float Duration = FMath::Max(HoldOption.HoldDuration, KINDA_SMALL_NUMBER);
	const float Normalized = FMath::Clamp(HoldElapsed / Duration, 0.f, 1.f);

	if (HoldElapsed < HoldOption.HoldDuration) {
		FUTHoldProgressSignature ProgressEvent = OnHoldProgress;
		ProgressEvent.Broadcast(Normalized, HoldOption.HoldDuration - HoldElapsed);
		return;
	}

	// Wait for the authoritative timer / acknowledgement, even when the UI reaches 100%.
	OnHoldProgress.Broadcast(1.f, 0.f);
}

void UUTInteractorComponent::CancelHoldInteract()
{
	if (!bHolding) {
		if (GetOwner() && GetOwner()->HasAuthority() && ServerHoldTarget.IsValid()) {
			AActor* Target = ServerHoldTarget.Get();
			const FName Option = ServerHoldOptionId;
			ServerCancelHold();
			ClientHoldEnded(Target, Option, false);
		}
		return;
	}
	FinishHold(/*bCompleted*/ false);
}

void UUTInteractorComponent::FinishHold(bool bCompleted)
{
	AActor* Target = HoldTarget.Get();
	const FName OptionId = HoldOption.OptionId;

	// Cancel the authoritative hold as well (client cancel, focus loss, EndPlay).
	if (GetOwner() && GetOwner()->HasAuthority()) {
		ServerCancelHold();
	}
	else if (!bCompleted) {
		ServerHoldCancel();
	}

	bHolding = false;
	HoldElapsed = 0.f;
	HoldOption = FUTInteractionOption();
	HoldTarget = nullptr;
	SetComponentTickEnabled(false);

	if (bCompleted) {
		FUTHoldEndedSignature CompletedEvent = OnHoldCompleted;
		CompletedEvent.Broadcast(Target, OptionId);
	}
	else {
		FUTHoldEndedSignature CancelledEvent = OnHoldCancelled;
		CancelledEvent.Broadcast(Target, OptionId);
	}
}

void UUTInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelHoldInteract();
	ServerCancelHold();
	SetFocus(nullptr, FText::GetEmpty());
	Super::EndPlay(EndPlayReason);
}

AActor* UUTInteractorComponent::FindBestInteractable() const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) {
		return nullptr;
	}

	FVector TraceStart = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	ViewPoint(TraceStart, ViewRotation);
	const FVector TraceDirection = ViewRotation.Vector();
	const FVector TraceEnd = TraceStart + TraceDirection * InteractionDistance;

#if !UE_BUILD_SHIPPING
	if (CVarUTDebugInteraction.GetValueOnGameThread() != 0) {
		DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, false, 0.f);
		DrawDebugSphere(World, TraceEnd, InteractionRadius, 12, FColor::Cyan, false, 0.f);
	}
#endif
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UTInteract), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(Owner);

	struct FUTCandidateScore
	{
		AActor* Actor = nullptr;
		float AngularMiss = 0.f;
		float NormalizedDistance = 0.f;
		int32 Priority = 0;
	};

	TArray<FUTCandidateScore> Candidates;
	auto Consider = [&](AActor* Actor, float Distance, const FVector& ToActor) {
		if (!Actor || !Actor->Implements<UUTInteractable>()) {
			return;
		}
		FUTCandidateScore& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Actor = Actor;
		Candidate.NormalizedDistance = Distance / FMath::Max(InteractionDistance, 1.f);
		Candidate.AngularMiss = 1.f - FVector::DotProduct(TraceDirection, ToActor.GetSafeNormal());
		Candidate.Priority = IUTInteractable::Execute_GetInteractionPriority(Actor);
	};

	if (InteractionRadius > 0.f) {
		// The sphere sweep returns everything roughly along the ray; scoring decides,
		// not the raw hit order. Priority first, then angular closeness, then distance.
		FCollisionShape Shape = FCollisionShape::MakeSphere(InteractionRadius);
		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, TraceStart, TraceEnd, FQuat::Identity, InteractionTraceChannel, Shape, Params);
		TSet<AActor*> Seen;
		for (const FHitResult& Hit : Hits) {
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || Seen.Contains(HitActor)) {
				continue;
			}
			Seen.Add(HitActor);
			const float Distance = FVector::Dist(TraceStart, HitActor->GetActorLocation());
			Consider(HitActor, Distance, HitActor->GetActorLocation() - TraceStart);
		}
	}
	else {
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, InteractionTraceChannel, Params)) {
			AActor* HitActor = Hit.GetActor();
			if (HitActor) {
				Consider(HitActor, Hit.Distance, HitActor->GetActorLocation() - TraceStart);
			}
		}
	}

	if (Candidates.IsEmpty()) {
		return nullptr;
	}

	Candidates.Sort(
		[](const FUTCandidateScore& A, const FUTCandidateScore& B) {
			if (A.Priority != B.Priority) {
				return A.Priority > B.Priority;
			}
			// Angular miss (1 - cos) dominates distance: a target the player looks at
			// beats a closer one off to the side.
			return (A.AngularMiss * AngularMissWeight + A.NormalizedDistance) < (B.AngularMiss * AngularMissWeight + B.NormalizedDistance);
		});

#if !UE_BUILD_SHIPPING
	if (CVarUTDebugInteraction.GetValueOnGameThread() != 0) {
		for (const FUTCandidateScore& Candidate : Candidates) {
			DrawDebugString(World, Candidate.Actor->GetActorLocation() + FVector(0, 0, 60),
							FString::Printf(TEXT("%s priority=%d score=%.2f range=%.0f/%.0f"), *Candidate.Actor->GetName(), Candidate.Priority,
											Candidate.AngularMiss * AngularMissWeight + Candidate.NormalizedDistance, Candidate.NormalizedDistance * InteractionDistance, InteractionDistance),
							nullptr, Candidate.Actor == Candidates[0].Actor ? FColor::Green : FColor::Yellow, 0.f);
		}
	}
#endif
	return Candidates[0].Actor;
}

void UUTInteractorComponent::SetFocus(AActor* NewFocus, const FText& NewPrompt)
{
	AActor* Previous = FocusedActor.Get();
	const bool bChanged = Previous != NewFocus || !FocusedPrompt.EqualTo(NewPrompt);
	FocusedActor = NewFocus;
	FocusedPrompt = NewPrompt;
	if (bChanged) {
		FUTFocusChangedSignature FocusEvent = OnFocusChanged;
		FocusEvent.Broadcast(NewFocus, NewPrompt);
	}
}
