// UT Templates - stats types.
#pragma once

#include "CoreMinimal.h"
#include "UTStatsTypes.generated.h"

UENUM(BlueprintType)
enum class EUTStatModifierOp : uint8
{
	/** Added to the base value before multiplication. */
	Add,
	/** Multiplies: (base + add) * (1 + sum of multiply magnitudes). */
	Multiply
};

/** One replicated stat row: base value from the authority and the final value after modifiers. */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTReplicatedStatEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName StatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float BaseValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	float FinalValue = 0.f;
};

/** Opaque reference to one registered modifier; id 0 is invalid. */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTStatModifierHandle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
};

/** Plain data snapshot for save/load orchestration (base values only). */
USTRUCT(BlueprintType)
struct UTCOMBATLITE_API FUTStatsSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TMap<FName, float> BaseValues;
};
