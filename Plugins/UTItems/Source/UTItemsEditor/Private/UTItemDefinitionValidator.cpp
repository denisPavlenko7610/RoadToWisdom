// UT Templates - item definition validation implementation.
#include "UTItemDefinitionValidator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "UTItemDefinition.h"

UUTItemDefinitionValidator::UUTItemDefinitionValidator()
{
	bIsEnabled = true;
}

bool UUTItemDefinitionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return InAsset && InAsset->IsA<UUTItemDefinition>();
}

EDataValidationResult UUTItemDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UUTItemDefinition* Item = Cast<UUTItemDefinition>(InAsset);
	if (!Item) {
		return EDataValidationResult::NotValidated;
	}

	if (Item->ItemId.IsNone()) {
		AssetFails(InAsset, NSLOCTEXT("UTValidation", "ItemMissingId", "Item definition has no ItemId; the registry cannot resolve it."));
		return EDataValidationResult::Invalid;
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassPaths.Add(UUTItemDefinition::StaticClass()->GetClassPathName());
	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssets(Filter, Assets);
	for (const FAssetData& Other : Assets) {
		FName OtherId;
		if (Other.GetTagValue(GET_MEMBER_NAME_CHECKED(UUTItemDefinition, ItemId), OtherId) && OtherId == Item->ItemId && Other.AssetName != InAsset->GetFName()) {
			AssetFails(InAsset, FText::FromString(FString::Printf(
									TEXT("ItemId '%s' is already claimed by '%s'."), *Item->ItemId.ToString(), *Other.AssetName.ToString())));
			return EDataValidationResult::Invalid;
		}
	}

	if (Item->MaxStackSize < 1) {
		AssetFails(InAsset, NSLOCTEXT("UTValidation", "ItemBadStack", "MaxStackSize must be at least 1."));
		return EDataValidationResult::Invalid;
	}

	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}
