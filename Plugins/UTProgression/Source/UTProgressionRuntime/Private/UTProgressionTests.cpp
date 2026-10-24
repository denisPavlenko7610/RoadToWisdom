// UT Templates - progression domain automation tests (logic-only).
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "UObject/GCObject.h"

// Keeps test-created UObjects alive across any GC pass during the test.
namespace UTTestGC
{
struct FGCRoot : public FGCObject
{
	TArray<TObjectPtr<UObject>> Objects;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (TObjectPtr<UObject>& Object : Objects) {
			Collector.AddReferencedObject(Object);
		}
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("UTTestGCRoot");
	}
};
} // namespace UTTestGC

#include "UTProgressionComponent.h"
#include "UTProgressionTestObserver.h"
#include "UTWalletComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTWalletTest, "UT.Progression.Wallet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTWalletTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UUTWalletComponent* Wallet = NewObject<UUTWalletComponent>();
	GCRoot.Objects.Add(Wallet);

	TestTrue("add gold", Wallet->Add("Gold", 100).IsSuccess());
	TestTrue("add silver", Wallet->Add("Silver", 50).IsSuccess());
	TestEqual("gold balance", Wallet->GetBalance("Gold"), 100);

	// Atomic spend: either the full amount leaves, or nothing changes.
	TestFalse("cannot overspend", Wallet->TrySpend("Gold", 200).IsSuccess());
	TestEqual("untouched after failed spend", Wallet->GetBalance("Gold"), 100);
	TestTrue("exact spend", Wallet->TrySpend("Gold", 40).IsSuccess());
	TestEqual("after spend", Wallet->GetBalance("Gold"), 60);

	// Negative amounts rejected.
	TestFalse("negative add", Wallet->Add("Gold", -5).IsSuccess());
	TestEqual("no change from negative add", Wallet->GetBalance("Gold"), 60);
	Wallet->SetBalance("Limit", MAX_int64);
	TestFalse("overflow rejected", Wallet->Add("Limit", 1).IsSuccess());
	TestEqual("overflow leaves balance intact", Wallet->GetBalance("Limit"), MAX_int64);

	// Snapshot round trip.
	const FUTWalletSnapshot Snapshot = Wallet->MakeSnapshot();
	Wallet->SetBalance("Gold", 0);
	TestEqual("zeroed", Wallet->GetBalance("Gold"), 0);
	Wallet->RestoreFromSnapshot(Snapshot);
	TestEqual("restored", Wallet->GetBalance("Gold"), 60);
	auto* Observer = NewObject<UUTProgressionTestObserver>();
	GCRoot.Objects.Add(Observer);
	Wallet->OnBalanceChanged.AddDynamic(Observer, &UUTProgressionTestObserver::BalanceChanged);
	Wallet->RestoreFromSnapshot(FUTWalletSnapshot());
	Wallet->OnRep_BalanceEntries();
	TestEqual("removed balances notify zero", Observer->LastBalance, int64(0));
	TestTrue("removed currencies leave notification cache", Wallet->ReplicatedBalancesLastSeen.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTProgressionTest, "UT.Progression.XpLevels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTProgressionTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UUTProgressionComponent* Progression = NewObject<UUTProgressionComponent>();
	GCRoot.Objects.Add(Progression);
	Progression->BaseXpPerLevel = 100;

	TestEqual("starts level 1", Progression->GetLevel(), 1);
	TestEqual("first grant", Progression->AddXp(150), 1);
	TestEqual("level 2", Progression->GetLevel(), 2);
	TestEqual("multi-level grant", Progression->AddXp(250), 3);
	TestEqual("level 5", Progression->GetLevel(), 5);
	TestEqual("total xp", (int32)Progression->GetTotalXp(), 400);
	TestFalse("negative xp refused", Progression->AddXp(-10) > 0);
	TestEqual("xp to next", Progression->GetXpToNextLevel(), 100);
	return true;
}

