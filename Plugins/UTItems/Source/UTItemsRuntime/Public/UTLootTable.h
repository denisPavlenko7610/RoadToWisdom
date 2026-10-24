#pragma once
#include "Engine/DataAsset.h"
#include "UTItemDefinition.h"
#include "UTLootTable.generated.h"

/** One definition-backed candidate. Guaranteed entries are awarded once per Roll call. */
USTRUCT(BlueprintType)
struct UTITEMSRUNTIME_API FUTLootEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Loot")
	TObjectPtr<UUTItemDefinition> ItemDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Loot", meta = (ClampMin = "0", EditCondition = "!bGuaranteed"))
	float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|Loot")
	bool bGuaranteed = false;
	bool IsValid() const;
};

/** One awarded roll. Repeated definitions remain separate; composition decides how to combine them. */
USTRUCT(BlueprintType)
struct UTITEMSRUNTIME_API FUTLootItem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "UT|Loot")
	FName ItemId;
	UPROPERTY(BlueprintReadOnly, Category = "UT|Loot")
	int32 Count = 0;
};
USTRUCT(BlueprintType)
struct UTITEMSRUNTIME_API FUTLootResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "UT|Loot")
	TArray<FUTLootItem> Items;
};

/** Pure weighted selection with replacement, no world/inventory/UI side effects.
 * Invalid entries are skipped. No nesting: compose tables explicitly in game code.
 * Gameplay callers roll on the authority and persist/replicate the committed result.
 */
UCLASS(BlueprintType)
class UTITEMSRUNTIME_API UUTLootTable : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Loot")
	TArray<FUTLootEntry> Entries;
	/** Guaranteed entries once, plus 0..1024 weighted draws, in authored order. Advances Stream. */
	FUTLootResult Roll(FRandomStream& Stream, int32 Rolls = 1) const;
	/** Same seed, table ordering and roll count produce the same result. */
	UFUNCTION(BlueprintCallable, Category = "UT|Loot")
	FUTLootResult RollWithSeed(int32 Seed, int32 Rolls = 1) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
