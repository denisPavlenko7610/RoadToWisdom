// UT Templates - world state saveable contract.
#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Interface.h"
#include "UTSaveable.generated.h"

UINTERFACE(BlueprintType)
class UUTSaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * An object whose world state must survive save/load: doors, chests, levers,
 * breakables, collected pickups, boss flags.
 *
 * StableId is the identity contract: it must be a configured FName that is stable
 * across sessions ("Farm.Level.Chest01"), never derived from a runtime pointer or
 * index. Duplicate ids in one world are a hard error at capture time.
 *
 * Server-only by rule: capture and restore run on the authority; clients receive the
 * restored state through normal replication, not through the save system.
 */
class UTSAVERUNTIME_API IUTSaveable
{
	GENERATED_BODY()

public:
	/** Configured persistent identity; NAME_None (unconfigured) objects are skipped with a warning. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Save")
	FName GetSaveStableId() const;

	/** Capture the state that should persist. Use a dedicated save-state struct, not a blob. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Save")
	FInstancedStruct CaptureState() const;

	/** Apply a previously captured state. Restores publish no gameplay events. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UT|Save")
	bool RestoreState(const FInstancedStruct& State);
};
