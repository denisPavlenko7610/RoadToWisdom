// UT Templates - stats component implementation.
#include "UTStatsComponent.h"

#include "Net/UnrealNetwork.h"

UUTStatsComponent::UUTStatsComponent()
{
	SetIsReplicatedByDefault(true);
}

void UUTStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTStatsComponent, StatEntries);
}

void UUTStatsComponent::OnRep_StatEntries()
{
	// Diff the replicated rows into change events; clients read FinalValue.
	const TMap<FName, float> Previous = MoveTemp(LastSeenValues);
	LastSeenValues.Reset();
	for (const FUTReplicatedStatEntry& Entry : StatEntries) {
		LastSeenValues.Add(Entry.StatId, Entry.FinalValue);
	}
	for (const FUTReplicatedStatEntry& Entry : StatEntries) {
		const float* Old = Previous.Find(Entry.StatId);
		if (!Old || !FMath::IsNearlyEqual(*Old, Entry.FinalValue)) {
			FUTStatChangedSignature ChangedEvent = OnStatChanged;
			ChangedEvent.Broadcast(Entry.StatId, Entry.FinalValue);
		}
	}
	for (const TPair<FName, float>& Pair : Previous) {
		if (!LastSeenValues.Contains(Pair.Key)) {
			FUTStatChangedSignature ChangedEvent = OnStatChanged;
			ChangedEvent.Broadcast(Pair.Key, 0.f);
		}
	}
}

void UUTStatsComponent::SyncStatEntries(FName StatId)
{
	for (FUTReplicatedStatEntry& Entry : StatEntries) {
		if (Entry.StatId == StatId) {
			Entry.BaseValue = GetStatBase(StatId);
			Entry.FinalValue = ComputeValue(StatId);
			return;
		}
	}
	if (BaseValues.Contains(StatId)) {
		FUTReplicatedStatEntry Entry;
		Entry.StatId = StatId;
		Entry.BaseValue = GetStatBase(StatId);
		Entry.FinalValue = ComputeValue(StatId);
		StatEntries.Add(Entry);
	}
}

FUTStatModifierHandle UUTStatsComponent::AddModifier(FName StatId, EUTStatModifierOp Op, float Magnitude, UObject* Source)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return FUTStatModifierHandle();
	}

	FUTStatModifierHandle Handle;
	if (!BaseValues.Contains(StatId)) {
		return Handle;
	}

	FUTModifierEntry Entry;
	Entry.Op = Op;
	Entry.Magnitude = Magnitude;
	Entry.Id = NextModifierId++;
	Entry.Source = Source;
	Modifiers.FindOrAdd(StatId).Add(Entry);

	Handle.Id = Entry.Id;

	FUTStatChangedSignature ChangedEvent = OnStatChanged;
	ChangedEvent.Broadcast(StatId, ComputeValue(StatId));
	SyncStatEntries(StatId);
	return Handle;
}

void UUTStatsComponent::RemoveModifier(FName StatId, FUTStatModifierHandle Handle)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	TArray<FUTModifierEntry>* Entries = Modifiers.Find(StatId);
	if (!Entries || !Handle.IsValid()) {
		return;
	}

	const int32 RemovedCount = Entries->RemoveAll(
		[&Handle](const FUTModifierEntry& Entry) {
			return Entry.Id == Handle.Id;
		});
	if (RemovedCount > 0) {
		FUTStatChangedSignature ChangedEvent = OnStatChanged;
		ChangedEvent.Broadcast(StatId, ComputeValue(StatId));
		SyncStatEntries(StatId);
	}
}

void UUTStatsComponent::RemoveModifiersFromSource(UObject* Source, FName StatId)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	const TWeakObjectPtr<UObject> WeakSource(Source);

	TArray<FName, TInlineAllocator<8>> StatIds;
	if (StatId.IsNone()) {
		Modifiers.GetKeys(StatIds);
	}
	else {
		StatIds.Add(StatId);
	}

	for (const FName& Id : StatIds) {
		TArray<FUTModifierEntry>* Entries = Modifiers.Find(Id);
		if (!Entries) {
			continue;
		}
		const int32 RemovedCount = Entries->RemoveAll(
			[&WeakSource](const FUTModifierEntry& Entry) {
				return Entry.Source == WeakSource;
			});
		if (RemovedCount > 0) {
			FUTStatChangedSignature ChangedEvent = OnStatChanged;
			ChangedEvent.Broadcast(Id, ComputeValue(Id));
			SyncStatEntries(Id);
		}
	}
}

float UUTStatsComponent::GetStatValue(FName StatId) const
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		const FUTReplicatedStatEntry* Entry = StatEntries.FindByPredicate(
			[StatId](const FUTReplicatedStatEntry& Row) {
				return Row.StatId == StatId;
			});
		return Entry ? Entry->FinalValue : 0.f;
	}
	return ComputeValue(StatId);
}

bool UUTStatsComponent::HasStat(FName StatId) const
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return StatEntries.ContainsByPredicate(
			[StatId](const FUTReplicatedStatEntry& Row) {
				return Row.StatId == StatId;
			});
	}
	return BaseValues.Contains(StatId);
}

void UUTStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || GetOwner()->HasAuthority()) {
		StatEntries.Reset();
		for (const TPair<FName, float>& Pair : BaseValues) {
			SyncStatEntries(Pair.Key);
		}
	}
}

float UUTStatsComponent::GetStatBase(FName StatId) const
{
	// Same unified read model as GetStatValue.
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		for (const FUTReplicatedStatEntry& Entry : StatEntries) {
			if (Entry.StatId == StatId) {
				return Entry.BaseValue;
			}
		}
		return 0.f;
	}

	const float* Base = BaseValues.Find(StatId);
	return Base ? *Base : 0.f;
}

bool UUTStatsComponent::SetStatBase(FName StatId, float NewBase)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return false;
	}

	float* Base = BaseValues.Find(StatId);
	if (!Base) {
		return false;
	}
	*Base = NewBase;

	FUTStatChangedSignature ChangedEvent = OnStatChanged;
	ChangedEvent.Broadcast(StatId, ComputeValue(StatId));
	SyncStatEntries(StatId);
	return true;
}

FUTStatsSnapshot UUTStatsComponent::MakeSnapshot() const
{
	FUTStatsSnapshot Snapshot;
	Snapshot.BaseValues = BaseValues;
	return Snapshot;
}

void UUTStatsComponent::RestoreFromSnapshot(const FUTStatsSnapshot& Snapshot)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	BaseValues = Snapshot.BaseValues;
	// Modifiers are session state (buffs, equipment) and are not restored; their
	// owning systems re-apply them when they themselves are restored.
	Modifiers.Reset();
	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.

	// Rebuild the replicated rows from the restored base values.
	StatEntries.Reset();
	for (const TPair<FName, float>& Pair : BaseValues) {
		SyncStatEntries(Pair.Key);
	}
}

float UUTStatsComponent::ComputeValue(FName StatId) const
{
	const float* Base = BaseValues.Find(StatId);
	if (!Base) {
		return 0.f;
	}

	float Value = *Base;
	float MultiplySum = 0.f;
	if (const TArray<FUTModifierEntry>* Entries = Modifiers.Find(StatId)) {
		for (const FUTModifierEntry& Entry : *Entries) {
			if (Entry.Op == EUTStatModifierOp::Add) {
				Value += Entry.Magnitude;
			}
			else {
				MultiplySum += Entry.Magnitude;
			}
		}
	}
	Value *= (1.f + MultiplySum);
	return FMath::Max(0.f, Value);
}
