// UT Templates - status effects implementation.
#include "UTStatusEffectsComponent.h"

#include "Net/UnrealNetwork.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "UTStatusEffect.h"
#include "UTStatsComponent.h"
#include "UTHealthComponent.h"

namespace
{
	// Seconds; floor for an effect's damage period so a bad asset cannot spam damage ticks.
	constexpr float MinDamagePeriodSeconds = 0.05f;
	// Seconds; interval of the replicated-display refresh while any effect is active.
	constexpr float DisplayRefreshIntervalSeconds = 1.f;
}

UUTStatusEffectsComponent::UUTStatusEffectsComponent()
{
	SetIsReplicatedByDefault(true);
}

void UUTStatusEffectsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTStatusEffectsComponent, ReplicatedDisplays);
}

void UUTStatusEffectsComponent::OnRep_ReplicatedDisplays()
{
	FUTReplicatedEffectsChangedSignature ChangedEvent = OnReplicatedEffectsChanged;
	ChangedEvent.Broadcast();
}

void UUTStatusEffectsComponent::RefreshReplicatedDisplays()
{
	ReplicatedDisplays.Reset();
	for (const TPair<int32, FUTActiveEffect>& Pair : Active) {
		const UUTStatusEffect* Def = Pair.Value.Def;
		if (!Def) {
			continue;
		}
		FUTActiveEffectDisplay Display;
		Display.EffectName = Def->EffectName;
		Display.EffectTags = Def->EffectTags;
		Display.Duration = Def->Duration;
		Display.Remaining = GetWorld()
								? FMath::Max(0.f, GetWorld()->GetTimerManager().GetTimerRemaining(Pair.Value.ExpiryTimer))
								: 0.f;
		Display.Handle = Pair.Key;
		ReplicatedDisplays.Add(Display);
	}
}

int32 UUTStatusEffectsComponent::ApplyEffect(UUTStatusEffect* Effect, AActor* Source)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return 0;
	}

	if (!Effect) {
		return 0;
	}

	// Stacking policy is evaluated over active instances of the same definition.
	for (const TPair<int32, FUTActiveEffect>& Pair : Active) {
		if (Pair.Value.Def != Effect) {
			continue;
		}

		const int32 ExistingHandle = Pair.Key;
		UWorld* World = GetWorld();
		switch (Effect->Stacking) {
		case EUTStatusEffectStacking::Ignore:
			return 0;

		case EUTStatusEffectStacking::Refresh:
			if (World && Effect->Duration > 0.f) {
				FUTActiveEffect* Existing = Active.Find(ExistingHandle);
				FTimerDelegate ExpiryDelegate;
				ExpiryDelegate.BindUObject(this, &UUTStatusEffectsComponent::HandleExpiry, ExistingHandle);
				World->GetTimerManager().SetTimer(Existing->ExpiryTimer, ExpiryDelegate, Effect->Duration, false);
			}
			return ExistingHandle;

		case EUTStatusEffectStacking::Stack:
		default:
			break;
		}
		break;
	}

	return ApplyEffectInternal(Effect, Source);
}

int32 UUTStatusEffectsComponent::ApplyEffectInternal(UUTStatusEffect* Effect, AActor* Source)
{
	UUTStatsComponent* Stats = ResolveStats();

	FUTActiveEffect Instance;
	Instance.Handle = NextHandle++;
	Instance.Def = Effect;
	Instance.Source = Source;

	if (Stats) {
		for (const FUTStatusStatModifier& Modifier : Effect->StatModifiers) {
			FUTEffectStatHandle& StatHandle = Instance.StatHandles.AddDefaulted_GetRef();
			StatHandle.StatId = Modifier.StatId;
			StatHandle.Modifier = Stats->AddModifier(Modifier.StatId, Modifier.Op, Modifier.Magnitude, Effect);
		}
	}

	UWorld* World = GetWorld();
	if (World) {
		if (Effect->Duration > 0.f) {
			FTimerDelegate ExpiryDelegate;
			ExpiryDelegate.BindUObject(this, &UUTStatusEffectsComponent::HandleExpiry, Instance.Handle);
			World->GetTimerManager().SetTimer(Instance.ExpiryTimer, ExpiryDelegate, Effect->Duration, false);
		}

		if (Effect->PeriodicDamage.Amount > 0.f) {
			FTimerDelegate DamageDelegate;
			DamageDelegate.BindUObject(this, &UUTStatusEffectsComponent::HandlePeriodicDamage, Instance.Handle);
			World->GetTimerManager().SetTimer(
				Instance.DamageTimer, DamageDelegate,
				FMath::Max(MinDamagePeriodSeconds, Effect->DamagePeriod), true);
		}
	}

	const int32 NewHandle = Instance.Handle;
	Active.Add(NewHandle, MoveTemp(Instance));

	FUTEffectAppliedSignature AppliedEvent = OnEffectApplied;
	AppliedEvent.Broadcast(Effect, NewHandle);

	RefreshReplicatedDisplays();

	if (World && !World->GetTimerManager().IsTimerActive(DisplayRefreshTimer)) {
		World->GetTimerManager().SetTimer(
			DisplayRefreshTimer,
			FTimerDelegate::CreateUObject(this, &UUTStatusEffectsComponent::RefreshReplicatedDisplays), DisplayRefreshIntervalSeconds, true);
	}
	return NewHandle;
}

