// UT Templates - health component implementation.
#include "UTHealthComponent.h"

#include "Net/UnrealNetwork.h"
#include "UTActorDiedMessage.h"
#include "UTMessageBusSubsystem.h"
#include "UTTags.h"

UUTHealthComponent::UUTHealthComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UUTHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTHealthComponent, CurrentHealth);
	DOREPLIFETIME(UUTHealthComponent, MaxHealth);
	DOREPLIFETIME(UUTHealthComponent, bInvulnerable);
	DOREPLIFETIME(UUTHealthComponent, bDead);
}

void UUTHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || GetOwner()->HasAuthority()) {
		CurrentHealth = FMath::Clamp(StartingHealth, 0.f, MaxHealth);
		bDead = CurrentHealth <= 0.f;
	}
	LastSeenHealth = CurrentHealth;
}

void UUTHealthComponent::OnRep_CurrentHealth()
{
	const bool bWasDamage = CurrentHealth < LastSeenHealth;
	BroadcastHealthChange(bWasDamage, LastSeenHealth);
	LastSeenHealth = CurrentHealth;
}

void UUTHealthComponent::OnRep_bDead()
{
	// Replication is state, not events: derive the boundary crossed from the old flag.
	if (bDead) {
		FUTDiedSignature DiedEvent = OnDied;
		DiedEvent.Broadcast();
	}
	else {
		FUTRevivedSignature RevivedEvent = OnRevived;
		RevivedEvent.Broadcast();
	}
}

void UUTHealthComponent::BroadcastHealthChange(bool bWasDamage, float OldHealth)
{
	FUTHealthChange Change;
	Change.bApplied = true;
	Change.bWasDamage = bWasDamage;
	Change.OldHealth = OldHealth;
	Change.NewHealth = CurrentHealth;
	Change.Delta = CurrentHealth - OldHealth;

	FUTHealthChangedSignature ChangedEvent = OnHealthChanged;
	ChangedEvent.Broadcast(Change);
}

void UUTHealthComponent::InitializeHealth(float NewMaxHealth, float NewStartingHealth)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}
	MaxHealth = FMath::Max(0.f, NewMaxHealth);
	CurrentHealth = FMath::Clamp(NewStartingHealth, 0.f, MaxHealth);
	LastSeenHealth = CurrentHealth;
	bDead = CurrentHealth <= 0.f;
}

FUTDamageResult UUTHealthComponent::ApplyDamage_Implementation(const FUTDamageContext& Context)
{
	FUTDamage Damage;
	Damage.Amount = Context.BaseAmount;
	Damage.DamageTypes = Context.DamageTypes;
	Damage.Instigator = Context.Instigator;
	Damage.Causer = Context.Causer;

	const FUTHealthChange Change = ApplyHealthDamage(Damage);
	FUTDamageResult Result;
	Result.bApplied = Change.bApplied;
	Result.bFatal = Change.WasFatal();
	Result.AmountApplied = Change.bApplied ? FMath::Abs(Change.Delta) : 0.f;
	Result.HealthBefore = Change.OldHealth;
	Result.HealthAfter = Change.NewHealth;
	return Result;
}

FUTHealthChange UUTHealthComponent::ApplyHealthDamage(const FUTDamage& Damage)
{
	FUTHealthChange Rejection;
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		// Never apply gameplay state from a client: the server owns damage.
		Rejection.bWasDamage = true;
		Rejection.Damage = Damage;
		return Rejection;
	}
	if (Damage.Amount <= 0.f || bInvulnerable || !IsAlive()) {
		Rejection.bWasDamage = true;
		Rejection.Damage = Damage;
		return Rejection;
	}

	FUTDamage Working = Damage;
	for (const TObjectPtr<UUTDamageRule>& Rule : DamageRules) {
		if (Rule) {
			// Rules only transform the proposal; they cannot mutate this component.
			Working.Amount = FMath::Max(0.f, Rule->ModifyDamage(Working, this));
		}
	}

	if (Working.Amount <= 0.f) {
		Rejection.bWasDamage = true;
		Rejection.Damage = Working;
		return Rejection;
	}

	return Commit(CurrentHealth - Working.Amount, true, Working);
}

