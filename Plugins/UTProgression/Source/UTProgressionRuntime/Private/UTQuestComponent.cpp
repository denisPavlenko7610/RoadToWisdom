// UT Templates - quest tracker implementation.
#include "UTQuestComponent.h"

#include "Net/UnrealNetwork.h"

#include "Engine/World.h"
#include "UTMessageBusSubsystem.h"
#include "UTMessageTypes.h"
#include "UTQuestsLog.h"

UUTQuestComponent::UUTQuestComponent()
{
	SetIsReplicatedByDefault(true);
}

void UUTQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTQuestComponent, ReplicatedQuests);
}

void UUTQuestComponent::RebuildReplicatedQuests()
{
	ReplicatedQuests.Reset();
	for (const TPair<FName, FUTActiveQuest>& Pair : Quests) {
		FUTReplicatedQuest Entry;
		Entry.QuestId = Pair.Key;
		Entry.bAccepted = true;
		Entry.bCompleted = Pair.Value.bCompleted;
		for (const TPair<FName, int32>& Count : Pair.Value.Progress) {
			FUTReplicatedObjectiveCount Objective;
			Objective.ObjectiveId = Count.Key;
			Objective.Count = Count.Value;
			Entry.Objectives.Add(Objective);
		}
		ReplicatedQuests.Add(Entry);
	}
}

void UUTQuestComponent::OnRep_Quests()
{
	for (auto It = LastSeenReplicatedQuests.CreateIterator(); It; ++It) {
		if (!ReplicatedQuests.ContainsByPredicate(
				[Id = It.Key()](const FUTReplicatedQuest& Quest) {
					return Quest.QuestId == Id;
				})) {
			It.RemoveCurrent();
		}
	}
	// Diff replication into the same events the authority fires directly, so quest UI
	// behaves identically on clients without any extra plumbing.
	for (const FUTReplicatedQuest& Quest : ReplicatedQuests) {
		const FUTReplicatedQuest* Seen = LastSeenReplicatedQuests.Find(Quest.QuestId);
		if (!Seen) {
			FUTQuestAcceptedSignature AcceptedEvent = OnQuestAccepted;
			AcceptedEvent.Broadcast(GetOrLoadQuestDefinition(Quest.QuestId));
		}

		if (Seen && !Seen->bCompleted && Quest.bCompleted) {
			FUTQuestCompletedSignature CompletedEvent = OnQuestCompleted;
			CompletedEvent.Broadcast(GetOrLoadQuestDefinition(Quest.QuestId));
		}

		if (Seen) {
			for (const FUTReplicatedObjectiveCount& Objective : Quest.Objectives) {
				const FUTReplicatedObjectiveCount* OldCount = Seen->Objectives.FindByPredicate(
					[&Objective](const FUTReplicatedObjectiveCount& Candidate) {
						return Candidate.ObjectiveId == Objective.ObjectiveId;
					});
				if (!OldCount || OldCount->Count != Objective.Count) {
					FUTObjectiveAdvancedSignature AdvancedEvent = OnObjectiveAdvanced;
					AdvancedEvent.Broadcast(GetOrLoadQuestDefinition(Quest.QuestId), Objective.ObjectiveId);
				}
			}
		}

		LastSeenReplicatedQuests.Add(Quest.QuestId, Quest);
	}
}

FUTResult UUTQuestComponent::AcceptQuest(UUTQuestDefinition* Quest)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("AcceptQuest must run on the server.")));
	}

	if (!Quest || Quest->QuestId.IsNone()) {
		return FUTResult::Failure(
			TEXT("InvalidQuest"),
			FText::FromString(TEXT("AcceptQuest requires a quest definition with a QuestId.")));
	}
	if (Quests.Contains(Quest->QuestId)) {
		return FUTResult::Failure(
			TEXT("DuplicateQuest"),
			FText::FromString(FString::Printf(TEXT("Quest '%s' is already accepted or completed."), *Quest->QuestId.ToString())));
	}
	if (!ArePrerequisitesMet(Quest)) {
		return FUTResult::Failure(
			TEXT("PrerequisitesNotMet"),
			FText::FromString(FString::Printf(TEXT("Quest '%s' has unmet prerequisite quests."), *Quest->QuestId.ToString())));
	}

	// Remember the asset: the snapshot only stores QuestId, so a restore on a fresh
	// component has nothing else to resolve the definition from.
	RegisterQuestDefinition(Quest);

	FUTActiveQuest State;
	State.Quest = Quest;
	State.DefinitionPath = FSoftObjectPath(Quest);
	Quests.Add(Quest->QuestId, MoveTemp(State));
	RebuildReplicatedQuests();

	RebuildListeners();

	FUTQuestAcceptedSignature AcceptedEvent = OnQuestAccepted;
	AcceptedEvent.Broadcast(Quest);
	return FUTResult::Success();
}

