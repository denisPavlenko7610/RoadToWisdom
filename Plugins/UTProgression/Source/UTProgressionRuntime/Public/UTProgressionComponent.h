// UT Templates - actor-component progression.
#pragma once

#include "Components/ActorComponent.h"
#include "UTProgressionComponent.generated.h"

/** Plain data snapshot for save/load orchestration. */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTProgressionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int64 TotalXp = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTXpChangedSignature, int64, NewTotalXp, int64, Delta);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTLevelUpSignature, int32, NewLevel, int32, OldLevel);

/**
 * XP and levels. Levels come from cumulative thresholds: LevelXpThresholds[i] is the
 * total XP required to reach level i+1. With an empty array a linear curve is used
 * (level L needs (L-1) * BaseXpPerLevel XP), capped at MaxLevel.
 *
 * Rewards granting XP bridge from gameplay messages or game code; this module is pure bookkeeping.
 *
 * Multiplayer: server-authoritative. AddXp runs on the authority only; total XP and the
 * derived level are replicated, and clients see OnXpChanged/OnLevelUp through OnRep.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTPROGRESSIONRUNTIME_API UUTProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Progression")
	FUTXpChangedSignature OnXpChanged;

	UPROPERTY(BlueprintAssignable, Category = "UT|Progression")
	FUTLevelUpSignature OnLevelUp;

	/** Cumulative XP per level: index i = total XP to reach level i+1. Overrides the linear curve when non-empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	TArray<int32> LevelXpThresholds;

	/** Used only by the linear fallback curve. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "1"))
	int32 MaxLevel = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "1"))
	int32 BaseXpPerLevel = 100;

	UUTProgressionComponent();

	/** Grant positive XP; publishes OnXpChanged and OnLevelUp once per gained level. Returns the levels gained. Authority-only. */
	UFUNCTION(BlueprintCallable, Category = "UT|Progression")
	int32 AddXp(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "UT|Progression")
	int32 GetLevel() const
	{
		return CurrentLevel;
	}

	UFUNCTION(BlueprintPure, Category = "UT|Progression")
	int64 GetTotalXp() const
	{
		return TotalXp;
	}

	/** XP missing for the next level; -1 when at max level. */
	UFUNCTION(BlueprintPure, Category = "UT|Progression")
	int32 GetXpToNextLevel() const;

	/** Progress into the current level 0..1; 1 at max level. */
	UFUNCTION(BlueprintPure, Category = "UT|Progression")
	float GetLevelProgress() const;

	UFUNCTION(BlueprintPure, Category = "UT|Progression")
	bool IsMaxLevel() const;

	UFUNCTION(BlueprintPure, Category = "UT|Progression")
	FUTProgressionSnapshot MakeSnapshot() const;

	/** Restore raw XP without publishing events (save-load path); level is recomputed. */
	UFUNCTION(BlueprintCallable, Category = "UT|Progression")
	void RestoreFromSnapshot(const FUTProgressionSnapshot& Snapshot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class FUTProgressionRestoreAuthorityTest;
	UPROPERTY(ReplicatedUsing = OnRep_TotalXp, VisibleAnywhere, Category = "UT")
	int64 TotalXp = 0;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "UT")
	int32 CurrentLevel = 1;

	/** Levels seen on this machine, to diff OnRep into level-up events. */
	int32 LastSeenLevel = 1;

	UFUNCTION()
	void OnRep_TotalXp();

	int64 XpForLevel(int32 Level) const;

	int32 GetMaxLevel() const;

	int32 LevelFromXp(int64 InXp) const;
};
