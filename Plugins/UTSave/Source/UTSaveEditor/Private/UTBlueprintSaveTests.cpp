#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "UTSaveable.h"
#include "UTWorldStateSubsystem.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTBlueprintSaveTest, "UT.Save.BlueprintOnlyInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTBlueprintSaveTest::RunTest(const FString&)
{
	TStrongObjectPtr<UBlueprint> Blueprint(
		FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), "SaveProbe"),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));
	if (!TestTrue(
			"Blueprint interface added", FBlueprintEditorUtils::ImplementNewInterface(
											 Blueprint.Get(), UUTSaveable::StaticClass()->GetClassPathName()))) {
		return false;
	}
	int32 ConfiguredReturns = 0;
	for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces) {
		for (UEdGraph* Graph : Interface.Graphs) {
			for (UEdGraphNode* Node : Graph->Nodes) {
				if (auto* Result = Cast<UK2Node_FunctionResult>(Node)) {
					if (UEdGraphPin* Pin = Result->FindPin(TEXT("ReturnValue"))) {
						if (Graph->GetFName() == "GetSaveStableId") {
							Graph->GetSchema()->TrySetDefaultValue(*Pin, TEXT("Blueprint.Probe"));
							++ConfiguredReturns;
						}
						if (Graph->GetFName() == "RestoreState") {
							Graph->GetSchema()->TrySetDefaultValue(*Pin, TEXT("true"));
							++ConfiguredReturns;
						}
					}
				}
			}
		}
	}
	TestEqual("configured return pins", ConfiguredReturns, 2);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint.Get());
	FKismetEditorUtilities::CompileBlueprint(Blueprint.Get());
	if (!TestTrue("Blueprint compiled", Blueprint->Status != BS_Error)) {
		return false;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	AActor* Actor = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
	TestTrue("script interface recognized", Actor->Implements<UUTSaveable>());
	TestNull("script implementation has no C++ interface pointer", Cast<IUTSaveable>(Actor));
	TestEqual("Blueprint stable id dispatch", IUTSaveable::Execute_GetSaveStableId(Actor), FName("Blueprint.Probe"));
	auto* State = World->GetSubsystem<UUTWorldStateSubsystem>();
	FUTWorldStateSnapshot Snapshot;
	TestTrue("Blueprint capture succeeds", State->CaptureWorldState(Snapshot).IsSuccess());
	TestEqual("Blueprint actor captured", Snapshot.Entries.Num(), 1);
	FUTResult Result;
	TestEqual("Blueprint restore dispatched", State->ApplyWorldState(Snapshot, Result), 1);
	TestTrue("Blueprint restore succeeds", Result.IsSuccess());
	World->DestroyWorld(false);
	return true;
}
#endif