void UUTQuestComponent::AbandonQuest(FName QuestId)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	if (Quests.Remove(QuestId) > 0) {
		RebuildReplicatedQuests();
		RebuildListeners();
	}
}

FUTResult UUTQuestComponent::AdvanceObjective(FName QuestId, FName ObjectiveId, int32 Amount)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return FUTResult::Failure(TEXT("NoAuthority"), FText::FromString(TEXT("AdvanceObjective must run on the server.")));
	}

	FUTActiveQuest* State = Quests.Find(QuestId);
	if (!State || !State->Quest) {
		return FUTResult::Failure(
			TEXT("UnknownQuest"),
			FText::FromString(FString::Printf(TEXT("Quest '%s' is not active."), *QuestId.ToString())));
	}
	if (Amount <= 0) {
		return FUTResult::Failure(
			TEXT("InvalidAmount"),
			FText::FromString(TEXT("AdvanceObjective requires a positive amount.")));
	}

	const FUTObjectiveDef* Objective = State->Quest->Objectives.FindByPredicate(
		[ObjectiveId](const FUTObjectiveDef& Candidate) {
			return Candidate.ObjectiveId == ObjectiveId;
		});
	if (!Objective) {
		return FUTResult::Failure(
			TEXT("UnknownObjective"),
			FText::FromString(
				FString::Printf(TEXT("Quest '%s' has no objective '%s'."), *QuestId.ToString(), *ObjectiveId.ToString())));
	}
	if (State->bCompleted) {
		return FUTResult::Failure(
			TEXT("QuestCompleted"),
			FText::FromString(FString::Printf(TEXT("Quest '%s' is already completed."), *QuestId.ToString())));
	}

	// Clamp at the requirement so a scripted "give 10 of 5" cannot report progress past
	// the objective, and so completion detection is a simple equality check.
	const int32 Current = State->Progress.FindRef(ObjectiveId);
	const int32 NewCount = int32(FMath::Min<int64>(int64(Current) + Amount, Objective->RequiredCount));
	if (NewCount == Current) {
		return FUTResult::Success();
	}
	State->Progress.Add(ObjectiveId, NewCount);
	RebuildReplicatedQuests();

	FUTObjectiveAdvancedSignature AdvancedEvent = OnObjectiveAdvanced;
	AdvancedEvent.Broadcast(State->Quest, ObjectiveId);

	if (AreObjectivesComplete(*State)) {
		State->bCompleted = true;
		RebuildReplicatedQuests();
		RebuildListeners();

		FUTQuestCompletedSignature CompletedEvent = OnQuestCompleted;
		CompletedEvent.Broadcast(State->Quest);
	}
	return FUTResult::Success();
}

FUTQuestProgress UUTQuestComponent::GetQuestProgress(FName QuestId) const
{
	FUTQuestProgress Progress;
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		if (const FUTReplicatedQuest* State = ReplicatedQuests.FindByPredicate(
				[QuestId](const FUTReplicatedQuest& Entry) {
					return Entry.QuestId == QuestId;
				})) {
			Progress.bAccepted = State->bAccepted;
			Progress.bCompleted = State->bCompleted;
			for (const FUTReplicatedObjectiveCount& Objective : State->Objectives) {
				Progress.ObjectiveProgress.Add(Objective.ObjectiveId, Objective.Count);
			}
		}
		return Progress;
	}
	if (const FUTActiveQuest* State = Quests.Find(QuestId)) {
		Progress.bAccepted = true;
		Progress.bCompleted = State->bCompleted;
		Progress.ObjectiveProgress = State->Progress;
		Progress.Definition = State->Quest ? FSoftObjectPath(State->Quest) : State->DefinitionPath;
	}
	return Progress;
}

TArray<FName> UUTQuestComponent::GetAcceptedQuestIds() const
{
	TArray<FName> Ids;
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		for (const FUTReplicatedQuest& Quest : ReplicatedQuests) {
			Ids.Add(Quest.QuestId);
		}
		return Ids;
	}
	Quests.GetKeys(Ids);
	return Ids;
}

