#include "UTLootTable.h"

namespace
{
	// Upper bound on weighted draws per Roll call, matching the authored ClampMax.
	constexpr int32 MaxDrawCount = 1024;
}

bool FUTLootEntry::IsValid() const
{
	return ItemDefinition && !ItemDefinition->ItemId.IsNone() && MinCount > 0 && MaxCount >= MinCount && (bGuaranteed || (FMath::IsFinite(Weight) && Weight > 0.f));
}
FUTLootResult UUTLootTable::RollWithSeed(int32 Seed, int32 Rolls) const
{
	FRandomStream Stream(Seed);
	return Roll(Stream, Rolls);
}
FUTLootResult UUTLootTable::Roll(FRandomStream& Stream, int32 Rolls) const
{
	FUTLootResult Result;
	auto Award = [&Result, &Stream](const FUTLootEntry& Entry) {
		FUTLootItem& Item = Result.Items.AddDefaulted_GetRef();
		Item.ItemId = Entry.ItemDefinition->ItemId;
		// Avoid overflowing RandRange's inclusive int32 span for a very large count.
		const int64 Span = int64(Entry.MaxCount) - Entry.MinCount + 1;
		Item.Count = int32(int64(Entry.MinCount) + FMath::Min<int64>(int64(Stream.GetFraction() * double(Span)), Span - 1));
	};
	double TotalWeight = 0.0;
	for (const FUTLootEntry& Entry : Entries) {
		if (!Entry.IsValid()) {
			continue;
		}
		if (Entry.bGuaranteed) {
			Award(Entry);
		}
		else {
			TotalWeight += Entry.Weight;
		}
	}
	if (TotalWeight <= 0.0) {
		return Result;
	}
	for (int32 Index = 0; Index < FMath::Clamp(Rolls, 0, MaxDrawCount); ++Index) {
		double Choice = Stream.GetFraction() * TotalWeight;
		for (const FUTLootEntry& Entry : Entries) {
			if (Entry.bGuaranteed || !Entry.IsValid()) {
				continue;
			}
			Choice -= Entry.Weight;
			if (Choice < 0.0) {
				Award(Entry);
				break;
			}
		}
	}
	return Result;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"
EDataValidationResult UUTLootTable::IsDataValid(FDataValidationContext& Context) const
{
	bool bValid = true;
	for (int32 Index = 0; Index < Entries.Num(); ++Index) {
		if (!Entries[Index].IsValid()) {
			Context.AddError(FText::FromString(FString::Printf(TEXT("Loot entry %d needs an item definition/id, positive count range and positive finite weight (unless guaranteed)."), Index)));
			bValid = false;
		}
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
