// UT Templates - inventory types: stacks and snapshots.
#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UTItemInstance.h"
#include "UTInventoryTypes.generated.h"

class UUTInventoryComponent;

/**
 * One stack of identical items identified by ItemId.
 *
 * Definition-only stack items stay as (ItemId, Count) pairs on purpose: plain
 * stackables must not each become a UObject. Per-instance mutable item state lives
 * in the UTItemsRuntime instance layer, separate from these stacks.
 */
USTRUCT(BlueprintType)
struct UTINVENTORYRUNTIME_API FUTItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	int32 Count = 0;
};

/**
 * Fast Array Serializer item: one replicated stack with delta tracking. Clients receive
 * add/remove/change notifications instead of full-array resends, and their live view
 * (FUTItemStack) stays authoritative-free - mutations happen only on the server.
 */
USTRUCT()
struct UTINVENTORYRUNTIME_API FUTInventoryStackItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FUTItemStack Stack;
};

/**
 * Replicated stack list. Mutations are server-only; clients observe deltas through the
 * inventory component's OnItemAdded / OnItemRemoved / OnInventoryChanged delegates.
 */
USTRUCT()
struct UTINVENTORYRUNTIME_API FUTInventoryFastArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUTInventoryStackItem> Items;

	/** Set by the owning component constructor on both server and clients. */
	UPROPERTY(Transient)
	TObjectPtr<UUTInventoryComponent> Owner;

	// Client-side replication deltas (never run on the server).
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);

	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);

	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUTInventoryStackItem, FUTInventoryFastArray>(
			Items, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FUTInventoryFastArray> : public TStructOpsTypeTraitsBase2<FUTInventoryFastArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/** Unique items use their own FastArray; ordinary stacks keep their existing wire layout. */
USTRUCT()
struct UTINVENTORYRUNTIME_API FUTInventoryInstanceEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()
	UPROPERTY()
	FUTItemInstance Instance;
};

USTRUCT()
struct UTINVENTORYRUNTIME_API FUTInventoryInstanceArray : public FFastArraySerializer
{
	GENERATED_BODY()
	UPROPERTY()
	TArray<FUTInventoryInstanceEntry> Items;
	UPROPERTY(Transient)
	TObjectPtr<UUTInventoryComponent> Owner;

	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& Parameters)
	{
		return FastArrayDeltaSerialize<FUTInventoryInstanceEntry, FUTInventoryInstanceArray>(Items, Parameters, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FUTInventoryInstanceArray> : TStructOpsTypeTraitsBase2<FUTInventoryInstanceArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/** Plain data snapshot for save/load orchestration. */
USTRUCT(BlueprintType)
struct UTINVENTORYRUNTIME_API FUTInventorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TArray<FUTItemStack> Stacks;

	/** Additive save schema: old snapshots have an empty instance array. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UT")
	TArray<FUTItemInstance> Instances;
};
