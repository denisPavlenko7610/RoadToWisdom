#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UTLootContainer.h"
#include "UTInventoryComponent.h"
#include "UTSaveSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUTLootContainerTest, "UT.Items.ChestPersistence",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUTLootContainerTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	AUTLootContainer* Chest = World->SpawnActor<AUTLootContainer>();
	Chest->StableId = "Test.Chest";
	FUTItemPayload Item;
	Item.Stack.ItemId = "Test.Potion";
	Item.Stack.Count = 8;
	Chest->InitialContents.Add(Item);
	Item = FUTItemPayload();
	Item.bUnique = true;
	Item.Instance.ItemId = "Test.Weapon";
	Item.Instance.State.Durability = 42.f;
	Chest->InitialContents.Add(Item);
	AActor* User = World->SpawnActor<AActor>();
	UUTInventoryComponent* Inventory = NewObject<UUTInventoryComponent>(User);
	User->AddInstanceComponent(Inventory);
	Inventory->RegisterComponent();
	Inventory->MaxSlots = 1;
	Inventory->DefaultMaxStackSize = 5;
	TestTrue("open shared chest", Chest->Open(User));
	TestEqual("partial take", Chest->TakeItem(User, 0, 8), 5);
	TestEqual("unaccepted contents retained", Chest->GetLootState().Contents[0].Stack.Count, 3);
	const FGuid WeaponId = Chest->GetLootState().Contents[1].Instance.InstanceId;
	TestTrue("unique configured id generated once", WeaponId.IsValid());
	Chest->Open(User);
	TestEqual("reopen never rerolls identity", Chest->GetLootState().Contents[1].Instance.InstanceId, WeaponId);
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UUTSaveSubsystem> Save(NewObject<UUTSaveSubsystem>(Instance.Get()));
	FUTWorldStateSnapshot WorldSnapshot;
	FUTWorldStateEntry Entry;
	Entry.StableId = Chest->StableId;
	Entry.State = Chest->CaptureState_Implementation();
	WorldSnapshot.Entries.Add(Entry);
	TestTrue("nested world snapshot serializes", Save->WriteSection("World", 1, WorldSnapshot).IsSuccess());
	FUTWorldStateSnapshot Restored;
	TestTrue("nested world snapshot deserializes", Save->ReadSection("World", 1, Restored).IsSuccess());
	AUTLootContainer* Loaded = World->SpawnActor<AUTLootContainer>();
	Loaded->InitialContents = Chest->InitialContents;
	if (Restored.Entries.Num() == 1) {
		TestTrue("restore persistent chest", Loaded->RestoreState_Implementation(Restored.Entries[0].State));
		Inventory->MaxSlots = 0;
		TestEqual("take exact remaining contents", Loaded->TakeAll(User), 4);
		TestTrue("unique item granted", Inventory->GetInstance(WeaponId, Item.Instance));
		TestEqual("looted state", Loaded->GetLootState().Phase, EUTLootContainerPhase::Looted);
		TestTrue("save looted", Save->WriteSectionAny("Chest", 1, Loaded->CaptureState_Implementation()).IsSuccess());
		FInstancedStruct State = FInstancedStruct::Make<FUTLootContainerState>();
		Save->ReadSectionAny("Chest", 1, State);
		TestTrue("restore looted", Chest->RestoreState_Implementation(State));
		Chest->Open(User);
		TestTrue("looted chest does not regenerate", Chest->GetLootState().Contents.IsEmpty());
		TestEqual("nothing to take after load", Chest->TakeAll(User), 0);
	}
	else {
		AddError(TEXT("Nested world snapshot entry missing"));
	}
	Chest->SetRole(ROLE_SimulatedProxy);
	TestFalse("client cannot restore chest", Chest->RestoreState_Implementation(Entry.State));
	TestFalse("client cannot open", Chest->Open(User));
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
