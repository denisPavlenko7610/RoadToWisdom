// UT Templates - item registry subsystem.
#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UTItemDefinition.h"
#include "UTItemRegistry.generated.h"

/**
 * ItemId -> UUTItemDefinition resolution, the single lookup point of the item domain.
 *
 * Two registration sources:
 *  1. The Asset Manager: every UUTItemDefinition primary asset of type "UTItem" is
 *     registered automatically on first use (lazy, one scan per session).
 *  2. Manual registration for definitions that live outside the Asset Manager's
 *     scan paths (test fixtures, procedurally assembled items).
 *
 * UTInventoryRuntime and UTEquipmentRuntime both go through this registry; neither
 * references the other. Lookups that hit a soft path load synchronously and cache -
 * item definitions are small data assets, and this keeps the API free of async
 * plumbing. Hot-reload style re-registration is allowed: RegisterItem overwrites.
 */
UCLASS(BlueprintType)
class UTITEMSRUNTIME_API UUTItemRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Subsystem of the world's game instance; null outside normal play. */
	UFUNCTION(BlueprintPure, Category = "UT|Items", DisplayName = "Get UT Item Registry", Meta = (WorldContext = "WorldContextObject"))
	static UUTItemRegistrySubsystem* Get(const UObject* WorldContextObject);

	/** Register (or replace) one ItemId -> definition mapping. */
	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	void RegisterItem(FName ItemId, TSoftObjectPtr<UUTItemDefinition> Definition);

	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	void UnregisterItem(FName ItemId);

	/** Resolved definition for ItemId (Asset Manager entry or manual registration); sync-loads and caches. Null when unknown. */
	UFUNCTION(BlueprintPure, Category = "UT|Items")
	UUTItemDefinition* GetItemDefinition(FName ItemId) const;

	/** Soft reference for ItemId, without forcing a load. */
	UFUNCTION(BlueprintPure, Category = "UT|Items")
	TSoftObjectPtr<UUTItemDefinition> GetItemDefinitionSoft(FName ItemId) const;

	/** Max stack size for ItemId; 1 when the item is unknown (never stacks). */
	UFUNCTION(BlueprintPure, Category = "UT|Items")
	int32 GetMaxStackSize(FName ItemId) const;

	/** Scan Asset Manager primary assets of type "UTItem" into the registry. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "UT|Items")
	void ScanFromAssetManager() const;

	/** True when ItemId resolves to a definition (no load forced). */
	UFUNCTION(BlueprintPure, Category = "UT|Items")
	bool IsItemKnown(FName ItemId) const;

	virtual void Deinitialize() override;

private:
	// Lazy-loaded state mutated by const lookups (soft-load + cache are logically read operations).
	UPROPERTY(Transient)
	mutable TMap<FName, TSoftObjectPtr<UUTItemDefinition>> Registered;

	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UUTItemDefinition>> ResolvedCache;

	mutable bool bScannedAssetManager = false;
};