void UUTQuestComponent::RegisterQuestDefinition(UUTQuestDefinition* Quest)
{
	if (!Quest || Quest->QuestId.IsNone()) {
		return;
	}
	QuestDefinitions.FindOrAdd(Quest->QuestId) = TSoftObjectPtr<UUTQuestDefinition>(Quest);
	DefinitionCache.Add(Quest->QuestId, Quest);
}

UUTQuestDefinition* UUTQuestComponent::GetOrLoadQuestDefinition(FName QuestId)
{
	return ResolveDefinition(QuestId, FSoftObjectPath());
}

FUTQuestSnapshot UUTQuestComponent::MakeSnapshot() const
{
	FUTQuestSnapshot Snapshot;
	for (const TPair<FName, FUTActiveQuest>& Pair : Quests) {
		FUTQuestProgress Progress;
		Progress.bAccepted = true;
		Progress.bCompleted = Pair.Value.bCompleted;
		Progress.ObjectiveProgress = Pair.Value.Progress;
		Progress.Definition = Pair.Value.Quest ? FSoftObjectPath(Pair.Value.Quest) : Pair.Value.DefinitionPath;
		Snapshot.Quests.Add(Pair.Key, MoveTemp(Progress));
	}
	return Snapshot;
}

void UUTQuestComponent::RestoreFromSnapshot(const FUTQuestSnapshot& Snapshot)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	// Drop the previous listeners up front: the whole objective set is replaced below.
	UnbindListeners();
	Quests.Reset();
	DefinitionCache.Reset();

	for (const TPair<FName, FUTQuestProgress>& Pair : Snapshot.Quests) {
		if (!Pair.Value.bAccepted) {
			continue;
		}

		FUTActiveQuest State;
		State.bCompleted = Pair.Value.bCompleted;
		State.Progress = Pair.Value.ObjectiveProgress;
		State.DefinitionPath = Pair.Value.Definition;
		Quests.Add(Pair.Key, MoveTemp(State));
	}
	RebuildReplicatedQuests();

	// Resolve definitions before subscribing. RebuildListeners only binds tags of quests
	// whose definition is resolved, so an unresolved quest would sit in the data forever,
	// completed by nobody - and the snapshot cannot tell that apart from a dead quest id.
	for (TPair<FName, FUTActiveQuest>& Pair : Quests) {
		Pair.Value.Quest = ResolveDefinition(Pair.Key, Pair.Value.DefinitionPath);
		if (!Pair.Value.Quest) {
			UE_LOG(
				LogUTQuests, Warning,
				TEXT(
					"Quest '%s' was restored without its definition: register the asset in QuestDefinitions or Quests.AllQuests. Progress is kept but no objective can advance."),
				*Pair.Key.ToString());
		}
	}

	RebuildListeners();

	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.
}

void UUTQuestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindListeners();
	Super::EndPlay(EndPlayReason);
}

void UUTQuestComponent::UnbindListeners()
{
	if (UUTMessageBusSubsystem* Bus = CachedBus.Get()) {
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : ObjectiveListeners) {
			Bus->RemoveListener(Pair.Value);
		}
	}
	ObjectiveListeners.Reset();
}

void UUTQuestComponent::RebuildListeners()
{
	UnbindListeners();

	// Objectives advance server-side progress; clients receive counts via replication.
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}

	UUTMessageBusSubsystem* Bus = ResolveBus();
	if (!Bus) {
		return;
	}

	TSet<FGameplayTag> Tags;
	for (const TPair<FName, FUTActiveQuest>& Pair : Quests) {
		const FUTActiveQuest& State = Pair.Value;
		if (State.bCompleted || !State.Quest) {
			continue;
		}
		for (const FUTObjectiveDef& Objective : State.Quest->Objectives) {
			if (Objective.EventTag.IsValid()) {
				Tags.Add(Objective.EventTag);
			}
		}
	}

	for (const FGameplayTag& Tag : Tags) {
		// The subscribed tag is bound into the callback. The bus delivers a broadcast to
		// every ancestor tag, so a component listening on both Combat and Combat.Damage is
		// called twice for one Combat.Damage.Fire broadcast; without the bound tag each
		// call would walk every objective and advance the same one twice.
		const FDelegateHandle Handle = Bus->Listen(
			Tag,
			UUTMessageBusSubsystem::FUTMessageCallback::CreateUObject(this, &UUTQuestComponent::HandleObjectiveEvent, Tag));
		ObjectiveListeners.Add(Tag, Handle);
	}
}

