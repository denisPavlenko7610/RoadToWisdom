// UT Templates - wallet component implementation.
#include "UTWalletComponent.h"

#include "Net/UnrealNetwork.h"

UUTWalletComponent::UUTWalletComponent()
{
	SetIsReplicatedByDefault(true);
}

void UUTWalletComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTWalletComponent, BalanceEntries);
}

void UUTWalletComponent::OnRep_BalanceEntries()
{
	// Diffs drive the change event; the entry array is the state the client trusts.
	const TMap<FName, int64> Previous = MoveTemp(ReplicatedBalancesLastSeen);
	ReplicatedBalancesLastSeen.Reset();
	for (const FUTBalanceEntry& Entry : BalanceEntries) {
		ReplicatedBalancesLastSeen.Add(Entry.CurrencyId, Entry.Balance);
	}
	for (const FUTBalanceEntry& Entry : BalanceEntries) {
		const int64* Old = Previous.Find(Entry.CurrencyId);
		if (!Old || *Old != Entry.Balance) {
			FUTWalletChangedSignature ChangedEvent = OnBalanceChanged;
			ChangedEvent.Broadcast(Entry.CurrencyId, Entry.Balance);
		}
	}
	for (const TPair<FName, int64>& Pair : Previous) {
		if (!ReplicatedBalancesLastSeen.Contains(Pair.Key)) {
			FUTWalletChangedSignature ChangedEvent = OnBalanceChanged;
			ChangedEvent.Broadcast(Pair.Key, 0);
		}
	}
}

void UUTWalletComponent::RecordBalance(FName CurrencyId, int64 NewBalance)
{
	for (FUTBalanceEntry& Entry : BalanceEntries) {
		if (Entry.CurrencyId == CurrencyId) {
			Entry.Balance = NewBalance;
			return;
		}
	}
	FUTBalanceEntry Entry;
	Entry.CurrencyId = CurrencyId;
	Entry.Balance = NewBalance;
	BalanceEntries.Add(Entry);
}

FUTResult UUTWalletComponent::Add(FName CurrencyId, int64 Amount)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("Wallet changes must run on the server.")));
	}

	if (CurrencyId.IsNone() || Amount <= 0) {
		return FUTResult::Failure(
			TEXT("InvalidAmount"),
			FText::FromString(TEXT("Wallet Add requires a currency id and a positive amount.")));
	}

	const int64 Balance = GetBalance(CurrencyId);
	if (Amount > MAX_int64 - Balance) {
		return FUTResult::Failure(TEXT("BalanceOverflow"), FText::FromString(TEXT("Wallet balance exceeds int64 capacity.")));
	}
	return SetBalanceInternal(CurrencyId, Balance + Amount);
}

FUTResult UUTWalletComponent::TrySpend(FName CurrencyId, int64 Amount)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("Wallet changes must run on the server.")));
	}

	if (CurrencyId.IsNone() || Amount <= 0) {
		return FUTResult::Failure(
			TEXT("InvalidAmount"),
			FText::FromString(TEXT("Wallet TrySpend requires a currency id and a positive amount.")));
	}

	const int64 Current = GetBalance(CurrencyId);
	if (Current < Amount) {
		return FUTResult::Failure(
			TEXT("InsufficientFunds"),
			FText::FromString(FString::Printf(TEXT("Balance %lld is less than the requested %lld."), Current, Amount)));
	}

	return SetBalanceInternal(CurrencyId, Current - Amount);
}

FUTResult UUTWalletComponent::SetBalance(FName CurrencyId, int64 NewBalance)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("Wallet changes must run on the server.")));
	}

	if (CurrencyId.IsNone() || NewBalance < 0) {
		return FUTResult::Failure(
			TEXT("InvalidAmount"),
			FText::FromString(TEXT("Wallet SetBalance requires a currency id and a non-negative balance.")));
	}

	return SetBalanceInternal(CurrencyId, NewBalance);
}

int64 UUTWalletComponent::GetBalance(FName CurrencyId) const
{
	for (const FUTBalanceEntry& Entry : BalanceEntries) {
		if (Entry.CurrencyId == CurrencyId) {
			return Entry.Balance;
		}
	}
	return 0;
}

bool UUTWalletComponent::CanAfford(FName CurrencyId, int64 Amount) const
{
	return Amount >= 0 && GetBalance(CurrencyId) >= Amount;
}

TArray<FName> UUTWalletComponent::GetKnownCurrencies() const
{
	TArray<FName> Currencies;
	Currencies.Reserve(BalanceEntries.Num());
	for (const FUTBalanceEntry& Entry : BalanceEntries) {
		Currencies.Add(Entry.CurrencyId);
	}
	return Currencies;
}

FUTWalletSnapshot UUTWalletComponent::MakeSnapshot() const
{
	FUTWalletSnapshot Snapshot;
	for (const FUTBalanceEntry& Entry : BalanceEntries) {
		Snapshot.Balances.Add(Entry.CurrencyId, Entry.Balance);
	}
	return Snapshot;
}

void UUTWalletComponent::RestoreFromSnapshot(const FUTWalletSnapshot& Snapshot)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	BalanceEntries.Reset();
	for (const TPair<FName, int64>& Pair : Snapshot.Balances) {
		FUTBalanceEntry Entry;
		Entry.CurrencyId = Pair.Key;
		Entry.Balance = Pair.Value;
		BalanceEntries.Add(Entry);
	}
	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.
}

FUTResult UUTWalletComponent::SetBalanceInternal(FName CurrencyId, int64 NewBalance)
{
	RecordBalance(CurrencyId, NewBalance);
	ReplicatedBalancesLastSeen.Add(CurrencyId, NewBalance);
	FUTWalletChangedSignature ChangedEvent = OnBalanceChanged;
	ChangedEvent.Broadcast(CurrencyId, NewBalance);
	return FUTResult::Success();
}
