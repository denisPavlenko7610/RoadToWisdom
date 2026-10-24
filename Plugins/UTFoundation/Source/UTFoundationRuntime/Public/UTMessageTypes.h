// UT Templates - typed gameplay message types.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UTMessageTypes.generated.h"

/**
 * Envelope delivered to every listener. The tag routes the message; the payload is a
 * typed struct that carries the actual contract (FUTCheckpointReachedMessage,
 * FUTActorDiedMessage, a game-defined struct...).
 *
 * Rules of the road: direct relationships use direct calls; known one-to-one systems
 * use delegates/interfaces; cross-domain notifications go through this bus with a
 * purpose-built payload struct. Do not broadcast blobs of loose fields - a payload
 * struct with a comment explaining its fields IS the API between the two domains.
 */
USTRUCT(BlueprintType)
struct UTFOUNDATIONRUNTIME_API FUTMessage
{
	GENERATED_BODY()

	/** The tag this message was broadcast under; stamped automatically by Broadcast. */
	UPROPERTY(BlueprintReadOnly, Category = "UT")
	FGameplayTag Tag;

	/** Typed payload. Untyped listeners only look at the tag; typed listeners must match its struct type. */
	UPROPERTY(BlueprintReadOnly, Category = "UT")
	FInstancedStruct Payload;
};

/** Blueprint callback for ListenForMessage. Payload inspection is limited in BP - game systems should define typed native listeners. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FUTMessageDelegate, const FUTMessage&, Message);
