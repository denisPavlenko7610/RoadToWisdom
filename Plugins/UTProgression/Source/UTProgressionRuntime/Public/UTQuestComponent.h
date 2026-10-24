// UT Templates - quest tracker component.
#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UTMessageTypes.h"
#include "UTResult.h"
#include "UTQuestDefinition.h"
#include "UTQuestComponent.generated.h"

class UUTMessageBusSubsystem;

/** Per-objective progress for one quest. */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bAccepted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bCompleted = false;

	/** ObjectiveId -> current count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TMap<FName, int32> ObjectiveProgress;

	/**
	 * Definition asset this progress came from. The snapshot stores an id per quest, so
	 * without the path a fresh component could not rebuild live state from a save; the path
	 * is resolved against the registry first and only used as a fallback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FSoftObjectPath Definition;
};

/** Persistent snapshot (accepted quests + progress). */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTQuestSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TMap<FName, FUTQuestProgress> Quests;
};

/** One replicated objective counter (TMap is not replicable). */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTReplicatedObjectiveCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int32 Count = 0;
};

/** Compact replicated quest state: ids and counts travel; definitions stay local assets. */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTReplicatedQuest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bAccepted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bCompleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TArray<FUTReplicatedObjectiveCount> Objectives;
};

/** Live quest state. Reflected so the definition reference is visible to the GC. */
USTRUCT()
struct UTPROGRESSIONRUNTIME_API FUTActiveQuest
{
	GENERATED_BODY()

	/**
	 * Quest definition asset. Only safe because the owning component keeps the whole map
	 * in a UPROPERTY - a plain TMap<TObjectPtr<>> is invisible to the GC and the definition
	 * gets collected while the quest is still in progress.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UUTQuestDefinition> Quest = nullptr;

	UPROPERTY(Transient)
	TMap<FName, int32> Progress;

	UPROPERTY(Transient)
	bool bCompleted = false;

	/** Snapshot fallback path; resolved on restore, never serialized on its own. */
	FSoftObjectPath DefinitionPath;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTQuestAcceptedSignature, UUTQuestDefinition*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTQuestCompletedSignature, UUTQuestDefinition*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUTObjectiveAdvancedSignature, UUTQuestDefinition*, Quest, FName, ObjectiveId);

/**
 * Quest tracker: accepts UUTQuestDefinition data assets, advances objectives from UT
 * message bus broadcasts (an objective with tag "Kill.Dragon" advances on every broadcast
 * of that tag or its children) and completes quests when all objectives finish.
 *
 * Rewards are not handled here - OnQuestCompleted hands the definition to game code,
 * which grants items/XP through the other modules.
 *
 * Restoring a snapshot only carries QuestId + counts, so live state is rebuilt by looking
 * each id up in QuestDefinitions (an editor- or game-populated registry of the assets this
 * component can own). Register your assets there in BeginPlay, or pass AcceptQuest /
 * RegisterQuestDefinition once so the save-load path never depends on a level actor.
 */
UCLASS(ClassGroup = (UT), meta = (BlueprintSpawnableComponent))
class UTPROGRESSIONRUNTIME_API UUTQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|Quests")
	FUTQuestAcceptedSignature OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "UT|Quests")
	FUTQuestCompletedSignature OnQuestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "UT|Quests")
	FUTObjectiveAdvancedSignature OnObjectiveAdvanced;

	/** Accept a quest. Fails on duplicates and unmet prerequisites. */
	UFUNCTION(BlueprintCallable, Category = "UT|Quests")
	FUTResult AcceptQuest(UUTQuestDefinition* Quest);

	UFUNCTION(BlueprintCallable, Category = "UT|Quests")
	void AbandonQuest(FName QuestId);

	/**
	 * Advance an objective by hand, for goals no event tag can express (talk to an NPC,
	 * pick a lock). Clamped at the objective requirement and completes the quest with
	 * OnQuestCompleted when every objective reaches RequiredCount.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Quests")
	FUTResult AdvanceObjective(FName QuestId, FName ObjectiveId, int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "UT|Quests")
	FUTQuestProgress GetQuestProgress(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "UT|Quests")
	bool IsQuestCompleted(FName QuestId) const { return GetQuestProgress(QuestId).bCompleted; }

	UFUNCTION(BlueprintPure, Category = "UT|Quests")
	TArray<FName> GetAcceptedQuestIds() const;

	/**
	 * Register a definition in the QuestId -> asset registry used to rebuild live state
	 * from a snapshot. Populate QuestDefinitions in the editor, or call this from your own
	 * quest registry before RestoreFromSnapshot.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Quests")
	void RegisterQuestDefinition(UUTQuestDefinition* Quest);

	/** Definition for a quest id, loaded from the registry on demand; null when unknown. */
	UFUNCTION(BlueprintCallable, Category = "UT|Quests")
	UUTQuestDefinition* GetOrLoadQuestDefinition(FName QuestId);

	/**
	 * QuestId -> definition registry, consulted by the restore path. A soft reference is
	 * what makes a restored quest survive a component that never had the asset in memory.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Quests")
	TMap<FName, TSoftObjectPtr<UUTQuestDefinition>> QuestDefinitions;

	UFUNCTION(BlueprintPure, Category = "UT|Quests")
	FUTQuestSnapshot MakeSnapshot() const;

	/** Restore quests and progress without publishing events (save-load path). */
	UFUNCTION(BlueprintCallable, Category = "UT|Quests")
	void RestoreFromSnapshot(const FUTQuestSnapshot& Snapshot);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class FUTQuestReplicationTest;
	UUTQuestComponent();

	/** Server: rebuild the compact replicated view from the live state. */
	void RebuildReplicatedQuests();

	UFUNCTION()
	void OnRep_Quests();

	/** Client-side last seen state, to diff replication into gameplay events. */
	TMap<FName, FUTReplicatedQuest> LastSeenReplicatedQuests;

	/** Server-authoritative progress, replicated compactly to clients. */
	UPROPERTY(ReplicatedUsing = OnRep_Quests, VisibleAnywhere, Category = "UT")
	TArray<FUTReplicatedQuest> ReplicatedQuests;

	/**
	 * Live quest state. UPROPERTY so the GC keeps the referenced definitions alive and a
	 * native TMap would not - see FUTActiveQuest. Authority-only.
	 */
	UPROPERTY(Transient)
	TMap<FName, FUTActiveQuest> Quests;

	/** Hard references to assets already resolved through QuestDefinitions. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UUTQuestDefinition>> DefinitionCache;

	TMap<FGameplayTag, FDelegateHandle> ObjectiveListeners;
	TWeakObjectPtr<UUTMessageBusSubsystem> CachedBus;

	void UnbindListeners();
	void RebuildListeners();
	void HandleObjectiveEvent(const FUTMessage& Message, FGameplayTag ObjectiveTag);
	bool ArePrerequisitesMet(const UUTQuestDefinition* Quest) const;
	bool AreObjectivesComplete(const FUTActiveQuest& State) const;
	UUTQuestDefinition* ResolveDefinition(FName QuestId, const FSoftObjectPath& FallbackPath);
	UUTMessageBusSubsystem* ResolveBus();
};
