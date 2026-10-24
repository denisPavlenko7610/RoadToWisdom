// UT Templates - item definition data asset.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UTItemDefinition.generated.h"

/**
 * Data-driven item description. This is the item domain boundary asset: both
 * UTInventoryRuntime and UTEquipmentRuntime resolve definitions through the item
 * registry, never through each other.
 *
 * Identity model: the definition is a Primary Data Asset. ItemId (an FName owned by
 * the game) is stamped into GetPrimaryAssetId as type "UTItem", so the Asset Manager
 * can enumerate and scan the whole item database; UUTItemRegistrySubsystem resolves
 * ItemId -> definition at runtime. Author these as Data Assets in the project.
 */
UCLASS(BlueprintType)
class UTITEMSRUNTIME_API UUTItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identity used by the registry, saves and cross-system references. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (MultiLine = "true"))
	FText Description;

	/** Items per stack; 1 means the item never stacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	/** Meaning of the item (Item.Weapon, Item.Material) - for filtering and rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	FGameplayTagContainer ItemTags;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("UTItem"), ItemId);
	}
};
