// UT Templates - actor-component stats.
#pragma once

#include "Components/ActorComponent.h"
#include "UTStatsTypes.h"
#include "UTStatsComponent.generated.h"

/**
 * Named stat base values with stacking modifiers - the light alternative to a GAS
 * AttributeSet for projects that do not run the abilities framework.
 *
 * Final value: (base + sum of Add) * (1 + sum of Multiply), clamped to >= 0.
 * Modifiers are registered against stats that exist in BaseValues and carry an
 * optional Source object so a buff system can remove everything it granted.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTCOMBATLITE_API UUTStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTStatChangedSignature, FName, StatId, float, NewValue);

	UPROPERTY(BlueprintAssignable, Category = "UT|Stats")
	FUTStatChangedSignature OnStatChanged;

	/** Declared stats and their base values; modifiers can only attach to declared stats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	TMap<FName, float> BaseValues;

	/** Register a modifier. Returns an invalid handle if the stat is not declared. */
	UFUNCTION(BlueprintCallable, Category = "UT|Stats")
	FUTStatModifierHandle AddModifier(FName StatId, EUTStatModifierOp Op, float Magnitude, UObject* Source = nullptr);

	UUTStatsComponent();

	UFUNCTION(BlueprintCallable, Category = "UT|Stats")
	void RemoveModifier(FName StatId, FUTStatModifierHandle Handle);

	/** Remove every modifier granted by Source (all stats, or one stat when declared). */
	UFUNCTION(BlueprintCallable, Category = "UT|Stats")
	void RemoveModifiersFromSource(UObject* Source, FName StatId);

	UFUNCTION(BlueprintPure, Category = "UT|Stats")
	float GetStatValue(FName StatId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Stats")
	float GetStatBase(FName StatId) const;

	/** Change the base value; publishes OnStatChanged when the final value moves. */
	UFUNCTION(BlueprintCallable, Category = "UT|Stats")
	bool SetStatBase(FName StatId, float NewBase);

	UFUNCTION(BlueprintPure, Category = "UT|Stats")
	bool HasStat(FName StatId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Stats")
	FUTStatsSnapshot MakeSnapshot() const;

	/** Restore raw base values without publishing events (save-load path). */
	UFUNCTION(BlueprintCallable, Category = "UT|Stats")
	void RestoreFromSnapshot(const FUTStatsSnapshot& Snapshot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	friend class FUTStatsReplicationTest;

	/** Server-authoritative stat rows replicated to clients (base + computed final). */
	UPROPERTY(ReplicatedUsing = OnRep_StatEntries, VisibleAnywhere, Category = "UT")
	TArray<FUTReplicatedStatEntry> StatEntries;

	UFUNCTION()
	void OnRep_StatEntries();

	/** Last final values seen on this machine, to diff OnRep into change events. */
	TMap<FName, float> LastSeenValues;

	/** Push the authoritative base/final pair for one stat into the replicated rows. */
	void SyncStatEntries(FName StatId);

	struct FUTModifierEntry
	{
		EUTStatModifierOp Op = EUTStatModifierOp::Add;
		float Magnitude = 0.f;
		int32 Id = 0;
		TWeakObjectPtr<UObject> Source;
	};

	// Session state only (not saved, no reflection needed): plain C++ container.
	TMap<FName, TArray<FUTModifierEntry>> Modifiers;

	int32 NextModifierId = 1;

	float ComputeValue(FName StatId) const;
};
