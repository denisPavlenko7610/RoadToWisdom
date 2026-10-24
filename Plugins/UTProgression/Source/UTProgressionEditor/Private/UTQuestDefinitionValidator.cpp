// UT Templates - quest definition validation implementation.
#include "UTQuestDefinitionValidator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UTQuestDefinition.h"

UUTQuestDefinitionValidator::UUTQuestDefinitionValidator()
{
	bIsEnabled = true;
}

bool UUTQuestDefinitionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return InAsset && InAsset->IsA<UUTQuestDefinition>();
}

EDataValidationResult UUTQuestDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UUTQuestDefinition* Quest = Cast<UUTQuestDefinition>(InAsset);
	if (!Quest) {
		return EDataValidationResult::NotValidated;
	}

	if (Quest->QuestId.IsNone()) {
		AssetFails(InAsset, NSLOCTEXT("UTValidation", "QuestMissingId", "Quest definition has no QuestId; saves and prerequisites use it."));
		return EDataValidationResult::Invalid;
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassPaths.Add(UUTQuestDefinition::StaticClass()->GetClassPathName());
	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssets(Filter, Assets);
	for (const FAssetData& Other : Assets) {
		FName OtherId;
		if (Other.GetTagValue(GET_MEMBER_NAME_CHECKED(UUTQuestDefinition, QuestId), OtherId) && OtherId == Quest->QuestId && Other.AssetName != InAsset->GetFName()) {
			AssetFails(InAsset, FText::FromString(FString::Printf(
									TEXT("QuestId '%s' is already claimed by '%s'."), *Quest->QuestId.ToString(), *Other.AssetName.ToString())));
			return EDataValidationResult::Invalid;
		}
	}

	for (const FUTObjectiveDef& Objective : Quest->Objectives) {
		if (Objective.ObjectiveId.IsNone()) {
			AssetFails(InAsset, NSLOCTEXT("UTValidation", "ObjectiveMissingId", "Quest has an objective without an ObjectiveId."));
			return EDataValidationResult::Invalid;
		}
		if (!Objective.EventTag.IsValid()) {
			AssetFails(InAsset, FText::FromString(FString::Printf(
									TEXT("Objective '%s' has an invalid EventTag."), *Objective.ObjectiveId.ToString())));
			return EDataValidationResult::Invalid;
		}
		if (Objective.RequiredCount < 1) {
			AssetFails(InAsset, FText::FromString(FString::Printf(
									TEXT("Objective '%s' needs RequiredCount >= 1."), *Objective.ObjectiveId.ToString())));
			return EDataValidationResult::Invalid;
		}
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}
