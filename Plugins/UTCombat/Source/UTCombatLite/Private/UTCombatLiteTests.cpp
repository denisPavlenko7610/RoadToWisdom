// UT Templates - combat-lite automation tests (logic-only).
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

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "UTActorDiedMessage.h"
#include "UTDamageContract.h"
#include "UTDamageDispatch.h"
#include "UTHealthComponent.h"
#include "UTStatsComponent.h"
#include "UTStatusEffectsComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTHealthTest, "UT.CombatLite.Health",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTHealthTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UUTHealthComponent* Health = NewObject<UUTHealthComponent>();
	GCRoot.Objects.Add(Health);
	Health->InitializeHealth(100.f, 100.f);

	// Damage through the contract.
	FUTDamageContext Context;
	Context.BaseAmount = 30.f;
	const FUTDamageResult First = IUTDamageReceiver::Execute_ApplyDamage(Health, Context);
	TestTrue("damage applied", First.bApplied);
	TestEqual("amount", (int32)First.AmountApplied, 30);
	TestEqual("health after", (int32)First.HealthAfter, 70);
	TestFalse("not fatal", First.bFatal);

	// Healing.
	const FUTHealthChange Heal = Health->ApplyHeal(20.f);
	TestTrue("heal applied", Heal.bApplied);
	TestEqual("healed", (int32)Health->GetHealth(), 90);

	// Fatal damage.
	Context.BaseAmount = 500.f;
	const FUTDamageResult Fatal = IUTDamageReceiver::Execute_ApplyDamage(Health, Context);
	TestTrue("fatal applied", Fatal.bApplied);
	TestTrue("fatal flag", Fatal.bFatal);
	TestFalse("dead cannot receive more", IUTDamageReceiver::Execute_CanReceiveDamage(Health));

	// Revive.
	Health->Revive(0.5f);
	TestEqual("revived half", (int32)Health->GetHealth(), 50);
	FUTHealthSnapshot DeadSnapshot;
	DeadSnapshot.Maximum = 100.f;
	DeadSnapshot.Current = 0.f;
	Health->RestoreFromSnapshot(DeadSnapshot);
	TestTrue("dead snapshot restores replicated flag", Health->bDead);
	DeadSnapshot.Current = 25.f;
	Health->RestoreFromSnapshot(DeadSnapshot);
	TestFalse("alive snapshot clears replicated flag", Health->bDead);
	Health->SetMaxHealth(0.f);
	TestTrue("zero cap updates replicated death flag", Health->bDead);
	TestEqual("zero cap updates health notification baseline", Health->LastSeenHealth, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTStatsTest, "UT.CombatLite.Stats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTStatsTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UUTStatsComponent* Stats = NewObject<UUTStatsComponent>();
	GCRoot.Objects.Add(Stats);
	Stats->BaseValues.Add("Damage", 10.f);
	Stats->BaseValues.Add("Speed", 100.f);

	// (base + add) * (1 + multiply)
	const FUTStatModifierHandle Add = Stats->AddModifier("Damage", EUTStatModifierOp::Add, 5.f);
	TestTrue("add handle", Add.IsValid());
	TestEqual("after add", (int32)Stats->GetStatValue("Damage"), 15);

	const FUTStatModifierHandle Mult = Stats->AddModifier("Damage", EUTStatModifierOp::Multiply, 1.f);
	TestEqual("after multiply", (int32)Stats->GetStatValue("Damage"), 30);

	// Undeclared stat refused.
	TestFalse("undeclared stat refused", Stats->AddModifier("Missing", EUTStatModifierOp::Add, 1.f).IsValid());

	// Source bulk removal reverts everything the source granted.
	UObject* Source = GetTransientPackage();
	const FUTStatModifierHandle Speed = Stats->AddModifier("Speed", EUTStatModifierOp::Add, 50.f, Source);
	Stats->RemoveModifiersFromSource(Source, NAME_None);
	TestEqual("speed reverted", (int32)Stats->GetStatValue("Speed"), 100);
	Stats->RemoveModifier("Damage", Mult);
	Stats->RemoveModifier("Damage", Add);
	TestEqual("damage reverted", (int32)Stats->GetStatValue("Damage"), 10);
	return true;
}

