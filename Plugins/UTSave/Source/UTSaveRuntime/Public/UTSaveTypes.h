// UT Templates - save types.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UTResult.h"
#include "StructUtils/InstancedStruct.h"
#include "UTSaveTypes.generated.h"

/** Fired when an async slot operation finishes. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FUTSaveCompletedSignature, FUTResult, Result, FName, SlotName);

/** One serialized section inside a slot: raw bytes plus identity for integrity checks. */
USTRUCT(BlueprintType)
struct UTSAVERUNTIME_API FUTSaveSectionRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName Section;

	/** Caller-defined schema version; readers must pass the version they understand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int32 Version = 0;

	/** Struct path of the section data type; empty for custom byte sections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FSoftObjectPath StructPath;

	/**
	 * Serialized byte count of Bytes, recorded for integrity checks only. This is the size
	 * of the serialized archive and is deliberately not comparable to the in-memory size of
	 * the C++ struct (TArray, FString and padding make those two unrelated numbers).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int32 DataSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TArray<uint8> Bytes;
};

/** The object persisted to one slot. Never fill it directly - go through UUTSaveSubsystem. */
UCLASS(BlueprintType)
class UTSAVERUNTIME_API UUTSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bump when the section record layout itself changes (not for section schemas). */
	UPROPERTY(VisibleAnywhere, Category = "UT")
	int32 FormatVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "UT")
	FName SlotName;

	UPROPERTY(VisibleAnywhere, Category = "UT")
	FDateTime Timestamp;

	UPROPERTY(VisibleAnywhere, Category = "UT")
	TMap<FName, FUTSaveSectionRecord> Sections;
};

/** One saveable object's captured state, keyed by its stable id. */
USTRUCT(BlueprintType)
struct UTSAVERUNTIME_API FUTWorldStateEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FInstancedStruct State;
};

/** Plain data snapshot of every saveable object in a world, for the save orchestrator. */
USTRUCT(BlueprintType)
struct UTSAVERUNTIME_API FUTWorldStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TArray<FUTWorldStateEntry> Entries;
};