FUTHealthChange UUTHealthComponent::ApplyHeal(float Amount, AActor* Instigator)
{
	FUTHealthChange Rejection;
	if ((GetOwner() && !GetOwner()->HasAuthority()) || Amount <= 0.f || !IsAlive()) {
		return Rejection;
	}

	FUTDamage Meta;
	Meta.Instigator = Instigator;
	return Commit(CurrentHealth + Amount, false, Meta);
}

void UUTHealthComponent::Revive(float HealthFraction)
{
	if ((GetOwner() && !GetOwner()->HasAuthority()) || IsAlive() || MaxHealth <= 0.f) {
		return;
	}

	HealthFraction = FMath::Clamp(HealthFraction, 0.f, 1.f);
	const float NewHealth = FMath::Clamp(MaxHealth * HealthFraction, 1.f, MaxHealth);

	FUTDamage Meta;
	const FUTHealthChange Change = Commit(NewHealth, false, Meta);

	FUTRevivedSignature RevivedEvent = OnRevived;
	RevivedEvent.Broadcast();
}

void UUTHealthComponent::ResetHealth()
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	const float NewHealth = FMath::Clamp(StartingHealth, 0.f, MaxHealth);
	if (FMath::IsNearlyEqual(NewHealth, CurrentHealth)) {
		return;
	}

	const bool bWasDead = !IsAlive();
	FUTDamage Meta;
	Commit(NewHealth, false, Meta);

	if (bWasDead && IsAlive()) {
		FUTRevivedSignature RevivedEvent = OnRevived;
		RevivedEvent.Broadcast();
	}
}

void UUTHealthComponent::SetMaxHealth(float NewMaxHealth, bool bClampCurrent)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	NewMaxHealth = FMath::Max(0.f, NewMaxHealth);
	MaxHealth = NewMaxHealth;
	if (bClampCurrent) {
		const float Clamped = FMath::Clamp(CurrentHealth, 0.f, MaxHealth);
		if (Clamped != CurrentHealth) {
			Commit(Clamped, false, FUTDamage());
		}
	}
}

FUTHealthSnapshot UUTHealthComponent::MakeSnapshot() const
{
	FUTHealthSnapshot Snapshot;
	Snapshot.Current = CurrentHealth;
	Snapshot.Maximum = MaxHealth;
	return Snapshot;
}

void UUTHealthComponent::RestoreFromSnapshot(const FUTHealthSnapshot& Snapshot)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	MaxHealth = FMath::Max(0.f, Snapshot.Maximum);
	CurrentHealth = FMath::Clamp(Snapshot.Current, 0.f, MaxHealth);
	// Derived state must match the restored values exactly - a save with a dead actor
	// has to restore the replicated death flag too, not just the health number.
	LastSeenHealth = CurrentHealth;
	bDead = CurrentHealth <= 0.f;
	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.
}

FUTHealthChange UUTHealthComponent::Commit(float NewHealth, bool bWasDamage, const FUTDamage& Damage)
{
	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	LastSeenHealth = CurrentHealth;
	bDead = CurrentHealth <= 0.f;

	FUTHealthChange Change;
	Change.bApplied = true;
	Change.bWasDamage = bWasDamage;
	Change.OldHealth = OldHealth;
	Change.NewHealth = CurrentHealth;
	Change.Delta = CurrentHealth - OldHealth;
	Change.Damage = Damage;

	FUTHealthChangedSignature ChangedEvent = OnHealthChanged;
	ChangedEvent.Broadcast(Change);

	const bool bWasAlive = OldHealth > 0.f;
	const bool bIsAlive = CurrentHealth > 0.f;
	if (bWasAlive && !bIsAlive) {
		FUTDiedSignature DiedEvent = OnDied;
		DiedEvent.Broadcast();

		// Cross-domain death notice. Gameplay state lives on the authority, so the bus
		// message is authority-only; presentation hooks use the OnDied delegate above.
		if (GetOwner() && GetOwner()->HasAuthority()) {
			if (UUTMessageBusSubsystem* Bus = UUTMessageBusSubsystem::Get(this)) {
				FUTActorDiedMessage Message;
				Message.Victim = GetOwner();
				Message.Killer = Damage.Instigator;
				Bus->BroadcastMessage(UTTags::TAG_UT_Message_ActorDied, Message);
			}
		}
	}

	return Change;
}
