// UT Templates - actor-component wallet.
#pragma once

#include "Components/ActorComponent.h"
#include "UTResult.h"
#include "UTWalletComponent.generated.h"

/** One replicated balance entry (TMap cannot be replicated directly). */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTBalanceEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName CurrencyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int64 Balance = 0;
};

/** Plain data snapshot for save/load orchestration. */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTWalletSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TMap<FName, int64> Balances;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTWalletChangedSignature, FName, CurrencyId, int64, NewBalance);

/**
 * Named currency balances with guarded add/spend. Currency identities are FNames
 * owned by the game; display data (icons, names) is presentation concern and lives
 * outside this module.
 *
 * Multiplayer: server-authoritative. Add/TrySpend/SetBalance run on the authority only;
 * clients observe OnBalanceChanged through OnRep diffs of the replicated balances.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTPROGRESSIONRUNTIME_API UUTWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Wallet")
	FUTWalletChangedSignature OnBalanceChanged;

	UUTWalletComponent();

	/** Add a positive amount. Rejected amounts publish nothing. */
	UFUNCTION(BlueprintCallable, Category = "UT|Wallet")
	FUTResult Add(FName CurrencyId, int64 Amount);

	/** Spend atomically: either the full amount is deducted or nothing changes. */
	UFUNCTION(BlueprintCallable, Category = "UT|Wallet")
	FUTResult TrySpend(FName CurrencyId, int64 Amount);

	/** Direct assignment for game logic and debugging; publishes OnBalanceChanged. */
	UFUNCTION(BlueprintCallable, Category = "UT|Wallet")
	FUTResult SetBalance(FName CurrencyId, int64 NewBalance);

	UFUNCTION(BlueprintPure, Category = "UT|Wallet")
	int64 GetBalance(FName CurrencyId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Wallet")
	bool CanAfford(FName CurrencyId, int64 Amount) const;

	UFUNCTION(BlueprintPure, Category = "UT|Wallet")
	TArray<FName> GetKnownCurrencies() const;

	UFUNCTION(BlueprintPure, Category = "UT|Wallet")
	FUTWalletSnapshot MakeSnapshot() const;

	/** Restore raw balances without publishing events (save-load path). */
	UFUNCTION(BlueprintCallable, Category = "UT|Wallet")
	void RestoreFromSnapshot(const FUTWalletSnapshot& Snapshot);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class FUTWalletTest;
	/** Server-authoritative balances, replicated as an entry array (TMap is not replicable). */
	UPROPERTY(ReplicatedUsing = OnRep_BalanceEntries, VisibleAnywhere, Category = "UT")
	TArray<FUTBalanceEntry> BalanceEntries;

	UFUNCTION()
	void OnRep_BalanceEntries();

	/** Last balances seen on this machine, to diff OnRep into change events. */
	TMap<FName, int64> ReplicatedBalancesLastSeen;

	FUTResult SetBalanceInternal(FName CurrencyId, int64 NewBalance);

	void RecordBalance(FName CurrencyId, int64 NewBalance);
};
