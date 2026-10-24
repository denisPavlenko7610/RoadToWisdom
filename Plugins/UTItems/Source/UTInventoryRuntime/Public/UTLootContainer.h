#pragma once
#include "UTWorldItem.h"
#include "UTSaveable.h"
#include "UTLootContainer.generated.h"
class UUTLootTable;
class UUTCondition;

UENUM(BlueprintType)
enum class EUTLootContainerPhase : uint8
{
	Closed,
	Opened,
	Looted
};

/** One coherent replicated/persistent shared state, including unique item identities. */
USTRUCT(BlueprintType)
struct UTINVENTORYRUNTIME_API FUTLootContainerState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UT|Loot")
	EUTLootContainerPhase Phase = EUTLootContainerPhase::Closed;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UT|Loot")
	TArray<FUTItemPayload> Contents;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUTLootContainerChanged);

/** Shared chest capability with no UI framework. Open generates once; take commits
 * only accepted quantities. RPC intents belong on owned interactors/controllers.
 * Per-player loot is deliberately not part of this shared world-state contract.
 */
UCLASS(Blueprintable)
class UTINVENTORYRUNTIME_API AUTLootContainer : public AActor, public IUTInteractable, public IUTSaveable
{
	GENERATED_BODY()
public:
	AUTLootContainer();
	/** Optional pure gate, authored on both roles. Server rechecks it for open/take. */
	UPROPERTY(EditAnywhere, Instanced, Category = "UT|Loot")
	TObjectPtr<UUTCondition> UseCondition;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;
	/** Used when LootTable is unset. Unique configured records get ids on first open if unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Loot")
	TArray<FUTItemPayload> InitialContents;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Loot")
	TObjectPtr<UUTLootTable> LootTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Loot")
	int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Loot", meta = (ClampMin = "0", ClampMax = "1024"))
	int32 Rolls = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Loot", meta = (ClampMin = "0"))
	float UseDistance = 250.f;
	/** Set a stable world id to opt in to IUTSaveable persistence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT|Save")
	FName StableId;
	UPROPERTY(BlueprintAssignable, Category = "UT|Loot")
	FUTLootContainerChanged OnContentsChanged;
	UFUNCTION(BlueprintPure, Category = "UT|Loot")
	FUTLootContainerState GetLootState() const { return State; }
	/** Server only; validates user/range. Reopening never rerolls. */
	UFUNCTION(BlueprintCallable, Category = "UT|Loot")
	bool Open(AActor* User);
	/** Server only. Index refers to GetLootState contents; validate/refresh UI after each result. */
	UFUNCTION(BlueprintCallable, Category = "UT|Loot")
	int32 TakeItem(AActor* User, int32 Index, int32 Count);
	UFUNCTION(BlueprintCallable, Category = "UT|Loot")
	int32 TakeAll(AActor* User);
	virtual bool CanInteract_Implementation(const FUTInteractionInfo& Info) const override;
	virtual void Interact_Implementation(const FUTInteractionInfo& Info) override;
	virtual FText GetInteractionPrompt_Implementation(const FUTInteractionInfo&) const override;
	virtual FName GetSaveStableId_Implementation() const override { return StableId; }
	virtual FInstancedStruct CaptureState_Implementation() const override;
	virtual bool RestoreState_Implementation(const FInstancedStruct& Saved) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_State)
	FUTLootContainerState State;
	UFUNCTION()
	void OnRep_State();
	bool CanUse(AActor* User) const;
	bool bTransacting = false;
};
