#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/Player/RoadToWisdomExplorerController.h"
#include "Gameplay/Character/RoadToWisdomOnFootCharacter.h"
#include "Gameplay/Vehicle/RoadToWisdomVanRadioPanel.h"
#include "Gameplay/Save/RoadToWisdomSaveCoordinator.h"
#include "UTInteractorComponent.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQRRadioSmoke, "QuietRoad.Session.RadioHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FQRRadioSmoke::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("QuietRoadAutomation"))) {
		AddError(TEXT("Use -QuietRoadAutomation for an isolated slot."));
		return false;
	}
	UGameplayStatics::DeleteGameInSlot(TEXT("QuietRoad_Automation"), 0);
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this]()
			{
			UWorld* World=GEditor->PlayWorld;
			if (!TestNotNull(TEXT("PIE world"),World)) return true;
			auto* PC=Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(World,0));
			auto* Character=PC ? Cast<ARoadToWisdomOnFootCharacter>(PC->GetPawn()) : nullptr;
			if (!TestNotNull(TEXT("Explorer"),Character)) return true;
			ARoadToWisdomVanRadioPanel* Radio=nullptr;
			for (TActorIterator<ARoadToWisdomVanRadioPanel> I(World);I;++I) Radio=*I;
			if (!TestNotNull(TEXT("Radio"),Radio)) return true;
			const FVector Side=PC->GetVanPawn()->GetActorRightVector();
			FVector Position=Radio->GetActorLocation()-Side*200.f;
			Position.Z=100.f;
			Character->SetActorLocation(Position);
			PC->SetControlRotation(Side.Rotation());
			TestEqual(TEXT("Radio attached to camper"),Radio->GetAttachParentActor(),static_cast<AActor*>(PC->GetVanPawn()));
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this]()
			{
			auto* Character=Cast<ARoadToWisdomOnFootCharacter>(UGameplayStatics::GetPlayerPawn(GEditor->PlayWorld,0));
			if (!Character) return true;
			Character->GetInteractor()->UpdateFocus();
			TestNotNull(TEXT("Camera trace focuses radio"),Cast<ARoadToWisdomVanRadioPanel>(Character->GetInteractor()->
				GetFocusedActor()));
			Character->HandleInteract();
			TestTrue(TEXT("Press starts Toolkit hold"),Character->GetInteractor()->IsHoldingInteract());
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.1f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this]()
			{
			auto* Character=Cast<ARoadToWisdomOnFootCharacter>(UGameplayStatics::GetPlayerPawn(GEditor->PlayWorld,0));
			if (!Character) return true;
			Character->ReleaseInteract();
			TestFalse(TEXT("Tap cancels hold"),Character->GetInteractor()->IsHoldingInteract());
			TestFalse(TEXT("Tap does not write save"),UGameplayStatics::DoesSaveGameExist(TEXT("QuietRoad_Automation"),0));
			Character->HandleInteract();
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this]()
			{
			auto* Character=Cast<ARoadToWisdomOnFootCharacter>(UGameplayStatics::GetPlayerPawn(GEditor->PlayWorld,0));
			if (!Character) return true;
			Character->ReleaseInteract();
			TestFalse(TEXT("Early hold release does not write save"),UGameplayStatics::DoesSaveGameExist(TEXT(
				"QuietRoad_Automation"),0));
			Character->HandleInteract();
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this]()
			{
			auto* Character=Cast<ARoadToWisdomOnFootCharacter>(UGameplayStatics::GetPlayerPawn(GEditor->PlayWorld,0));
			if (!Character) return true;
			TestTrue(TEXT("Full hold writes real isolated slot"),UGameplayStatics::DoesSaveGameExist(TEXT("QuietRoad_Automation"),
				0));
			Character->ReleaseInteract();
			UGameplayStatics::DeleteGameInSlot(TEXT("QuietRoad_Automation"),0);
			Character->HandleInteract();
			Character->CancelInteract();
			TestFalse(TEXT("Cancellation clears Toolkit hold"),Character->GetInteractor()->IsHoldingInteract());
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this]()
			{
			TestFalse(TEXT("Cancelled hold never saves later"),UGameplayStatics::DoesSaveGameExist(TEXT("QuietRoad_Automation"),0)
			);
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
