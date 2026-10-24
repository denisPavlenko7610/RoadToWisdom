#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/Player/RoadToWisdomExplorerController.h"
#include "Gameplay/Character/RoadToWisdomOnFootCharacter.h"
#include "Gameplay/Save/RoadToWisdomSaveCoordinator.h"
#include "Gameplay/Items/RoadToWisdomPlacedPickup.h"
#include "Gameplay/Missions/RoadToWisdomMissionManager.h"
#include "Gameplay/Missions/RoadToWisdomFuseBox.h"
#include "UTInventoryComponent.h"
#include "UTSaveSubsystem.h"
#include "UTLever.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQRSessionSmoke, "QuietRoad.Session.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FQRSessionSmoke::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("QuietRoadAutomation"))) {
		AddError(TEXT("Run with -QuietRoadAutomation to isolate the save slot."));
		return false;
	}
	UGameplayStatics::DeleteGameInSlot(TEXT("QuietRoad_Automation"), 0);
	for (int32 Phase = 0; Phase < 5; ++Phase) {
		ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")));
		ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
		ADD_LATENT_AUTOMATION_COMMAND(
			FFunctionLatentCommand([this, Phase]()
				{
				UWorld* World = GEditor->PlayWorld;
				if (!TestNotNull(TEXT("PIE world"), World)) return true;
				auto* PC = Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(World, 0));
				if (!TestNotNull(TEXT("Explorer controller"), PC)) return true;
				auto* Character = Cast<ARoadToWisdomOnFootCharacter>(PC->GetPawn());
				auto* Coordinator = World->GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>();
				auto* Save = World->GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
				ARoadToWisdomMissionManager* Mission = nullptr;
				ARoadToWisdomFuseBox* Box = nullptr;
				AUTLever* Lever = nullptr;
				for (TActorIterator<ARoadToWisdomMissionManager> I(World); I; ++I) Mission = *I;
				for (TActorIterator<ARoadToWisdomFuseBox> I(World); I; ++I) Box = *I;
				for (TActorIterator<AUTLever> I(World); I; ++I) Lever = *I;
				if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Mission"), Mission)
					|| !TestNotNull(TEXT("Box"), Box) || !TestNotNull(TEXT("Lever"), Lever)) return true;
				auto* Inventory = Character->GetInventory();
				TArray<ARoadToWisdomPlacedPickup*> Pickups;
				ARoadToWisdomPlacedPickup::GetAllPlacedPickups(World, Pickups);
				FUTInteractionInfo Info;
				Info.Interactor = Character;
				if (Phase <= 1)
				{
				TestEqual(TEXT("Uncollected fuse survives reload"), Pickups.Num(), 1);
				TestEqual(TEXT("No fuse in inventory"), Inventory->GetItemCount(TEXT("Item.Fuse")), 0);
				if (Pickups.Num() != 1) return true;
				auto* Pickup = Pickups[0];
				if (Phase == 0)
				{
				TestTrue(TEXT("Save before pickup"), Coordinator->SaveSession(Mission));
				Character->SetActorLocation(Pickup->GetActorLocation() + FVector(1000, 0, 0));
				Pickup->Interact_Implementation(Info);
				TestTrue(TEXT("Out-of-range pickup retained"), IsValid(Pickup));
				Character->SetActorLocation(Pickup->GetActorLocation() + FVector(0, -100, 0));
				Inventory->MaxSlots = 1;
				Inventory->AddItem(TEXT("Item.TestFiller"), 1);
				Pickup->Interact_Implementation(Info);
				TestEqual(TEXT("Full inventory rejects fuse"), Inventory->GetItemCount(TEXT("Item.Fuse")), 0);
				TestTrue(TEXT("Rejected pickup remains"), IsValid(Pickup));
				TestTrue(TEXT("Save after failed pickup"), Coordinator->SaveSession(Mission));
				FQRPickupsSection Section;
				TestTrue(TEXT("Read pickup section"), Save->ReadSection(TEXT("Pickups"), 2, Section).IsSuccess());
				TestEqual(TEXT("Failed transfer never collected"), Section.CollectedIds.Num(), 0);
				Inventory->ClearInventory();
				// Exercise the real Toolkit partial-transfer result and the project's
				// remainder section, including restoration after a second transfer.
				Inventory->DefaultMaxStackSize = 2;
				auto* Parts = World->SpawnActorDeferred<ARoadToWisdomPlacedPickup>(
					ARoadToWisdomPlacedPickup::StaticClass(), FTransform(Pickup->GetActorLocation() + FVector(0, -150, 0)));
				Parts->SaveStableId = TEXT("QuietRoad.Test.Parts");
				Parts->InitialItem.Stack.ItemId = TEXT("Item.TestParts");
				Parts->InitialItem.Stack.Count = 5;
				Parts->FinishSpawning(Parts->GetActorTransform());
				Parts->Interact_Implementation(Info);
				TestEqual(TEXT("Partial accepts capacity only"), Inventory->GetItemCount(TEXT("Item.TestParts")), 2);
				TestEqual(TEXT("Partial leaves remainder"), Parts->GetItem().GetCount(), 3);
				TestTrue(TEXT("Save partial stack"), Coordinator->SaveSession(Mission));
				Inventory->ClearInventory();
				Parts->Interact_Implementation(Info);
				TestEqual(TEXT("Second transfer changes live remainder"), Parts->GetItem().GetCount(), 1);
				TestTrue(TEXT("Restore partial-stack section"), Coordinator->TryRestoreSession(PC));
				PC->ApplyPendingSessionRestore();
				TArray<ARoadToWisdomPlacedPickup*> Restored;
				ARoadToWisdomPlacedPickup::GetAllPlacedPickups(World, Restored);
				for (auto* Item : Restored)
				if (Item->SaveStableId == TEXT("QuietRoad.Test.Parts"))
				{
				TestEqual(TEXT("Saved remainder restored exactly"), Item->GetItem().GetCount(), 3);
				Item->Destroy();
				}
				TestEqual(TEXT("Partial inventory restored"), Inventory->GetItemCount(TEXT("Item.TestParts")), 2);
				Inventory->ClearInventory();
				Inventory->MaxSlots = 8;
				// Possession round trip uses the actual controller and Enhanced Input.
				const FVector Door = PC->VanPawn->GetActorTransform().TransformPosition(PC->EnterProbeOffset);
				Character->SetActorLocation(Door);
				PC->RequestVehicleToggle();
				TestTrue(TEXT("Enter camper"), PC->IsDriving());
				PC->RequestVehicleToggle();
				TestFalse(TEXT("Exit stationary camper"), PC->IsDriving());
				auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
				if (TestNotNull(TEXT("Enhanced Input subsystem"), Input))
				for (const auto& Context : PC->OnFootMappingContexts)
				TestTrue(TEXT("On-foot context restored"), Input->HasMappingContext(Context.Get()));
				TestTrue(TEXT("Save clean pre-pickup state"), Coordinator->SaveSession(Mission));
				}
				else
				{
				Character->SetActorLocation(Pickup->GetActorLocation() + FVector(0, -100, 0));
				Mission->ApplyState(EQRRelayMissionStage::FindFuse, false);
				Pickup->Interact_Implementation(Info);
				TestEqual(TEXT("Successful pickup transfers exactly one fuse"), Inventory->GetItemCount(TEXT("Item.Fuse")), 1);
				TestTrue(TEXT("Save after successful pickup"), Coordinator->SaveSession(Mission));
				}
				}
				else if (Phase == 2)
				{
				TestEqual(TEXT("Collected fuse absent after restart"), Pickups.Num(), 0);
				TestEqual(TEXT("Inventory fuse restored once"), Inventory->GetItemCount(TEXT("Item.Fuse")), 1);
				TestFalse(TEXT("Socket initially empty"), Box->bFuseInstalled);
				Box->Interact_Implementation(Info);
				TestTrue(TEXT("Fuse installed"), Box->bFuseInstalled);
				TestEqual(TEXT("Fuse consumed once"), Inventory->GetItemCount(TEXT("Item.Fuse")), 0);
				TestEqual(TEXT("Mission advances"), Mission->GetStage(), EQRRelayMissionStage::PullLever);
				TestTrue(TEXT("Save installed state"), Coordinator->SaveSession(Mission));
				}
				else if (Phase == 3)
				{
				TestEqual(TEXT("No pickup after installed-state reload"), Pickups.Num(), 0);
				TestTrue(TEXT("Socket restored"), Box->bFuseInstalled);
				TestEqual(TEXT("No inventory duplicate"), Inventory->GetItemCount(TEXT("Item.Fuse")), 0);
				TestEqual(TEXT("Stage restored"), Mission->GetStage(), EQRRelayMissionStage::PullLever);
				Lever->SetOn(true);
				TestEqual(TEXT("First mission completed"), Mission->GetStage(), EQRRelayMissionStage::Completed);
				TestTrue(TEXT("Save completion"), Coordinator->SaveSession(Mission));
				}
				else
				{
				TestEqual(TEXT("Completion survives restart"), Mission->GetStage(), EQRRelayMissionStage::Completed);
				TestTrue(TEXT("Lever restored"), Lever->bIsOn);
				TestTrue(TEXT("Fuse still installed"), Box->bFuseInstalled);
				TestEqual(TEXT("No respawn on completion"), Pickups.Num(), 0);
				Save->RemoveSection(TEXT("Inventory"));
				Save->SaveToSlot(URoadToWisdomSaveCoordinator::GetSessionSlotName());
				TestFalse(TEXT("Missing mandatory section rejects whole restore"), Coordinator->TryRestoreSession(PC));
				TestTrue(TEXT("Rejected restore does not change socket"), Box->bFuseInstalled);
				UGameplayStatics::DeleteGameInSlot(TEXT("QuietRoad_Automation"), 0);
				}
				return true;
				})
		);
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	}
	return true;
}
#endif
