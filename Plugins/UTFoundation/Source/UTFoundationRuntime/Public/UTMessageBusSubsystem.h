// UT Templates - typed gameplay message bus subsystem.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UTMessageTypes.h"
#include "UTMessageBusSubsystem.generated.h"

/**
 * Tag-routed, synchronous, typed gameplay messages.
 *
 * A broadcast of "UT.Message.Combat.Died" notifies listeners registered on that tag and
 * on any of its ancestors (GameplayTag matching semantics). Delivery is immediate:
 * handlers run in the broadcast's stack frame, in registration order, which is what
 * gameplay logic (checkpoint -> respawn) expects. The engine's experimental
 * AsyncMessageSystem covers async/threaded delivery and is deliberately not reimplemented here.
 *
 * Typed listeners (ListenTyped in C++) declare the payload struct they understand and
 * are skipped with a warning when a broadcast carries a different struct - the struct
 * type is the contract, not a convention. Blueprint listeners receive the envelope and
 * mostly match on the tag.
 *
 * Native listeners get a removal handle. Dynamic (Blueprint) listeners are lifetime-safe:
 * a binding whose object has been garbage collected never fires and is pruned on the
 * next broadcast. Tags carry routing, not identity - do not invent one tag per object.
 */
UCLASS(BlueprintType)
class UTFOUNDATIONRUNTIME_API UUTMessageBusSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Untyped native callback; the payload is available through FUTMessage::Payload. */
	DECLARE_DELEGATE_OneParam(FUTMessageCallback, const FUTMessage&);

	/** Register a native listener. Returns a handle for RemoveListener; invalid if the tag or callback is empty. */
	FDelegateHandle Listen(FGameplayTag Tag, const FUTMessageCallback& Callback);

	/**
	 * Register a typed native listener. The callback only fires when the broadcast
	 * payload struct is TMessage (or a child of it); anything else logs a warning and
	 * skips the listener instead of reinterpreting unrelated bytes.
	 */
	template <typename TMessage>
	FDelegateHandle ListenTyped(FGameplayTag Tag, TFunction<void(const TMessage&)> Callback)
	{
		const UScriptStruct* ExpectedStruct = TMessage::StaticStruct();
		TFunction<void(const TMessage&)> CapturedCallback = MoveTemp(Callback);
		return Listen(Tag, FUTMessageCallback::CreateLambda([ExpectedStruct, CapturedCallback = MoveTemp(CapturedCallback)](const FUTMessage& Message) {
						  const UScriptStruct* ActualStruct = Message.Payload.GetScriptStruct();
						  if (!ExpectedStruct || !ActualStruct || !ActualStruct->IsChildOf(ExpectedStruct)) {
							  UE_LOG(LogTemp, Display,
									 TEXT("Typed listener for '%s' skipped a broadcast: payload was '%s', expected '%s'."),
									 *Message.Tag.ToString(),
									 ActualStruct ? *ActualStruct->GetName() : TEXT("None"),
									 ExpectedStruct ? *ExpectedStruct->GetName() : TEXT("None"));
							  return;
						  }
						  CapturedCallback(*reinterpret_cast<const TMessage*>(Message.Payload.GetMemory()));
					  }));
	}

	/** Remove a native listener registered with Listen/ListenTyped. Safe to call with an unknown handle. */
	void RemoveListener(FDelegateHandle Handle);

	/** Register a Blueprint listener (Blueprint: "Create Event"). */
	UFUNCTION(BlueprintCallable, Category = "UT|Messages")
	void ListenForMessage(FGameplayTag Tag, FUTMessageDelegate Message);

	/** Remove an exact Blueprint binding previously registered with ListenForMessage. */
	UFUNCTION(BlueprintCallable, Category = "UT|Messages")
	void ClearMessage(FGameplayTag Tag, FUTMessageDelegate Message);

	/** Remove all listeners (native and Blueprint) registered under Tag or its children. */
	UFUNCTION(BlueprintCallable, Category = "UT|Messages")
	void ClearTagListeners(FGameplayTag Tag);

	/** Notify listeners for Tag and its ancestors. Immediate, in-frame delivery. */
	UFUNCTION(BlueprintCallable, Category = "UT|Messages")
	void Broadcast(FGameplayTag Tag, const FInstancedStruct& Payload);

	/** Typed C++ broadcast: wraps Payload into the envelope. */
	template <typename TMessage>
	void BroadcastMessage(FGameplayTag Tag, const TMessage& Payload)
	{
		Broadcast(Tag, FInstancedStruct::Make<TMessage>(Payload));
	}

	/** True when at least one live listener would receive a broadcast of Tag. */
	UFUNCTION(BlueprintPure, Category = "UT|Messages")
	bool HasListeners(FGameplayTag Tag) const;

	/** Subsystem of the world's game instance. Returns null outside normal play. */
	UFUNCTION(BlueprintPure, Category = "UT|Messages", DisplayName = "Get UT Message Bus", Meta = (WorldContext = "WorldContextObject"))
	static UUTMessageBusSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

private:
	struct FUTTagListeners
	{
		TArray<FUTMessageCallback> Native;
		TArray<FUTMessageDelegate> Dynamic;
	};

	TMap<FGameplayTag, FUTTagListeners> Listeners;
	TMap<FDelegateHandle, FGameplayTag> NativeHandleToTag;

	void BroadcastEnvelope(const FUTMessage& Message);
	void PruneTag(FGameplayTag Tag);
};
