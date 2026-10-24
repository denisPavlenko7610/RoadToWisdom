#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UTLootTable.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUTLootTest, "UT.Items.LootTable",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUTLootTest::RunTest(const FString&)
{
	TStrongObjectPtr<UUTLootTable> Table(NewObject<UUTLootTable>());
	TestTrue("empty table", Table->RollWithSeed(42, 5).Items.IsEmpty());
	UUTItemDefinition* Potion = NewObject<UUTItemDefinition>(Table.Get());
	Potion->ItemId = "Test.Potion";
	UUTItemDefinition* Wood = NewObject<UUTItemDefinition>(Table.Get());
	Wood->ItemId = "Test.Wood";
	FUTLootEntry Entry;
	Entry.ItemDefinition = Potion;
	Entry.MinCount = 2;
	Entry.MaxCount = 5;
	Table->Entries.Add(Entry);
	Entry.ItemDefinition = Wood;
	Entry.Weight = 3.f;
	Table->Entries.Add(Entry);
	Entry.ItemDefinition = nullptr;
	Table->Entries.Add(Entry);
	Entry.ItemDefinition = Potion;
	Entry.Weight = 0.f;
	Table->Entries.Add(Entry);
	Entry.Weight = -1.f;
	Table->Entries.Add(Entry);
	Entry.Weight = 1.f;
	Entry.MinCount = 8;
	Entry.MaxCount = 2;
	Table->Entries.Add(Entry);
#if WITH_EDITOR
	FDataValidationContext Validation;
	TestEqual("editor diagnoses invalid authored entries", Table->IsDataValid(Validation), EDataValidationResult::Invalid);
#endif
	const FUTLootResult A = Table->RollWithSeed(1234, 1000);
	const FUTLootResult B = Table->RollWithSeed(1234, 1000);
	TestEqual("one valid result per draw", A.Items.Num(), 1000);
	TestEqual("same result length", A.Items.Num(), B.Items.Num());
	int32 WoodCount = 0;
	for (int32 Index = 0; Index < A.Items.Num(); ++Index) {
		TestEqual("deterministic definition", A.Items[Index].ItemId, B.Items[Index].ItemId);
		TestEqual("deterministic count", A.Items[Index].Count, B.Items[Index].Count);
		TestTrue("count in range", A.Items[Index].Count >= 2 && A.Items[Index].Count <= 5);
		WoodCount += A.Items[Index].ItemId == Wood->ItemId ? 1 : 0;
	}
	TestTrue("weight 3 dominates weight 1", WoodCount > 650 && WoodCount < 850);
	Entry.bGuaranteed = true;
	Entry.Weight = 0.f;
	Entry.MinCount = Entry.MaxCount = 7;
	Table->Entries.Add(Entry);
	const FUTLootResult Guaranteed = Table->RollWithSeed(1, 4);
	TestEqual("guaranteed once plus draws", Guaranteed.Items.Num(), 5);
	TestEqual("guaranteed fixed count", Guaranteed.Items[0].Count, 7);
	TestEqual("zero draws still awards guaranteed", Table->RollWithSeed(1, 0).Items.Num(), 1);
	Table->Entries.RemoveAt(0, 6);
	TestEqual("guaranteed only table", Table->RollWithSeed(1, 5).Items.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
