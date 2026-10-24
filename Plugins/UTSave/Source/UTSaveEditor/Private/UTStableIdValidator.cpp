// UT Templates - stable save id validation implementation.
#include "UTStableIdValidator.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UTSaveable.h"

UUTStableIdValidator::UUTStableIdValidator()
{
	bIsEnabled = true;
}

bool UUTStableIdValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return InAsset && InAsset->IsA<UWorld>();
}

EDataValidationResult UUTStableIdValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UWorld* World = Cast<UWorld>(InAsset);
	if (!World) {
		// Only loaded worlds can be scanned; run validation with the map open.
		return EDataValidationResult::NotValidated;
	}

	TMap<FName, FString> Seen;
	EDataValidationResult Result = EDataValidationResult::Valid;
	for (TActorIterator<AActor> It(World); It; ++It) {
		const AActor* Actor = *It;
		if (!Actor || !Actor->Implements<UUTSaveable>()) {
			continue;
		}
		const FName StableId = IUTSaveable::Execute_GetSaveStableId(Actor);
		if (StableId.IsNone()) {
			continue;
		}
		if (FString* Existing = Seen.Find(StableId)) {
			AssetFails(const_cast<AActor*>(Actor), FText::FromString(FString::Printf(
													   TEXT("Stable id '%s' is also used by '%s' - world state identity must be unique."),
													   *StableId.ToString(), **Existing)));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		Seen.Add(StableId, Actor->GetName());
	}
	return Result;
}
