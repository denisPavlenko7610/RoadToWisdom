// UT Templates - items/inventory automation tests (logic-only).
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

#include "UTInventoryComponent.h"
#include "UTItemDefinition.h"
#include "UTItemRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTItemRegistryTest, "UT.Items.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTItemRegistryTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UGameInstance* FakeInstance = NewObject<UGameInstance>(GetTransientPackage());
	GCRoot.Objects.Add(FakeInstance);
	UUTItemRegistrySubsystem* Registry = NewObject<UUTItemRegistrySubsystem>(FakeInstance);
	GCRoot.Objects.Add(Registry);
	Registry->RegisterItem("Test.Potion", TSoftObjectPtr<UUTItemDefinition>(NewObject<UUTItemDefinition>()));
	// The soft pointer needs a live object: re-register with a persistent definition.
	UUTItemDefinition* Potion = NewObject<UUTItemDefinition>();
	Potion->ItemId = "Test.Potion";
	Potion->MaxStackSize = 5;
	Registry->RegisterItem("Test.Potion", Potion);

	UUTItemDefinition* Sword = NewObject<UUTItemDefinition>();
	Sword->ItemId = "Test.Sword";
	Sword->MaxStackSize = 1;
	Registry->RegisterItem("Test.Sword", Sword);

	const UUTItemDefinition* Resolved = Registry->GetItemDefinition("Test.Potion");
	TestNotNull("potion resolves", Resolved);
	TestEqual("potion stack size", Registry->GetMaxStackSize("Test.Potion"), 5);
	TestEqual("sword never stacks", Registry->GetMaxStackSize("Test.Sword"), 1);
	TestEqual("unknown item", Registry->GetMaxStackSize("Test.Unknown"), 1);
	TestFalse("unknown not known", Registry->IsItemKnown("Test.Unknown"));
	Registry->UnregisterItem("Test.Potion");
	TestNull("unregistered item gone", Registry->GetItemDefinition("Test.Potion"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTInventoryTest, "UT.Items.Inventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTInventoryTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UUTInventoryComponent* Inventory = NewObject<UUTInventoryComponent>();
	GCRoot.Objects.Add(Inventory);
	Inventory->MaxSlots = 3;
	Inventory->DefaultMaxStackSize = 10;

	// Stacking: two adds fill one stack of 10, then open a second slot.
	TestEqual("first add", Inventory->AddItem("Test.Gem", 7), 7);
	TestEqual("second add same stack", Inventory->AddItem("Test.Gem", 5), 5);
	TestEqual("gem count", Inventory->GetItemCount("Test.Gem"), 12);
	TestEqual("slots used", Inventory->GetUsedSlotCount(), 2);

	// Max slots: the third slot fills to the 30-item cap, the overflow is refused.
	TestEqual("third slot add", Inventory->AddItem("Test.Gem", 20), 18);
	TestEqual("partial add beyond slots", Inventory->AddItem("Test.Gem", 50), 0);

	// Atomic consumption: insufficient count leaves the inventory untouched.
	TestFalse("consume more than owned fails", Inventory->ConsumeItems("Test.Gem", 100));
	TestEqual("untouched after failed consume", Inventory->GetItemCount("Test.Gem"), 30);
	TestTrue("consume exact amount", Inventory->ConsumeItems("Test.Gem", 30));
	TestEqual("empty after consume", Inventory->GetItemCount("Test.Gem"), 0);
	TestEqual("slots freed", Inventory->GetUsedSlotCount(), 0);

	TestFalse("negative consume rejected", Inventory->ConsumeItems("Test.Gem", -1));
	TestFalse("zero consume rejected", Inventory->ConsumeItems("Test.Gem", 0));

	// Snapshot round trip (save/load path publishes no events; here we check data only).
	Inventory->AddItem("Test.Gem", 4);
	const FUTInventorySnapshot Snapshot = Inventory->MakeSnapshot();
	Inventory->ClearInventory();
	TestEqual("cleared", Inventory->GetItemCount("Test.Gem"), 0);
	Inventory->RestoreFromSnapshot(Snapshot);
	TestEqual("restored", Inventory->GetItemCount("Test.Gem"), 4);
	return true;
}

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTItemInstanceTest, "UT.Items.Instances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTItemInstanceTest::RunTest(const FString&)
{
	UUTInventoryComponent* Inventory = NewObject<UUTInventoryComponent>();
	UTTestGC::FGCRoot Root;
	Root.Objects.Add(Inventory);
	Inventory->MaxSlots = 2;
	TestEqual("stack takes one slot", Inventory->AddItem("Test.Wood", 5), 5);
	FUTItemInstance Weapon = FUTItemInstance::Create("Test.Weapon");
	Weapon.State.Ammo = 7;
	Weapon.State.Durability = 0.5f;
	Weapon.State.CustomSchema = "Test.QuestState";
	Weapon.State.CustomVersion = 1;
	Weapon.State.CustomData = {1, 2, 3};
	FUTItemRoll Roll;
	Roll.ModifierId = "Damage";
	Roll.Value = 2.5f;
	Weapon.State.Rolls.Add(Roll);
	TestTrue("add unique", Inventory->AddInstance(Weapon));
	TestFalse("duplicate identity refused", Inventory->AddInstance(Weapon));
	TestEqual("shared slot budget", Inventory->AddItem("Test.Stone", 1), 0);
	TestEqual("stack removal never consumes unique", Inventory->RemoveItem(Weapon.ItemId, 1), 0);
	FUTInventorySnapshot Snapshot = Inventory->MakeSnapshot();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive WriteArchive(Writer, false);
	FUTInventorySnapshot::StaticStruct()->SerializeItem(WriteArchive, &Snapshot, nullptr);
	FUTInventorySnapshot Loaded;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive ReadArchive(Reader, false);
	FUTInventorySnapshot::StaticStruct()->SerializeItem(ReadArchive, &Loaded, nullptr);
	TestFalse("serialization succeeded", ReadArchive.IsError());
	Inventory->ClearInventory();
	Inventory->RestoreFromSnapshot(Loaded);
	FUTItemInstance Copy;
	TestTrue("identity survives save", Inventory->GetInstance(Weapon.InstanceId, Copy));
	TestEqual("ammo survives save", Copy.State.Ammo, 7);
	TestEqual("durability survives save", Copy.State.Durability, 0.5f);
	TestTrue("custom bytes survive save", Copy.State.CustomData == Weapon.State.CustomData);
	TestEqual("rolled modifier survives save", Copy.State.Rolls[0].Value, 2.5f);
	FUTItemInstanceState Changed = Copy.State;
	Changed.Ammo = 3;
	TestTrue("state update", Inventory->SetInstanceState(Weapon.InstanceId, Changed));
	Changed.CustomData.SetNum(FUTItemInstanceState::MaxCustomDataBytes + 1);
	TestFalse("oversized custom state refused", Inventory->SetInstanceState(Weapon.InstanceId, Changed));
	Loaded.Instances.Add(Weapon);
	Inventory->RestoreFromSnapshot(Loaded);
	Inventory->GetInstance(Weapon.InstanceId, Copy);
	TestEqual("invalid snapshot leaves live state untouched", Copy.State.Ammo, 3);
	TestTrue("remove exact identity", Inventory->RemoveInstance(Weapon.InstanceId, Copy));
	TestFalse("cannot remove twice", Inventory->RemoveInstance(Weapon.InstanceId, Copy));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	AActor* Client = World->SpawnActor<AActor>();
	UUTInventoryComponent* ClientInventory = NewObject<UUTInventoryComponent>(Client);
	Client->SetRole(ROLE_SimulatedProxy);
	TestFalse("client cannot add unique", ClientInventory->AddInstance(Weapon));
	TestFalse("client cannot edit unique", ClientInventory->SetInstanceState(Weapon.InstanceId, Weapon.State));
	TestFalse("client cannot consume", ClientInventory->ConsumeItems("Test.Wood", 1));
	World->DestroyWorld(false);
	return true;
}

#include "UTInventoryTestObserver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTInventoryNotificationTest, "UT.Items.CommittedRemovalNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTInventoryNotificationTest::RunTest(const FString&)
{
	UTTestGC::FGCRoot Root;
	auto* Inventory = NewObject<UUTInventoryComponent>();
	auto* Observer = NewObject<UUTInventoryTestObserver>();
	Root.Objects.Add(Observer);
	Observer->Inventory = Inventory;
	Inventory->AddItem("Test.Gem", 1);
	Inventory->OnInventoryChanged.AddDynamic(Observer, &UUTInventoryTestObserver::Changed);
	// Model the engine's receive order: PreReplicatedRemove precedes physical erase.
	TArray<int32> Removed{0};
	Inventory->ReplicatedStacks.PreReplicatedRemove(Removed, 0);
	TestEqual("aggregate event waits for committed removal", Observer->Notifications, 0);
	Inventory->ReplicatedStacks.Items.Reset();
	FFastArraySerializer::FPostReplicatedReceiveParameters Parameters{};
	Inventory->ReplicatedStacks.PostReplicatedReceive(Parameters);
	TestEqual("one aggregate event", Observer->Notifications, 1);
	TestEqual("listener reads committed count", Observer->ObservedCount, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