void UUTQuestComponent::HandleObjectiveEvent(const FUTMessage& Message, FGameplayTag ObjectiveTag)
{
	// Only objectives owned by this listener's tag. A deeper broadcast still reaches this
	// listener, which is how "Kill" advances on Kill.Dragon, but the objective itself is
	// counted exactly once no matter how many tags are in flight.
	bool bStructureChanged = false;
	for (TPair<FName, FUTActiveQuest>& Pair : Quests) {
		FUTActiveQuest& State = Pair.Value;
		if (State.bCompleted || !State.Quest) {
			continue;
		}

		for (const FUTObjectiveDef& Objective : State.Quest->Objectives) {
			if (Objective.EventTag != ObjectiveTag) {
				continue;
			}

			const int32 Current = State.Progress.FindRef(Objective.ObjectiveId);
			if (Current >= Objective.RequiredCount) {
				continue;
			}

			State.Progress.Add(Objective.ObjectiveId, FMath::Min(Current + 1, Objective.RequiredCount));

			FUTObjectiveAdvancedSignature AdvancedEvent = OnObjectiveAdvanced;
			AdvancedEvent.Broadcast(State.Quest, Objective.ObjectiveId);
		}

		if (AreObjectivesComplete(State)) {
			State.bCompleted = true;
			bStructureChanged = true;

			FUTQuestCompletedSignature CompletedEvent = OnQuestCompleted;
			CompletedEvent.Broadcast(State.Quest);
		}
	}

	RebuildReplicatedQuests();
	if (bStructureChanged) {
		RebuildListeners();
	}
}

bool UUTQuestComponent::ArePrerequisitesMet(const UUTQuestDefinition* Quest) const
{
	for (const FName& PrerequisiteId : Quest->PrerequisiteQuestIds) {
		const FUTActiveQuest* State = Quests.Find(PrerequisiteId);
		if (!State || !State->bCompleted) {
			return false;
		}
	}
	return true;
}

bool UUTQuestComponent::AreObjectivesComplete(const FUTActiveQuest& State) const
{
	if (!State.Quest || State.Quest->Objectives.IsEmpty()) {
		return false;
	}

	// Mirrors UUTQuestDefinition::Objectives: every objective at RequiredCount. Objectives
	// without an EventTag can only be finished through AdvanceObjective.
	for (const FUTObjectiveDef& Objective : State.Quest->Objectives) {
		if (State.Progress.FindRef(Objective.ObjectiveId) < Objective.RequiredCount) {
			return false;
		}
	}
	return true;
}

UUTQuestDefinition* UUTQuestComponent::ResolveDefinition(FName QuestId, const FSoftObjectPath& FallbackPath)
{
	if (const TObjectPtr<UUTQuestDefinition>* Cached = DefinitionCache.Find(QuestId)) {
		return Cached->Get();
	}
	if (const FUTActiveQuest* State = Quests.Find(QuestId); State && State->Quest) {
		DefinitionCache.Add(QuestId, State->Quest);
		return State->Quest;
	}

	// Configured registry first, snapshot path as fallback. The path keeps a save written
	// with a differently-named registry recoverable; the registry keeps the asset alive in
	// a cooked build without baking asset references into the save file.
	TSoftObjectPtr<UUTQuestDefinition> Soft;
	if (const TSoftObjectPtr<UUTQuestDefinition>* Registered = QuestDefinitions.Find(QuestId)) {
		Soft = *Registered;
	}
	else if (!FallbackPath.IsNull()) {
		Soft = TSoftObjectPtr<UUTQuestDefinition>(FallbackPath);
	}
	if (Soft.IsNull()) {
		return nullptr;
	}

	UUTQuestDefinition* Loaded = Soft.LoadSynchronous();
	if (Loaded) {
		DefinitionCache.Add(QuestId, Loaded);
	}
	return Loaded;
}

UUTMessageBusSubsystem* UUTQuestComponent::ResolveBus()
{
	if (!CachedBus.IsValid()) {
		CachedBus = UUTMessageBusSubsystem::Get(this);
	}
	return CachedBus.Get();
}
