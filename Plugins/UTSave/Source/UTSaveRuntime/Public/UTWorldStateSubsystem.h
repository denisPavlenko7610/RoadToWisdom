// UT Templates - world state subsystem.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UTSaveTypes.h"
#include "UTWorldStateSubsystem.generated.h"

struct FInstancedStruct;

/**
 * Collects and replays the persistent state of every IUTSaveable actor in the world.
 *
 * Capture walks the world and asks each saveable for (StableId, State); Apply matches
 * entries back to live actors by stable id. Actors missing at restore time (destroyed
 * props, streamed-out levels) are reported but do not abort the restore.
 *
 * Server authority: run capture/restore on the server only. Clients see the restored
 * state arrive through ordinary replication - never save/load world state client-side.
 *
 * This subsystem does not touch disk by itself: the game's save orchestrator writes the
 * captured FUTWorldStateSnapshot into a UUTSaveSubsystem section and reads it back.
 */
UCLASS()
class UTSAVERUNTIME_API UUTWorldStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Collect the current state of every saveable actor. Fails on duplicate stable ids. */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult CaptureWorldState(FUTWorldStateSnapshot& OutSnapshot) const;

	/** Apply a captured snapshot to the live world. Returns the number of entries applied. */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	int32 ApplyWorldState(const FUTWorldStateSnapshot& Snapshot, FUTResult& OutResult);

	/** Stable ids currently seen more than once (editor/runtime validation helper). */
	UFUNCTION(BlueprintPure, Category = "UT|Save")
	TArray<FName> GetDuplicateStableIds() const;

private:
	/** Every actor implementing IUTSaveable. A full walk is fine: save/restore is rare. */
	FUTResult CollectSaveables(TMap<FName, TWeakObjectPtr<AActor>>& OutByStableId) const;
};
