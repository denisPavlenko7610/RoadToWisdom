// UT Templates - item registry subsystem implementation.
#include "UTItemRegistry.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"

UUTItemRegistrySubsystem* UUTItemRegistrySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine && WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UUTItemRegistrySubsystem>() : nullptr;
}

void UUTItemRegistrySubsystem::RegisterItem(FName ItemId, TSoftObjectPtr<UUTItemDefinition> Definition)
{
	if (ItemId.IsNone() || Definition.IsNull()) {
		return;
	}
	Registered.Add(ItemId, Definition);
	ResolvedCache.Remove(ItemId);
}

void UUTItemRegistrySubsystem::UnregisterItem(FName ItemId)
{
	Registered.Remove(ItemId);
	ResolvedCache.Remove(ItemId);
}

void UUTItemRegistrySubsystem::ScanFromAssetManager() const
{
	if (bScannedAssetManager) {
		return;
	}
	bScannedAssetManager = true;

	// Headless/commandlet contexts may have no Asset Manager; manual registrations
	// keep the registry functional there.
	UAssetManager* ManagerPtr = UAssetManager::GetIfValid();
	if (!ManagerPtr) {
		return;
	}
	UAssetManager& Manager = *ManagerPtr;
	TArray<FPrimaryAssetId> AssetIds;
	Manager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("UTItem")), AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds) {
		const TSoftObjectPtr<UUTItemDefinition> Path(Manager.GetPrimaryAssetPath(AssetId));
		if (!Path.IsNull()) {
			Registered.Add(AssetId.PrimaryAssetName, Path);
		}
	}
}

UUTItemDefinition* UUTItemRegistrySubsystem::GetItemDefinition(FName ItemId) const
{
	if (ItemId.IsNone()) {
		return nullptr;
	}
	ScanFromAssetManager();

	if (TObjectPtr<UUTItemDefinition>* Cached = ResolvedCache.Find(ItemId)) {
		return *Cached;
	}

	const TSoftObjectPtr<UUTItemDefinition>* Soft = Registered.Find(ItemId);
	if (!Soft) {
		return nullptr;
	}

	// Small data assets: one synchronous load per item id, cached for the session.
	UUTItemDefinition* Definition = Soft->LoadSynchronous();
	ResolvedCache.Add(ItemId, Definition);
	return Definition;
}

TSoftObjectPtr<UUTItemDefinition> UUTItemRegistrySubsystem::GetItemDefinitionSoft(FName ItemId) const
{
	ScanFromAssetManager();
	const TSoftObjectPtr<UUTItemDefinition>* Soft = Registered.Find(ItemId);
	return Soft ? *Soft : TSoftObjectPtr<UUTItemDefinition>();
}

int32 UUTItemRegistrySubsystem::GetMaxStackSize(FName ItemId) const
{
	const UUTItemDefinition* Definition = GetItemDefinition(ItemId);
	return Definition ? FMath::Max(1, Definition->MaxStackSize) : 1;
}

bool UUTItemRegistrySubsystem::IsItemKnown(FName ItemId) const
{
	ScanFromAssetManager();
	return Registered.Contains(ItemId);
}

void UUTItemRegistrySubsystem::Deinitialize()
{
	Registered.Reset();
	ResolvedCache.Reset();
	bScannedAssetManager = false;
	Super::Deinitialize();
}
