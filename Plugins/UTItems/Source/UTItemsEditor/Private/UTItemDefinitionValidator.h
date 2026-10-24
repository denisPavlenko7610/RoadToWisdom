// UT Templates - item definition validation.
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "UTItemDefinitionValidator.generated.h"

/** ItemId present and unique across the item database, stack size >= 1. */
UCLASS()
class UTITEMSEDITOR_API UUTItemDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UUTItemDefinitionValidator();

	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};