#include "UTQuestComponent.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTQuestReplicationTest, "UT.Progression.QuestReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTQuestReplicationTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Owner = World->SpawnActor<AActor>();
	UUTQuestComponent* Quest = NewObject<UUTQuestComponent>(Owner);
	UUTQuestDefinition* Definition = NewObject<UUTQuestDefinition>();
	Definition->QuestId = "Test.Quest";
	FUTObjectiveDef Objective;
	Objective.ObjectiveId = "Test.Objective";
	Objective.RequiredCount = 3;
	Definition->Objectives.Add(Objective);
	Quest->AcceptQuest(Definition);
	Quest->AdvanceObjective(Definition->QuestId, Objective.ObjectiveId, 1);
	TestEqual("partial progress replicated", Quest->ReplicatedQuests[0].Objectives[0].Count, 1);
	Quest->AdvanceObjective(Definition->QuestId, Objective.ObjectiveId, MAX_int32);
	TestEqual("large grant clamps without overflow", Quest->ReplicatedQuests[0].Objectives[0].Count, 3);
	Owner->SetRole(ROLE_SimulatedProxy);
	TestEqual(
		"client reads replicated progress",
		Quest->GetQuestProgress(Definition->QuestId).ObjectiveProgress.FindRef(Objective.ObjectiveId), 3);
	TestEqual("client reads accepted ids", Quest->GetAcceptedQuestIds().Num(), 1);
	TestFalse("client cannot advance", Quest->AdvanceObjective(Definition->QuestId, Objective.ObjectiveId).IsSuccess());
	Owner->SetRole(ROLE_Authority);
	Quest->RestoreFromSnapshot(FUTQuestSnapshot());
	TestTrue("empty restore clears replicated quests", Quest->ReplicatedQuests.IsEmpty());
	auto* Observer = NewObject<UUTProgressionTestObserver>();
	UTTestGC::FGCRoot Root;
	Root.Objects.Add(Observer);
	Quest->OnQuestAccepted.AddDynamic(Observer, &UUTProgressionTestObserver::QuestAccepted);
	Quest->AcceptQuest(Definition);
	Owner->SetRole(ROLE_SimulatedProxy);
	Observer->Accepted = 0;
	Quest->OnRep_Quests();
	TestEqual("first acceptance observed on client", Observer->Accepted, 1);
	Owner->SetRole(ROLE_Authority);
	Quest->AbandonQuest(Definition->QuestId);
	Owner->SetRole(ROLE_SimulatedProxy);
	Quest->OnRep_Quests();
	TestTrue("abandoned quest leaves client notification cache", Quest->LastSeenReplicatedQuests.IsEmpty());
	Owner->SetRole(ROLE_Authority);
	Quest->AcceptQuest(Definition);
	Owner->SetRole(ROLE_SimulatedProxy);
	Observer->Accepted = 0;
	Quest->OnRep_Quests();
	TestEqual("reacceptance observed on client", Observer->Accepted, 1);
	Owner->SetRole(ROLE_Authority);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTProgressionRestoreAuthorityTest, "UT.Progression.RestoreAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTProgressionRestoreAuthorityTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Owner = World->SpawnActor<AActor>();
	auto* Progression = NewObject<UUTProgressionComponent>(Owner);
	Progression->AddXp(50);
	auto* Observer = NewObject<UUTProgressionTestObserver>();
	UTTestGC::FGCRoot Root;
	Root.Objects.Add(Observer);
	Progression->BaseXpPerLevel = 100;
	Progression->OnLevelUp.AddDynamic(Observer, &UUTProgressionTestObserver::LevelUp);
	FUTProgressionSnapshot Snapshot;
	Snapshot.TotalXp = 1000;
	Owner->SetRole(ROLE_SimulatedProxy);
	Progression->RestoreFromSnapshot(Snapshot);
	TestEqual("client restore rejected", Progression->GetTotalXp(), int64(50));
	Owner->SetRole(ROLE_Authority);
	Snapshot.TotalXp = 250;
	Progression->RestoreFromSnapshot(Snapshot);
	Owner->SetRole(ROLE_SimulatedProxy);
	Progression->OnRep_TotalXp();
	TestTrue("client reports each crossed level", Observer->Levels == TArray<int32>({2, 3}));
	Owner->SetRole(ROLE_Authority);
	Snapshot.TotalXp = 0;
	Progression->RestoreFromSnapshot(Snapshot);
	Owner->SetRole(ROLE_SimulatedProxy);
	Progression->OnRep_TotalXp();
	TestEqual("downward restore resets observed level", Progression->LastSeenLevel, 1);
	Owner->SetRole(ROLE_Authority);
	Snapshot.TotalXp = MAX_int64;
	Progression->RestoreFromSnapshot(Snapshot);
	Progression->AddXp(1);
	TestEqual("xp saturates without signed overflow", Progression->GetTotalXp(), MAX_int64);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