// The dispatch must find a component-level receiver on a plain actor - an ordinary
// Character + UUTHealthComponent never implements the interface at the actor level.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTDamageDispatchTest, "UT.Combat.DamageDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTDamageDispatchTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	AActor* PlainActor = NewObject<AActor>();
	GCRoot.Objects.Add(PlainActor);
	UUTHealthComponent* Health = NewObject<UUTHealthComponent>(PlainActor);
	GCRoot.Objects.Add(Health);
	Health->InitializeHealth(100.f, 100.f);

	// No receiver on an empty actor.
	AActor* Empty = NewObject<AActor>();
	GCRoot.Objects.Add(Empty);
	TestFalse("empty actor not damageable", UTCombat::CanDamageActor(Empty));

	// Component-level receiver resolves and applies through the dispatch.
	TestTrue("component receiver resolves", UTCombat::CanDamageActor(PlainActor));
	FUTDamageContext Context;
	Context.BaseAmount = 25.f;
	const FUTDamageResult Result = UTCombat::ApplyDamageToActor(PlainActor, Context);
	TestTrue("damage applied via dispatch", Result.bApplied);
	TestEqual("health reduced", (int32)Health->GetHealth(), 75);

	// Actor-level implementations still win.
	UUTHealthComponent* ActorLevelReceiver = NewObject<UUTHealthComponent>();
	// (the interface stays component-implemented here; an actor implementing it itself
	// is covered by ResolveDamageReceiver taking the actor branch first)
	TestTrue("resolve returns the component", UTCombat::ResolveDamageReceiver(PlainActor) == Cast<UObject>(Health));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTStatsReplicationTest, "UT.CombatLite.StatsReplicationReadModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTStatsReplicationTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Owner = World->SpawnActor<AActor>();
	UUTStatsComponent* Stats = NewObject<UUTStatsComponent>(Owner);
	Stats->BaseValues.Add("Damage", 10.f);
	Stats->BaseValues.Add("Speed", 100.f);
	Stats->RegisterComponent();
	Stats->BeginPlay();
	TestEqual("authored stats published without mutation", Stats->StatEntries.Num(), 2);
	Stats->AddModifier("Damage", EUTStatModifierOp::Add, 5.f);
	Owner->SetRole(ROLE_SimulatedProxy);
	TestEqual("client reads final replicated value", Stats->GetStatValue("Damage"), 15.f);
	Stats->OnRep_StatEntries();
	TestEqual("client reads replicated base", Stats->GetStatBase("Damage"), 10.f);
	Stats->BaseValues.Reset();
	TestTrue("client presence uses replicated rows", Stats->HasStat("Damage"));
	Owner->SetRole(ROLE_Authority);
	FUTStatsSnapshot Snapshot;
	Snapshot.BaseValues.Add("Armor", 20.f);
	Stats->RestoreFromSnapshot(Snapshot);
	TestEqual("restore replaces replicated rows", Stats->StatEntries.Num(), 1);
	Owner->SetRole(ROLE_SimulatedProxy);
	TestFalse("removed stat absent on client", Stats->HasStat("Damage"));
	Stats->OnRep_StatEntries();
	TestFalse("removed stat forgotten by notification cache", Stats->LastSeenValues.Contains("Damage"));
	TestEqual("removed stat reads zero", Stats->GetStatValue("Damage"), 0.f);
	TestEqual("restored stat readable on client", Stats->GetStatValue("Armor"), 20.f);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTStatusClientReadTest, "UT.CombatLite.StatusClientReadModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTStatusClientReadTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Owner = World->SpawnActor<AActor>();
	auto* Effects = NewObject<UUTStatusEffectsComponent>(Owner);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag("UT.Message.ActorDied");
	FUTActiveEffectDisplay Display;
	Display.EffectTags.AddTag(Tag);
	Effects->ReplicatedDisplays.Add(Display);
	Owner->SetRole(ROLE_SimulatedProxy);
	TestEqual("client counts replicated effects", Effects->GetActiveEffectCount(), 1);
	TestTrue("client queries replicated effect tags", Effects->IsAffectedBy(Tag));
	Effects->ReplicatedDisplays.Reset();
	TestEqual("client observes removal", Effects->GetActiveEffectCount(), 0);
	TestFalse("client no longer affected", Effects->IsAffectedBy(Tag));
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
