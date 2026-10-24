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
#include "Gameplay/Missions/RoadToWisdomMissionManager.h"
#include "Gameplay/Save/RoadToWisdomSaveCoordinator.h"
#include "Gameplay/Vehicle/RoadToWisdomVanRadioPanel.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"

namespace
{
	float RouteY(float X)
	{
		const float T = FMath::Clamp(X / 24000.f, 0.f, 1.f);
		const float End = FMath::Clamp((X - 21000.f) / 3000.f, 0.f, 1.f);
		return 1200.f * FMath::Sin(2 * PI * T) + 350.f * FMath::Sin(4 * PI * T) - 900.f * End * End * (3 - 2 * End);
	}

	struct FQRDriveState
	{
		int32 Stage = 0;
		double Started = 0;
		float MaxDeviation = 0;
		bool bMovingExitTested = false;
		FVector SavedLocation = FVector::ZeroVector;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FQRRouteSmoke, "QuietRoad.Session.DriveAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FQRRouteSmoke::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("QuietRoadAutomation"))) {
		AddError(TEXT("Use -QuietRoadAutomation for the isolated slot."));
		return false;
	}
	UGameplayStatics::DeleteGameInSlot(TEXT("QuietRoad_Automation"), 0);
	auto State = MakeShared<FQRDriveState>();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/QuietRoad/Maps/L_QuietRoad_Prototype")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this,State]()
			{
			UWorld* World=GEditor->PlayWorld;
			auto* PC=World ? Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(World,0)) : nullptr;
			if (!TestNotNull(TEXT("Route controller"),PC)) return true;
			auto* Van=PC->GetVanPawn();
			if (!TestNotNull(TEXT("Chaos camper"),Van)) return true;
			if (State->Stage==0)
			{
			auto* Character=Cast<ARoadToWisdomOnFootCharacter>(PC->GetPawn());
			if (!TestNotNull(TEXT("Starting pedestrian"),Character)) return true;
			FVector Door=Van->GetActorTransform().TransformPosition(FVector(120,-190,0));
			Door.Z=100;
			Character->SetActorLocation(Door);
			PC->RequestVehicleToggle();
			if (!TestTrue(TEXT("Enter before actual drive"),PC->IsDriving())) return true;
			State->Started=World->GetTimeSeconds();
			State->Stage=1;
			}
			const FVector Location=Van->GetActorLocation();
			const float Speed=Van->GetVelocity().Size();
			if (World->GetTimeSeconds()-State->Started > 100.f)
			{
			AddError(FString::Printf(TEXT("Route timed out at %s, speed %.1f"),*Location.ToString(),Speed));
			return true;
			}
			if (State->Stage==1)
			{
			State->MaxDeviation=FMath::Max(State->MaxDeviation,FMath::Abs(Location.Y-RouteY(Location.X)));
			const float TargetX=Location.X+550.f;
			const FVector Target(TargetX,RouteY(TargetX),Location.Z);
			const FVector Local=Van->GetActorTransform().InverseTransformPosition(Target);
			Van->DoSteering(FMath::Clamp(FMath::Atan2(Local.Y,Local.X)*1.7f,-0.7f,0.7f));
			if (Speed > 700.f) Van->DoBrake(.1f);
			else Van->DoThrottle(.24f);
			if (!State->bMovingExitTested && Speed > 300.f)
			{
			PC->RequestVehicleToggle();
			TestTrue(TEXT("Exit rejected while moving"),PC->IsDriving());
			State->bMovingExitTested=true;
			}
			if (Location.X < 23200.f) return false;
			State->Stage=2;
			}
			Van->DoThrottle(0.f);
			Van->DoBrake(1.f);
			if (Speed > 25.f) return false;
			Van->DoHandbrakeStart();
			PC->RequestVehicleToggle();
			TestFalse(TEXT("Exit after braking at relay"),PC->IsDriving());
			TestTrue(TEXT("Entire route within road clearance"),State->MaxDeviation < 450.f);
			for (TActorIterator<ARoadToWisdomMissionManager> I(World);I;++I)
			{
			TestEqual(TEXT("Physical arrival advances mission"),I->GetStage(),EQRRelayMissionStage::FindFuse);
			PC->RequestVehicleToggle();
			TestTrue(TEXT("Re-enter at relay"),PC->IsDriving());
			auto* Save=World->GetGameInstance()->GetSubsystem<URoadToWisdomSaveCoordinator>();
			TestTrue(TEXT("Save while parked in driving mode"),Save->SaveSession(*I));
			}
			State->SavedLocation=Van->GetActorLocation();
			for (TActorIterator<ARoadToWisdomVanRadioPanel> I(World);I;++I)
			TestTrue(TEXT("Radio travelled with camper"),FVector::Dist(I->GetActorLocation(),State->SavedLocation)<400.f);
			AddInfo(FString::Printf(TEXT("Chaos route driven to %s; maximum centre deviation %.1f cm."),
				*State->SavedLocation.ToString(),State->MaxDeviation));
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
	ADD_LATENT_AUTOMATION_COMMAND(
		FFunctionLatentCommand([this,State]()
			{
			auto* PC=Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(GEditor->PlayWorld,0));
			if (!TestNotNull(TEXT("Reloaded controller"),PC)) return true;
			TestTrue(TEXT("Driving possession restored"),PC->IsDriving());
			if (auto* Van=PC->GetVanPawn())
			{
			TestTrue(TEXT("Vehicle restored near relay"),FVector::Dist(Van->GetActorLocation(),State->SavedLocation)<100.f);
			TestEqual(TEXT("Restored controller possesses persistent camper"),static_cast<APawn*>(PC->GetPawn()),static_cast<APawn*>(Van));
			}
			UGameplayStatics::DeleteGameInSlot(TEXT("QuietRoad_Automation"),0);
			return true;
			})
	);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
