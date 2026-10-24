// UT Templates - named cooldowns implementation.
#include "UTCooldownComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"

void UUTCooldownComponent::StartCooldown(FName CooldownId, float Duration)
{
	UWorld* World = GetWorld();
	if (!World || Duration <= 0.f || CooldownId.IsNone()) {
		return;
	}

	FUTCooldownEntry& Entry = Active.FindOrAdd(CooldownId);
	World->GetTimerManager().ClearTimer(Entry.Handle);
	Entry.Duration = Duration;
	Entry.Handle.Invalidate();

	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &UUTCooldownComponent::HandleCooldownEnded, CooldownId);
	World->GetTimerManager().SetTimer(Entry.Handle, Delegate, Duration, false);

	OnCooldownStarted.Broadcast(CooldownId, Duration);
}

void UUTCooldownComponent::StopCooldown(FName CooldownId)
{
	FUTCooldownEntry* Entry = Active.Find(CooldownId);
	if (!Entry) {
		return;
	}

	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(Entry->Handle);
	}
	Active.Remove(CooldownId);

	OnCooldownEnded.Broadcast(CooldownId);
}

void UUTCooldownComponent::StopAllCooldowns()
{
	TArray<FName, TInlineAllocator<8>> Ids;
	Active.GetKeys(Ids);
	for (const FName& Id : Ids) {
		StopCooldown(Id);
	}
}

bool UUTCooldownComponent::IsCoolingDown(FName CooldownId) const
{
	const FUTCooldownEntry* Entry = Active.Find(CooldownId);
	if (!Entry) {
		return false;
	}

	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(Entry->Handle);
}

float UUTCooldownComponent::GetRemaining(FName CooldownId) const
{
	const FUTCooldownEntry* Entry = Active.Find(CooldownId);
	if (!Entry) {
		return 0.f;
	}

	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(Entry->Handle)) : 0.f;
}

float UUTCooldownComponent::GetDuration(FName CooldownId) const
{
	const FUTCooldownEntry* Entry = Active.Find(CooldownId);
	return Entry && IsCoolingDown(CooldownId) ? Entry->Duration : 0.f;
}

void UUTCooldownComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Teardown is silent: world destruction would otherwise broadcast into dying listeners.
	if (UWorld* World = GetWorld()) {
		for (auto& Pair : Active) {
			World->GetTimerManager().ClearTimer(Pair.Value.Handle);
		}
	}
	Active.Reset();
	Super::EndPlay(EndPlayReason);
}

void UUTCooldownComponent::HandleCooldownEnded(FName CooldownId)
{
	Active.Remove(CooldownId);
	OnCooldownEnded.Broadcast(CooldownId);
}
