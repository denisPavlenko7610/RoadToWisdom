#pragma once
#include "CoreMinimal.h"
#include "UTItemInstance.generated.h"

/** A rolled modifier is data, not an executable effect. Game composition interprets it. */
USTRUCT(BlueprintType)
struct UTITEMSRUNTIME_API FUTItemRoll
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	FName ModifierId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	float Value = 0.f;
};

/** Small optional runtime state. -1 means the built-in field is unused.
 * CustomData is an opaque, versioned, game-owned byte schema (maximum 1024 bytes).
 * Never serialize UObject references/pointers into it. No dynamic UObject graph is accepted.
 */
USTRUCT(BlueprintType)
struct UTITEMSRUNTIME_API FUTItemInstanceState
{
	GENERATED_BODY()
	/** Highest roll count IsValid accepts. */
	static constexpr int32 MaxRollCount = 32;
	/** Largest CustomData payload IsValid accepts, in bytes. */
	static constexpr int32 MaxCustomDataBytes = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	int32 Ammo = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	float Durability = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	TArray<FUTItemRoll> Rolls;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	FName CustomSchema;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	int32 CustomVersion = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	TArray<uint8> CustomData;

	bool IsValid() const
	{
		if (Ammo < -1 || !FMath::IsFinite(Durability) || Durability < -1.f || Rolls.Num() > MaxRollCount || CustomData.Num() > MaxCustomDataBytes || CustomVersion < 0 || (!CustomData.IsEmpty() && CustomSchema.IsNone())) {
			return false;
		}
		for (const FUTItemRoll& Roll : Rolls) {
			if (Roll.ModifierId.IsNone() || !FMath::IsFinite(Roll.Value)) {
				return false;
			}
		}
		return true;
	}
};

/** One unique item. Identity survives transfer, replication and save/load.
 * ItemId resolves through UUTItemRegistrySubsystem, exactly like a stack's definition.
 * Create only on the server; plain stackable items do not need this structure.
 */
USTRUCT(BlueprintType)
struct UTITEMSRUNTIME_API FUTItemInstance
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "UT|Items")
	FGuid InstanceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	FName ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT|Items")
	FUTItemInstanceState State;

	bool IsValid() const { return InstanceId.IsValid() && !ItemId.IsNone() && State.IsValid(); }
	static FUTItemInstance Create(FName DefinitionId)
	{
		FUTItemInstance Item;
		Item.InstanceId = FGuid::NewGuid();
		Item.ItemId = DefinitionId;
		return Item;
	}
};
