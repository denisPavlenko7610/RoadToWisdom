// UT Templates - typed gameplay message bus implementation.
#include "UTMessageBusSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

FDelegateHandle UUTMessageBusSubsystem::Listen(FGameplayTag Tag, const FUTMessageCallback& Callback)
{
	if (!Tag.IsValid() || !Callback.IsBound()) {
		return FDelegateHandle();
	}

	Listeners.FindOrAdd(Tag).Native.Add(Callback);
	const FDelegateHandle Handle = Callback.GetHandle();
	NativeHandleToTag.Add(Handle, Tag);
	return Handle;
}

void UUTMessageBusSubsystem::RemoveListener(FDelegateHandle Handle)
{
	const FGameplayTag* FoundTag = NativeHandleToTag.Find(Handle);
	if (!FoundTag) {
		return;
	}

	const FGameplayTag Tag = *FoundTag;
	if (FUTTagListeners* TagListeners = Listeners.Find(Tag)) {
		TagListeners->Native.RemoveAll(
			[Handle](const FUTMessageCallback& Callback) {
				return Callback.GetHandle() == Handle;
			});
	}

	NativeHandleToTag.Remove(Handle);

	// Also drop the tag entry when this was its last subscriber.
	PruneTag(Tag);
}

void UUTMessageBusSubsystem::ListenForMessage(FGameplayTag Tag, FUTMessageDelegate Message)
{
	if (!Tag.IsValid() || !Message.IsBound()) {
		return;
	}

	Listeners.FindOrAdd(Tag).Dynamic.Add(Message);
}

void UUTMessageBusSubsystem::ClearMessage(FGameplayTag Tag, FUTMessageDelegate Message)
{
	FUTTagListeners* TagListeners = Listeners.Find(Tag);
	if (!TagListeners) {
		return;
	}

	TagListeners->Dynamic.RemoveAll(
		[&Message](const FUTMessageDelegate& Entry) {
			return Entry.GetUObject() == Message.GetUObject() && Entry.GetFunctionName() == Message.GetFunctionName();
		});

	PruneTag(Tag);
}

void UUTMessageBusSubsystem::ClearTagListeners(FGameplayTag Tag)
{
	TArray<FDelegateHandle, TInlineAllocator<8>> HandlesToRemove;
	for (const TPair<FDelegateHandle, FGameplayTag>& Pair : NativeHandleToTag) {
		if (Pair.Value == Tag || Pair.Value.MatchesTag(Tag)) {
			HandlesToRemove.Add(Pair.Key);
		}
	}
	for (const FDelegateHandle& Handle : HandlesToRemove) {
		RemoveListener(Handle);
	}

	TArray<FGameplayTag, TInlineAllocator<8>> TagsToRemove;
	for (const TPair<FGameplayTag, FUTTagListeners>& Pair : Listeners) {
		if (Pair.Key == Tag || Pair.Key.MatchesTag(Tag)) {
			TagsToRemove.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& TagToRemove : TagsToRemove) {
		Listeners.Remove(TagToRemove);
	}
}

void UUTMessageBusSubsystem::Broadcast(FGameplayTag Tag, const FInstancedStruct& Payload)
{
	if (!Tag.IsValid()) {
		return;
	}

	// Stamp the broadcast tag into the envelope so listeners can identify the message.
	FUTMessage Message;
	Message.Tag = Tag;
	Message.Payload = Payload;
	BroadcastEnvelope(Message);
}

void UUTMessageBusSubsystem::BroadcastEnvelope(const FUTMessage& Message)
{
	// Collect matching tags before invoking: a listener may broadcast, add or remove
	// listeners re-entrantly, and the map must not be mutated while iterated.
	TArray<FGameplayTag, TInlineAllocator<8>> MatchingTags;
	for (const TPair<FGameplayTag, FUTTagListeners>& Pair : Listeners) {
		// Listener fires when the broadcast tag equals its tag or is one of its children.
		if (Message.Tag.MatchesTag(Pair.Key)) {
			MatchingTags.Add(Pair.Key);
		}
	}

	for (const FGameplayTag& ListenerTag : MatchingTags) {
		FUTTagListeners* TagListeners = Listeners.Find(ListenerTag);
		if (!TagListeners) {
			continue;
		}

		// Snapshot per tag: invoking a listener may subscribe or unsubscribe.
		TArray<FUTMessageCallback, TInlineAllocator<4>> NativeSnapshot(TagListeners->Native);
		TArray<FUTMessageDelegate, TInlineAllocator<4>> DynamicSnapshot(TagListeners->Dynamic);
		for (const FUTMessageCallback& Callback : NativeSnapshot) {
			Callback.ExecuteIfBound(Message);
		}

		// TScriptDelegate bindings hold weak object refs, so a destroyed owner makes
		// IsBound() false; ExecuteIfBound skips it but keeps it until pruning.
		for (const FUTMessageDelegate& Event : DynamicSnapshot) {
			Event.ExecuteIfBound(Message);
		}
	}

	for (const FGameplayTag& ListenerTag : MatchingTags) {
		PruneTag(ListenerTag);
	}
}

void UUTMessageBusSubsystem::PruneTag(FGameplayTag Tag)
{
	FUTTagListeners* TagListeners = Listeners.Find(Tag);
	if (!TagListeners) {
		return;
	}

	// Drop bindings whose UObject or lambda owner is gone, so destroyed listeners do
	// not accumulate as permanently skipped entries.
	TArray<FDelegateHandle, TInlineAllocator<8>> DeadHandles;
	TagListeners->Native.RemoveAll(
		[&DeadHandles](const FUTMessageCallback& Callback) {
			const bool bDead = !Callback.IsBound();
			if (bDead && Callback.GetHandle().IsValid()) {
				DeadHandles.Add(Callback.GetHandle());
			}
			return bDead;
		});
	for (const FDelegateHandle& Handle : DeadHandles) {
		NativeHandleToTag.Remove(Handle);
	}

	TagListeners->Dynamic.RemoveAll(
		[](const FUTMessageDelegate& Event) {
			return !Event.IsBound();
		});

	if (TagListeners->Native.Num() == 0 && TagListeners->Dynamic.Num() == 0) {
		Listeners.Remove(Tag);
	}
}

bool UUTMessageBusSubsystem::HasListeners(FGameplayTag Tag) const
{
	for (const TPair<FGameplayTag, FUTTagListeners>& Pair : Listeners) {
		if (Tag.MatchesTag(Pair.Key)) {
			for (const FUTMessageCallback& Callback : Pair.Value.Native) {
				if (Callback.IsBound()) {
					return true;
				}
			}
			for (const FUTMessageDelegate& Event : Pair.Value.Dynamic) {
				if (Event.IsBound()) {
					return true;
				}
			}
		}
	}
	return false;
}

UUTMessageBusSubsystem* UUTMessageBusSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine && WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UUTMessageBusSubsystem>() : nullptr;
}

void UUTMessageBusSubsystem::Deinitialize()
{
	Listeners.Reset();
	NativeHandleToTag.Reset();
	Super::Deinitialize();
}
