#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UTWorldItem.h"
#include "UTInventoryComponent.h"
#include "UTItemDropComponent.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUTWorldItemTest, "UT.Items.WorldPickupDrop",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUTWorldItemTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	auto Collector = [World]() {
		AActor* Actor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Actor);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		UUTInventoryComponent* Inventory = NewObject<UUTInventoryComponent>(Actor);
		Actor->AddInstanceComponent(Inventory);
		Inventory->RegisterComponent();
		return Actor;
	};
	AActor* A = Collector();
	AActor* B = Collector();
	UUTInventoryComponent* Inventory = A->FindComponentByClass<UUTInventoryComponent>();
	Inventory->MaxSlots = 1;
	Inventory->DefaultMaxStackSize = 5;
	Inventory->AddItem("Test.Potion", 4);
	AUTWorldItem* Pickup = World->SpawnActor<AUTWorldItem>();
	FUTItemPayload Payload;
	Payload.Stack.ItemId = "Test.Potion";
	Payload.Stack.Count = 5;
	TestTrue("initialize pickup", Pickup->InitializeItem(Payload));
	TestEqual("partial acceptance", Pickup->TryPickup(A), 1);
	TestEqual("world retains remainder", Pickup->GetItem().GetCount(), 4);
	TestEqual("full inventory accepts nothing", Pickup->TryPickup(A), 0);
	B->SetActorLocation(FVector(1000, 0, 0));
	TestEqual("distance validated", Pickup->TryPickup(B), 0);
	B->SetActorLocation(FVector::ZeroVector);
	TestEqual("other collector accepts remainder", Pickup->TryPickup(B), 4);
	TestEqual("destroyed pickup cannot be taken twice", Pickup->TryPickup(A), 0);
	UUTItemDropComponent* Drop = NewObject<UUTItemDropComponent>(A);
	A->AddInstanceComponent(Drop);
	Drop->RegisterComponent();
	Drop->WorldItemClass = nullptr;
	TestNull("failed preparation", Drop->DropStack("Test.Potion", 2));
	TestEqual("failed spawn leaves inventory unchanged", Inventory->GetItemCount("Test.Potion"), 5);
	Drop->WorldItemClass = AUTWorldItem::StaticClass();
	TestNull("cannot invent drop quantity", Drop->DropStack("Test.Potion", 6));
	AUTWorldItem* Dropped = Drop->DropStack("Test.Potion", 2);
	TestNotNull("drop succeeds", Dropped);
	TestEqual("source debited", Inventory->GetItemCount("Test.Potion"), 3);
	if (Dropped) {
		TestEqual("world receives exact quantity", Dropped->GetItem().GetCount(), 2);
		TestEqual("can pick drop back up", Dropped->TryPickup(A), 2);
	}
	Inventory->ClearInventory();
	FUTItemInstance Unique = FUTItemInstance::Create("Test.Weapon");
	Unique.State.Ammo = 9;
	Inventory->AddInstance(Unique);
	Dropped = Drop->DropInstance(Unique.InstanceId);
	TestNotNull("unique drop", Dropped);
	if (Dropped) {
		TestEqual("unique pickup", Dropped->TryPickup(B), 1);
		FUTItemInstance Copy;
		TestTrue("exact identity transferred", B->FindComponentByClass<UUTInventoryComponent>()->GetInstance(Unique.InstanceId, Copy));
		TestEqual("state transferred", Copy.State.Ammo, 9);
	}
	A->SetRole(ROLE_SimulatedProxy);
	TestNull("client cannot spawn a drop", Drop->DropStack("Test.Potion", 1));
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
