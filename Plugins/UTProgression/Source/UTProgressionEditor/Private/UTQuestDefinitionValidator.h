// UT Templates - quest definition validation.
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "UTQuestDefinitionValidator.generated.h"

/** QuestId present and unique; objectives need ids, valid event tags, RequiredCount >= 1. */
UCLASS()
class UTPROGRESSIONEDITOR_API UUTQuestDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UUTQuestDefinitionValidator();

	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};
