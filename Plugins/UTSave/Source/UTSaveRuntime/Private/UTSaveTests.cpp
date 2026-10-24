// UT Templates - save automation tests (logic-only: sections and version contract).
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

#include "UTSaveSubsystem.h"
#include "UTSaveTestTypes.h"
#include "UTSaveTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUTSaveSectionsTest, "UT.Save.Sections",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTSaveSectionsTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UGameInstance* FakeInstance = NewObject<UGameInstance>(GetTransientPackage());
	GCRoot.Objects.Add(FakeInstance);
	UUTSaveSubsystem* Save = NewObject<UUTSaveSubsystem>(FakeInstance);
	GCRoot.Objects.Add(Save);

	FUTTestSaveData Data;
	Data.Score = 77;
	Data.Names = {TEXT("alpha"), TEXT("beta")};
	TestTrue("write section", Save->WriteSection("Test.Data", 3, Data).IsSuccess());
	TestTrue("has section", Save->HasSection("Test.Data"));

	// Exact-version read contract.
	FUTTestSaveData Read;
	TestTrue("read same version", Save->ReadSection("Test.Data", 3, Read).IsSuccess());
	TestEqual("score round trip", Read.Score, 77);
	TestEqual("names round trip", Read.Names.Num(), 2);

	// Wrong version rejected, not reinterpreted.
	FUTTestSaveData NotRead;
	TestFalse("wrong version rejected", Save->ReadSection("Test.Data", 2, NotRead).IsSuccess());
	TestFalse("newer version rejected", Save->ReadSection("Test.Data", 4, NotRead).IsSuccess());

	// Missing section.
	TestFalse("missing section", Save->ReadSection("Test.Missing", 1, NotRead).IsSuccess());

	// Raw byte escape hatch for migrations.
	int32 Version = 0;
	TArray<uint8> Bytes;
	TestTrue("raw read", Save->ReadSectionBytes("Test.Data", Version, Bytes).IsSuccess());
	TestEqual("raw version", Version, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUTSaveSlotTest, "UT.Save.Slots",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUTSaveSlotTest::RunTest(const FString& Parameters)
{
	UTTestGC::FGCRoot GCRoot;
	UGameInstance* FakeInstance = NewObject<UGameInstance>(GetTransientPackage());
	UUTSaveSubsystem* Save = NewObject<UUTSaveSubsystem>(FakeInstance);
	const FName SlotName = "UT_AutomationTemp";

	Save->DeleteSlot(SlotName);
	TestFalse("slot absent", Save->SlotExists(SlotName));

	FUTTestSaveData Data;
	Data.Score = 5;
	Save->WriteSection("Test.Data", 1, Data);
	TestTrue("save to slot", Save->SaveToSlot(SlotName).IsSuccess());
	TestTrue("slot exists", Save->SlotExists(SlotName));

	// Transactional load: valid slot replaces Current.
	Save->RemoveSection("Test.Data");
	TestFalse("section cleared", Save->HasSection("Test.Data"));
	TestTrue("load from slot", Save->LoadFromSlot(SlotName).IsSuccess());
	FUTTestSaveData Read;
	TestTrue("section restored", Save->ReadSection("Test.Data", 1, Read).IsSuccess());
	TestEqual("payload restored", Read.Score, 5);

	// Transactional contract: a failed load leaves the previously loaded state intact.
	const FUTResult LoadMissing = Save->LoadFromSlot("UT_AutomationMissing");
	TestFalse("missing slot fails", LoadMissing.IsSuccess());
	TestTrue("current untouched after failed load", Save->IsSlotLoaded());
	TestTrue("in-memory section still readable", Save->HasSection("Test.Data"));

	Save->DeleteSlot(SlotName);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
