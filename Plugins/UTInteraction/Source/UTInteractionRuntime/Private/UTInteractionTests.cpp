#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UTInteractorComponent.h"
#include "UTInteractionTestActor.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	// Must exceed AUTInteractionTestActor::HoldDurationSeconds so the pending server
	// timer becomes eligible after one tick.
	constexpr float TimerTickPastHoldSeconds = 2.1f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUTInteractionAuthorityTest, "UT.Interaction.AuthoritativeHold",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUTInteractionAuthorityTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	AActor* Owner = World->SpawnActor<AActor>();
	AUTInteractionTestActor* Target = World->SpawnActor<AUTInteractionTestActor>();
	UUTInteractorComponent* Interactor = NewObject<UUTInteractorComponent>(Owner);
	Owner->AddInstanceComponent(Interactor);
	Interactor->RegisterComponent();
	Interactor->ServerInteract_Implementation(Target, "Hold");
	TestEqual("tap RPC cannot bypass hold", Target->Executions, 0);
	FUTInteractionOption Option;
	Option.OptionId = "Hold";
	Option.HoldDuration = AUTInteractionTestActor::HoldDurationSeconds;
	TestTrue("owner authoritative", Owner->HasAuthority());
	TestTrue("target interface", Target->Implements<UUTInteractable>());
	TestTrue("component same world", Interactor->GetWorld() == Target->GetWorld());
	FUTInteractionOption Offered;
	TestTrue("hold offered", Interactor->FindOfferedOption(Target, "Hold", Offered));
	FUTInteractionInfo Info;
	Info.Interactor = Owner;
	Info.OptionId = "Hold";
	TestTrue("target allows interaction", IUTInteractable::Execute_CanInteract(Target, Info));
	TestTrue("server request valid", Interactor->ServerValidateRequest(Target, "Hold"));
	Interactor->StartHold(Target, Option);
	Interactor->TickComponent(3.f, LEVELTICK_All, nullptr);
	TestEqual("UI elapsed cannot execute gameplay", Target->Executions, 0);
	TestTrue("UI elapsed does not cancel server timer", World->GetTimerManager().TimerExists(Interactor->ServerHoldTimer));
	World->GetTimerManager().Tick(TimerTickPastHoldSeconds);
	++GFrameCounter; // Advance fixture frame so the pending timer becomes eligible.
	World->GetTimerManager().Tick(TimerTickPastHoldSeconds);
	TestEqual("server timer executes once", Target->Executions, 1);
	TestFalse("hold completed", Interactor->IsHoldingInteract());
	Interactor->StartHold(Target, Option);
	Interactor->CancelHoldInteract();
	World->GetTimerManager().Tick(3.f);
	TestEqual("cancel prevents execution", Target->Executions, 1);
	TestTrue("explicit AI intent starts authoritative hold", Interactor->InteractWithTarget(Target, "Hold"));
	TestFalse("second explicit intent rejected while holding", Interactor->InteractWithTarget(Target, "Hold"));
	TestFalse("unknown target cannot cancel activity", Interactor->CancelInteractionWithTarget(Owner));
	TestTrue("adapter can cancel only its own target", Interactor->CancelInteractionWithTarget(Target));
	TestFalse("cancelled hold no longer pending", World->GetTimerManager().TimerExists(Interactor->ServerHoldTimer));
	Owner->SetRole(ROLE_SimulatedProxy);
	TestFalse("explicit API cannot mutate on client", Interactor->InteractWithTarget(Target, "Hold"));
	Owner->SetRole(ROLE_Authority);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
