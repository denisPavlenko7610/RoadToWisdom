// UT Templates - stable save id validation.
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "UTStableIdValidator.generated.h"

/** Duplicate IUTSaveable stable ids inside one world break save/restore identity. */
UCLASS()
class UTSAVEEDITOR_API UUTStableIdValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UUTStableIdValidator();

	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};