void UUTStatusEffectsComponent::RemoveEffect(int32 Handle)
{
	RemoveEffectInternal(Handle);
}

void UUTStatusEffectsComponent::RemoveEffectsFromSource(AActor* Source)
{
	const TWeakObjectPtr<AActor> WeakSource(Source);
	TArray<int32, TInlineAllocator<8>> Handles;
	for (const TPair<int32, FUTActiveEffect>& Pair : Active) {
		if (Pair.Value.Source == WeakSource) {
			Handles.Add(Pair.Key);
		}
	}
	for (const int32 Handle : Handles) {
		RemoveEffectInternal(Handle);
	}
}

void UUTStatusEffectsComponent::RemoveEffectsByTag(const FGameplayTagContainer& Tags)
{
	TArray<int32, TInlineAllocator<8>> Handles;
	for (const TPair<int32, FUTActiveEffect>& Pair : Active) {
		if (Pair.Value.Def && Pair.Value.Def->EffectTags.HasAny(Tags)) {
			Handles.Add(Pair.Key);
		}
	}
	for (const int32 Handle : Handles) {
		RemoveEffectInternal(Handle);
	}
}

bool UUTStatusEffectsComponent::IsAffectedBy(FGameplayTag Tag) const
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return ReplicatedDisplays.ContainsByPredicate(
			[Tag](const FUTActiveEffectDisplay& Display) {
				return Display.EffectTags.HasTag(Tag);
			});
	}
	for (const TPair<int32, FUTActiveEffect>& Pair : Active) {
		if (Pair.Value.Def && Pair.Value.Def->EffectTags.HasTag(Tag)) {
			return true;
		}
	}
	return false;
}

int32 UUTStatusEffectsComponent::GetActiveEffectCount() const
{
	return GetOwner() && !GetOwner()->HasAuthority() ? ReplicatedDisplays.Num() : Active.Num();
}

void UUTStatusEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Silent teardown: revert stat modifiers, clear timers, no removal events.
	UUTStatsComponent* Stats = ResolveStats();
	for (TPair<int32, FUTActiveEffect>& Pair : Active) {
		ReleaseInstance(Pair.Value, Stats);
	}
	Active.Reset();
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(DisplayRefreshTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UUTStatusEffectsComponent::ReleaseInstance(FUTActiveEffect& Instance, UUTStatsComponent* Stats)
{
	if (UWorld* World = GetWorld()) {
		World->GetTimerManager().ClearTimer(Instance.ExpiryTimer);
		World->GetTimerManager().ClearTimer(Instance.DamageTimer);
	}
	if (Stats) {
		for (const FUTEffectStatHandle& StatHandle : Instance.StatHandles) {
			Stats->RemoveModifier(StatHandle.StatId, StatHandle.Modifier);
		}
	}
	Instance.StatHandles.Reset();
}

void UUTStatusEffectsComponent::RemoveEffectInternal(int32 Handle)
{
	FUTActiveEffect Instance;
	if (!Active.RemoveAndCopyValue(Handle, Instance)) {
		return;
	}

	ReleaseInstance(Instance, ResolveStats());

	FUTEffectRemovedSignature RemovedEvent = OnEffectRemoved;
	RemovedEvent.Broadcast(Instance.Def, Handle);

	RefreshReplicatedDisplays();

	// One-per-second refresh keeps the client's Remaining countdown honest while any
	// effect is active; the timer stops itself when nothing is left to display.
	UWorld* World = GetWorld();
	if (World) {
		if (!Active.IsEmpty() && !World->GetTimerManager().IsTimerActive(DisplayRefreshTimer)) {
			World->GetTimerManager().SetTimer(
				DisplayRefreshTimer,
				FTimerDelegate::CreateUObject(this, &UUTStatusEffectsComponent::RefreshReplicatedDisplays), DisplayRefreshIntervalSeconds, true);
		}
		else if (Active.IsEmpty()) {
			World->GetTimerManager().ClearTimer(DisplayRefreshTimer);
		}
	}
}

void UUTStatusEffectsComponent::HandleExpiry(int32 Handle)
{
	RemoveEffectInternal(Handle);
}

void UUTStatusEffectsComponent::HandlePeriodicDamage(int32 Handle)
{
	const FUTActiveEffect* Instance = Active.Find(Handle);
	if (!Instance || !Instance->Def) {
		return;
	}

	if (UUTHealthComponent* Health = ResolveHealth()) {
		FUTDamage Damage = Instance->Def->PeriodicDamage;
		Damage.Instigator = Instance->Source.Get();
		Damage.Causer = GetOwner();
		Health->ApplyHealthDamage(Damage);
	}
}

UUTStatsComponent* UUTStatusEffectsComponent::ResolveStats()
{
	if (!CachedStats.IsValid()) {
		CachedStats = GetOwner() ? GetOwner()->FindComponentByClass<UUTStatsComponent>() : nullptr;
	}
	return CachedStats.Get();
}

UUTHealthComponent* UUTStatusEffectsComponent::ResolveHealth()
{
	if (!CachedHealth.IsValid()) {
		CachedHealth = GetOwner() ? GetOwner()->FindComponentByClass<UUTHealthComponent>() : nullptr;
	}
	return CachedHealth.Get();
}
