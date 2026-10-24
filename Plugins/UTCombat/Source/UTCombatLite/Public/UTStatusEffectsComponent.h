// UT Templates - status effects component.
#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UTStatsTypes.h"
#include "UTStatusEffectsComponent.generated.h"

class UUTStatusEffect;
class UUTHealthComponent;
class UUTStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTEffectAppliedSignature, UUTStatusEffect*, Effect, int32, Handle);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTEffectRemovedSignature, UUTStatusEffect*, Effect, int32, Handle);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTReplicatedEffectsChangedSignature);

/** Client-visible description of one active effect (UI contract, no internals). */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTActiveEffectDisplay
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName EffectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTagContainer EffectTags;

	/** Total duration; 0 means "until removed". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Duration = 0.f;

	/** Seconds left, refreshed once per second by the authority (UI interpolates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float Remaining = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int32 Handle = 0;
};

/**
 * StatId plus the handle UTStats returned for it. A struct instead of
 * TPair<FName, FUTStatModifierHandle> because a TPair cannot be a UPROPERTY, and
 * the owning active-effect record has to be reflected to keep the GC aware.
 */
USTRUCT()
struct UTCOMBATLITE_API FUTEffectStatHandle
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FName StatId;

	UPROPERTY(Transient)
	FUTStatModifierHandle Modifier;
};

/** One active effect instance. */
USTRUCT()
struct UTCOMBATLITE_API FUTActiveEffect
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	int32 Handle = 0;

	/**
	 * Definition asset. Safe only because the owning component keeps the whole map in a
	 * UPROPERTY: a native TMap is invisible to the GC, so an inline-created definition
	 * (UUTStatusEffect is DefaultToInstanced) would be collected mid-effect and leave a
	 * dangling modifier behind.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UUTStatusEffect> Def = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Source;

	UPROPERTY(Transient)
	FTimerHandle ExpiryTimer;

	UPROPERTY(Transient)
	FTimerHandle DamageTimer;

	/** StatId + handle pairs: removal needs both. */
	UPROPERTY(Transient)
	TArray<FUTEffectStatHandle> StatHandles;
};

/**
 * Timed status effects on one actor: stat modifiers from UTStats and optional periodic
 * damage through UTHealth, with per-definition stacking policies.
 *
 * Effects are session state: never serialized, and EndPlay reverts every modifier
 * silently so UTStats bases stay clean.
 *
 * Multiplayer: server-authoritative. Timers and stat modifiers live on the authority;
 * clients receive a lightweight replicated display list (name, tags, duration,
 * remaining seconds refreshed once per second) through OnRep, which drives
 * OnReplicatedEffectsChanged for UI. Expiration is server-decided.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTCOMBATLITE_API UUTStatusEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|StatusEffects")
	FUTEffectAppliedSignature OnEffectApplied;

	UPROPERTY(BlueprintAssignable, Category = "UT|StatusEffects")
	FUTEffectRemovedSignature OnEffectRemoved;

	/** Clients only: fired when the replicated display list changes (apply, remove, periodic refresh). */
	UPROPERTY(BlueprintAssignable, Category = "UT|StatusEffects")
	FUTReplicatedEffectsChangedSignature OnReplicatedEffectsChanged;

	UUTStatusEffectsComponent();

	/** Register an effect. Returns the active-instance handle; 0 when rejected by the stacking policy. */
	UFUNCTION(BlueprintCallable, Category = "UT|StatusEffects")
	int32 ApplyEffect(UUTStatusEffect* Effect, AActor* Source = nullptr);

	UFUNCTION(BlueprintCallable, Category = "UT|StatusEffects")
	void RemoveEffect(int32 Handle);

	/** Remove every effect granted by Source (all their stat modifiers revert). */
	UFUNCTION(BlueprintCallable, Category = "UT|StatusEffects")
	void RemoveEffectsFromSource(AActor* Source);

	/** Remove every effect owning any of the tags. */
	UFUNCTION(BlueprintCallable, Category = "UT|StatusEffects")
	void RemoveEffectsByTag(const FGameplayTagContainer& Tags);

	/** True while an effect owning Tag is active. */
	UFUNCTION(BlueprintPure, Category = "UT|StatusEffects")
	bool IsAffectedBy(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "UT|StatusEffects")
	int32 GetActiveEffectCount() const;

	/** Client-visible active effect list (replicated; empty on the server beyond what Apply published). */
	UFUNCTION(BlueprintPure, Category = "UT|StatusEffects")
	TArray<FUTActiveEffectDisplay> GetActiveEffectDisplays() const
	{
		return ReplicatedDisplays;
	}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class FUTStatusClientReadTest;
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedDisplays, VisibleAnywhere, Category = "UT")
	TArray<FUTActiveEffectDisplay> ReplicatedDisplays;

	/** Server: rebuild the replicated display list from live instances. */
	void RefreshReplicatedDisplays();

	UFUNCTION()
	void OnRep_ReplicatedDisplays();

	FTimerHandle DisplayRefreshTimer;

	int32 ApplyEffectInternal(UUTStatusEffect* Effect, AActor* Source);

	void RemoveEffectInternal(int32 Handle);

	void HandleExpiry(int32 Handle);

	void HandlePeriodicDamage(int32 Handle);

	/** Clear one instance's timers and revert its stat modifiers. */
	void ReleaseInstance(FUTActiveEffect& Instance, UUTStatsComponent* Stats);

	UUTStatsComponent* ResolveStats();

	UUTHealthComponent* ResolveHealth();

	/** UPROPERTY so the GC sees the definition references - see FUTActiveEffect::Def. */
	UPROPERTY(Transient)
	TMap<int32, FUTActiveEffect> Active;

	TWeakObjectPtr<UUTStatsComponent> CachedStats;
	TWeakObjectPtr<UUTHealthComponent> CachedHealth;
	int32 NextHandle = 1;
};
