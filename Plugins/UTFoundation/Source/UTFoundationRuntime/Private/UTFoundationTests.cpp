// UT Templates - foundation automation tests (logic-only).
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

#include "UTConditions.h"
#include "UTMessageBusSubsystem.h"
#include "UTMessageTypes.h"
#include "UTFoundationTestTypes.h"
#include "UTResult.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"

// FUTResult ------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTResultTest, "UT.Foundation.Result",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTResultTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	const FUTResult Ok = FUTResult::Success();
	TestTrue("success flag", Ok.IsSuccess());
	TestEqual("success code", Ok.Code, FName("Success"));

	const FUTResult Fail = FUTResult::Failure(TEXT("SlotFull"), FText::FromString(TEXT("no room")));
	TestFalse("failure flag", Fail.IsSuccess());
	TestEqual("failure code", Fail.Code, FName("SlotFull"));
	return true;
}

// Conditions -----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTConditionsTest, "UT.Foundation.Conditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTConditionsTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	// Not of a passing (empty) And always fails; Not without a child passes.
	UUTConditionAnd* EmptyPass = NewObject<UUTConditionAnd>();
	TestTrue("empty And passes", EmptyPass->Evaluate(FUTConditionContext()));

	UUTConditionNot* NotOfPassing = NewObject<UUTConditionNot>();
	NotOfPassing->Condition = EmptyPass;
	TestFalse("Not of passing condition fails", NotOfPassing->Evaluate(FUTConditionContext()));

	UUTConditionAnd* And = NewObject<UUTConditionAnd>();
	And->Conditions.Add(NotOfPassing);
	TestFalse("And with failing child fails", And->Evaluate(FUTConditionContext()));

	UUTConditionOr* Or = NewObject<UUTConditionOr>();
	Or->Conditions.Add(NotOfPassing);
	TestFalse("Or with only failing children fails", Or->Evaluate(FUTConditionContext()));

	UUTConditionAnd* AnotherPass = NewObject<UUTConditionAnd>();
	UUTConditionNot* NotOfFailing = NewObject<UUTConditionNot>();
	NotOfFailing->Condition = And; // And currently fails
	Or->Conditions.Add(NotOfFailing);
	Or->Conditions.Add(AnotherPass);
	TestTrue("Or with a passing child passes", Or->Evaluate(FUTConditionContext()));

	// Repeated evaluation is stable (no hidden state, no random roll).
	const bool FirstResult = And->Evaluate(FUTConditionContext());
	for (int32 i = 0; i < 10; ++i) {
		TestTrue("re-evaluate stable", And->Evaluate(FUTConditionContext()) == FirstResult);
	}
	return true;
}

// Message bus ----------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUTMessageBusTest, "UT.Foundation.MessageBus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTMessageBusTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	// The bus logic does not touch its GameInstance, but the class requires one as its
	// outer; a bare GameInstance under the transient package is enough for the logic test.
	UGameInstance* FakeInstance = NewObject<UGameInstance>(GetTransientPackage());
	GCRoot.Objects.Add(FakeInstance);
	UUTMessageBusSubsystem* Bus = NewObject<UUTMessageBusSubsystem>(FakeInstance);
	GCRoot.Objects.Add(Bus);
	TestNotNull("bus exists", Bus);
	if (!Bus) {
		return false;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag("UT.Message.ActorDied");

	// Typed listener receives the payload.
	int32 TypedValue = 0;
	const FDelegateHandle TypedHandle = Bus->ListenTyped<FUTTestPingMessage>(
		Tag,
		[&TypedValue](const FUTTestPingMessage& Message) {
			TypedValue = Message.Value;
		});

	// Untyped listener counts broadcasts.
	int32 UntypedCount = 0;
	const FDelegateHandle UntypedHandle = Bus->Listen(
		Tag, UUTMessageBusSubsystem::FUTMessageCallback::CreateLambda(
				 [&UntypedCount](const FUTMessage&) {
					 ++UntypedCount;
				 }));

	FUTTestPingMessage Ping;
	Ping.Value = 42;
	Bus->BroadcastMessage(Tag, Ping);
	TestEqual("typed payload delivered", TypedValue, 42);
	TestEqual("untyped listener notified", UntypedCount, 1);

	// Type safety: a different payload struct is skipped, not reinterpreted.
	Bus->BroadcastMessage(Tag, FUTResult::Success());
	TestEqual("mismatched payload skipped", TypedValue, 42);
	TestEqual("untyped still notified", UntypedCount, 2);

	// Removal.
	Bus->RemoveListener(TypedHandle);
	Bus->RemoveListener(UntypedHandle);
	Bus->BroadcastMessage(Tag, Ping);
	TestEqual("removed listeners stay silent", UntypedCount, 2);

	// Removing the final listener erases the tag row. Dispatch must not read that row
	// again when moving from native to dynamic callbacks.
	int32 ReentrantCalls = 0;
	Bus->ListenTyped<FUTTestPingMessage>(
		Tag, [Bus, Tag, &ReentrantCalls](const FUTTestPingMessage&) {
			++ReentrantCalls;
			Bus->ClearTagListeners(Tag);
		});
	Bus->BroadcastMessage(Tag, Ping);
	Bus->BroadcastMessage(Tag, Ping);
	TestEqual("clear during dispatch runs once", ReentrantCalls, 1);
	TestFalse("cleared tag is pruned", Bus->HasListeners(Tag));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
