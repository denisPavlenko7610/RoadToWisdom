// UT Templates - actor-component health.
#pragma once

#include "Components/ActorComponent.h"
#include "UTDamageContract.h"
#include "UTHealthTypes.h"
#include "UTHealthComponent.generated.h"

/**
 * Actor-owned health with typed damage, ordered damage rules, healing, and death/revive.
 *
 * Implements IUTDamageReceiver, so any damage source built on the UTCombatCore contract
 * can damage it without knowing this component exists. ApplyDamage(FUTDamage) stays for
 * in-domain callers (status effects, game code).
 *
 * Event contract: a successful gameplay commit publishes OnHealthChanged, followed by
 * OnDied or OnRevived when a state boundary is crossed. RestoreFromSnapshot and
 * configuration changes publish no gameplay events - loading orchestration rebuilds
 * derived state once after all modules are restored.
 *
 * Multiplayer: server-authoritative. Damage, healing, revive and cap changes run on
 * the authority only; health, cap and invulnerability are replicated, so clients see
 * OnHealthChanged / OnDied / OnRevived through OnRep notifications. Never trust a
 * client-supplied damage amount - build damage contexts server-side.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTCOMBATLITE_API UUTHealthComponent : public UActorComponent, public IUTDamageReceiver
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTHealthChangedSignature, const FUTHealthChange&, Change);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTDiedSignature);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTRevivedSignature);

	UPROPERTY(BlueprintAssignable, Category = "UT")
	FUTHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "UT")
	FUTDiedSignature OnDied;

	UPROPERTY(BlueprintAssignable, Category = "UT")
	FUTRevivedSignature OnRevived;

	/** Replicated cap; changes are authority-only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "UT", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float StartingHealth = 100.f;

	/** Invulnerable actors ignore damage entirely; healing still applies. Authority-only change, replicated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "UT")
	bool bInvulnerable = false;

	/** Consulted in order before damage is applied; each rule transforms the proposed amount. */
	UPROPERTY(EditAnywhere, Instanced, Category = "UT")
	TArray<TObjectPtr<UUTDamageRule>> DamageRules;

	UUTHealthComponent();

	/**
	 * Explicit spawn-time initialization (max, starting). Components created in tests or
	 * spawned dynamically call this instead of relying on BeginPlay defaults. Authority-only.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	void InitializeHealth(float NewMaxHealth, float NewStartingHealth);

	/** Run rules, then apply. Non-positive amounts are rejected without publishing events. Named ApplyHealthDamage to stay clear of IUTDamageReceiver::ApplyDamage. */
	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	FUTHealthChange ApplyHealthDamage(const FUTDamage& Damage);

	// -- IUTDamageReceiver ------------------------------------------------------
	virtual FUTDamageResult ApplyDamage_Implementation(const FUTDamageContext& Context) override;

	virtual bool CanReceiveDamage_Implementation() const override
	{
		return !bInvulnerable && IsAlive();
	}

	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	FUTHealthChange ApplyHeal(float Amount, AActor* Instigator = nullptr);

	/** Bring a dead actor back; only works while dead. */
	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	void Revive(float HealthFraction = 1.f);

	/** Restore to StartingHealth. Publishes Changed (and Revived when leaving death). */
	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category = "UT|Health")
	bool IsAlive() const
	{
		return CurrentHealth > 0.f;
	}

	UFUNCTION(BlueprintPure, Category = "UT|Health")
	float GetHealth() const
	{
		return CurrentHealth;
	}

	UFUNCTION(BlueprintPure, Category = "UT|Health")
	float GetMaxHealth() const
	{
		return MaxHealth;
	}

	UFUNCTION(BlueprintPure, Category = "UT|Health")
	float GetHealthFraction() const
	{
		return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
	}

	/** Change the cap. Clamps current health unless bClampCurrent is false. */
	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	void SetMaxHealth(float NewMaxHealth, bool bClampCurrent = true);

	UFUNCTION(BlueprintPure, Category = "UT|Health")
	FUTHealthSnapshot MakeSnapshot() const;

	/** Restore raw values without publishing gameplay events (save-load path). */
	UFUNCTION(BlueprintCallable, Category = "UT|Health")
	void RestoreFromSnapshot(const FUTHealthSnapshot& Snapshot);

	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class FUTHealthTest;
	/** Authoritative on the server; replicated to clients (UI reads this). */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, VisibleAnywhere, Category = "UT")
	float CurrentHealth = 0.f;

	/** Authority-side death flag; clients observe death/revive through its OnRep. */
	UPROPERTY(ReplicatedUsing = OnRep_bDead, VisibleAnywhere, Category = "UT")
	bool bDead = false;

	/** Last health value seen on this machine, to build change records on clients. */
	float LastSeenHealth = 0.f;

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_bDead();

	void BroadcastHealthChange(bool bWasDamage, float OldHealth);

	FUTHealthChange Commit(float NewHealth, bool bWasDamage, const FUTDamage& Damage);
};
