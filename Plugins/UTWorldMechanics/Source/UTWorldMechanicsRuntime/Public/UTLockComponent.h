#pragma once
#include "Components/ActorComponent.h"
#include "UTConditions.h"
#include "UTLockComponent.generated.h"
class UUTCondition;
class UUTActionCost;
USTRUCT(BlueprintType)
struct UTWORLDMECHANICSRUNTIME_API FUTLockState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "UT|Lock")
	bool bLocked = true;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTLockChanged, bool, bLocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTUnlocked);
/** Reusable lock capability; no door, chest or inventory dependency. Components expose
 * snapshots; the saveable owner composes them into its persistent actor state. */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTWORLDMECHANICSRUNTIME_API UUTLockComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UUTLockComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Lock")
	bool bInitiallyLocked = true;
	/** Pure tag/stat/custom condition tree. Empty means no additional condition. */
	UPROPERTY(EditAnywhere, Instanced, Category = "UT|Lock")
	TObjectPtr<UUTCondition> UnlockConditions;
	/** Optional key/resource adapter. Kept generic so WorldMechanics never inspects inventory. */
	UPROPERTY(EditAnywhere, Instanced, Category = "UT|Lock")
	TObjectPtr<UUTActionCost> UnlockCost;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Lock", meta = (EditCondition = "UnlockCost != nullptr"))
	bool bConsumeKey = false;
	/** 0 disables this extra spatial gate for trusted server composition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Lock", meta = (ClampMin = "0"))
	float UnlockDistance = 300.f;
	UPROPERTY(BlueprintAssignable, Category = "UT|Lock")
	FUTUnlocked OnUnlocked;
	UPROPERTY(BlueprintAssignable, Category = "UT|Lock")
	FUTLockChanged OnLockChanged;
	UFUNCTION(BlueprintPure, Category = "UT|Lock")
	bool IsLocked() const { return bLocked; }
	UFUNCTION(BlueprintPure, Category = "UT|Lock")
	bool CanUse(AActor* User) const;
	UFUNCTION(BlueprintPure, Category = "UT|Lock")
	bool CanUnlock(AActor* User) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UT|Lock")
	bool TryUnlock(AActor* User);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UT|Lock")
	void Lock();
	UFUNCTION(BlueprintPure, Category = "UT|Lock")
	FUTLockState MakeSnapshot() const;
	/** Silent restore; owner/save orchestration refreshes derived presentation. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UT|Lock")
	bool RestoreFromSnapshot(const FUTLockState& State);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Locked)
	bool bLocked = true;
	bool bTransacting = false;
	bool IsUserValid(AActor* User) const;
	UFUNCTION()
	void OnRep_Locked();
};

/** Pure bridge for interaction/container gates; the consumer only needs UUTCondition. */
UCLASS(Blueprintable, EditInlineNew)
class UTWORLDMECHANICSRUNTIME_API UUTConditionLockUnlocked : public UUTCondition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Lock")
	TObjectPtr<UUTLockComponent> Lock;
	virtual bool Evaluate_Implementation(const FUTConditionContext&) const override { return IsValid(Lock) && !Lock->IsLocked(); }
};
