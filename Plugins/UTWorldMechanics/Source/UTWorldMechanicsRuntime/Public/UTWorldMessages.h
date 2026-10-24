// UT Templates - world-mechanics gameplay messages.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UTWorldMessages.generated.h"

/**
 * Announced when a player adopts a checkpoint (tag UTTags::TAG_UT_Message_CheckpointReached,
 * "UT.Message.CheckpointReached"). The struct is the contract between checkpoint and
 * respawn: the respawn transform is explicit, no loose Vector/Value conventions.
 *
 * Checkpoints are per player: only the actor that entered the checkpoint is named, so
 * other players' respawn components must ignore the message.
 */
USTRUCT(BlueprintType)
struct UTWORLDMECHANICSRUNTIME_API FUTCheckpointReachedMessage
{
	GENERATED_BODY()

	/** Where to respawn: the checkpoint's transform plus its configured offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FTransform RespawnTransform = FTransform::Identity;

	/** The checkpoint actor that was reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Checkpoint;

	/** The player actor that reached it (the only consumer that should adopt it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	TWeakObjectPtr<AActor> Player;
};
