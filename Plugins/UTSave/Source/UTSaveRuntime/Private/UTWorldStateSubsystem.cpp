// UT Templates - world state subsystem implementation.
#include "UTWorldStateSubsystem.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
static TAutoConsoleVariable<int32> CVarUTDebugSave(
	TEXT("ut.Debug.Save"), 0, TEXT("Enable toolkit save diagnostics."), ECVF_Cheat);
// Lifetime, in seconds, of the debug label drawn above saveable actors.
constexpr float DebugLabelLifetimeSeconds = 5.f;
#endif

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UTSaveable.h"
#include "UTSaveLog.h"

FUTResult UUTWorldStateSubsystem::CaptureWorldState(FUTWorldStateSnapshot& OutSnapshot) const
{
	OutSnapshot = FUTWorldStateSnapshot();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) {
		return FUTResult::Failure(TEXT("NotAuthority"), FText::FromString(TEXT("World capture requires server authority.")));
	}

	TMap<FName, TWeakObjectPtr<AActor>> ByStableId;
	const FUTResult Result = CollectSaveables(ByStableId);
	if (!Result.IsSuccess()) {
		return Result;
	}

	for (const TPair<FName, TWeakObjectPtr<AActor>>& Pair : ByStableId) {
		if (!Pair.Value.IsValid()) {
			continue;
		}
		FUTWorldStateEntry Entry;
		Entry.StableId = Pair.Key;
		Entry.State = IUTSaveable::Execute_CaptureState(Pair.Value.Get());
		OutSnapshot.Entries.Add(Entry);
	}
	return FUTResult::Success();
}

int32 UUTWorldStateSubsystem::ApplyWorldState(const FUTWorldStateSnapshot& Snapshot, FUTResult& OutResult)
{
	OutResult = FUTResult::Success();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) {
		OutResult = FUTResult::Failure(TEXT("NotAuthority"), FText::FromString(TEXT("World restore requires server authority.")));
		return 0;
	}

	TMap<FName, TWeakObjectPtr<AActor>> ByStableId;
	const FUTResult CollectResult = CollectSaveables(ByStableId);
	if (!CollectResult.IsSuccess()) {
		OutResult = CollectResult;
		return 0;
	}

	int32 Applied = 0;
	for (const FUTWorldStateEntry& Entry : Snapshot.Entries) {
		const TWeakObjectPtr<AActor>* Target = ByStableId.Find(Entry.StableId);
		if (!Target || !Target->IsValid()) {
			// The object may be destroyed or streamed out. A restore continues: the
			// stable-id contract means its entry is meaningful for the next session.
			UE_LOG(LogUTSave, Warning, TEXT("World state has no live object with stable id '%s'."), *Entry.StableId.ToString());
			continue;
		}
		if (IUTSaveable::Execute_RestoreState(Target->Get(), Entry.State)) {
			++Applied;
		}
		else {
			OutResult = FUTResult::Failure(
				TEXT("RestoreRejected"), FText::FromString(
											 FString::Printf(TEXT("Saveable '%s' rejected its state."), *Entry.StableId.ToString())));
			UE_LOG(LogUTSave, Warning, TEXT("World restore rejected by '%s'."), *Entry.StableId.ToString());
		}
	}
	return Applied;
}

TArray<FName> UUTWorldStateSubsystem::GetDuplicateStableIds() const
{
	TSet<FName> Seen;
	TSet<FName> Duplicates;

	TArray<IUTSaveable*> Saveables;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) {
		AActor* Actor = *It;
		if (Actor && Actor->Implements<UUTSaveable>()) {
			const FName StableId = IUTSaveable::Execute_GetSaveStableId(Actor);
			if (StableId.IsNone()) {
				continue;
			}
			if (Seen.Contains(StableId)) {
				Duplicates.Add(StableId);
			}
			Seen.Add(StableId);
		}
	}
	return Duplicates.Array();
}

FUTResult UUTWorldStateSubsystem::CollectSaveables(TMap<FName, TWeakObjectPtr<AActor>>& OutByStableId) const
{
	OutByStableId.Reset();

	for (TActorIterator<AActor> It(GetWorld()); It; ++It) {
		AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UUTSaveable>()) {
			continue;
		}

		const FName StableId = IUTSaveable::Execute_GetSaveStableId(Actor);
		if (StableId.IsNone()) {
			UE_LOG(
				LogUTSave, Warning, TEXT("Saveable actor '%s' has no stable id configured; it will not be saved."),
				*Actor->GetName());
			continue;
		}

		if (const TWeakObjectPtr<AActor>* Existing = OutByStableId.Find(StableId)) {
			return FUTResult::Failure(
				TEXT("DuplicateStableId"),
				FText::FromString(
					FString::Printf(
						TEXT("Stable id '%s' is used by more than one saveable ('%s', '%s')."),
						*StableId.ToString(), *GetNameSafe(Existing->Get()), *Actor->GetName())));
		}

#if !UE_BUILD_SHIPPING
		if (CVarUTDebugSave.GetValueOnGameThread() != 0) {
			UE_LOG(
				LogUTSave, Display, TEXT("UTSave id=%s actor=%s authority=%d"), *StableId.ToString(), *Actor->GetPathName(),
				Actor->HasAuthority());
			DrawDebugString(GetWorld(), Actor->GetActorLocation(), StableId.ToString(), nullptr, FColor::Purple, DebugLabelLifetimeSeconds);
		}
#endif
		OutByStableId.Add(StableId, Actor);
	}
	return FUTResult::Success();
}
