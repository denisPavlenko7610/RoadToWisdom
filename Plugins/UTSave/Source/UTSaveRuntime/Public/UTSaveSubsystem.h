// UT Templates - versioned save subsystem.
#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UTResult.h"
#include "UTSaveTypes.h"
#include "UTSaveSubsystem.generated.h"

/**
 * Versioned save sections over the engine USaveGame system.
 *
 * One slot = one UUTSaveGame holding named sections. Modules write their own section
 * (Inventory.Items, Health, World.Position) with a schema version and read it back with
 * the same type; a changed struct path or version fails cleanly instead of corrupting.
 *
 * Save flow: WriteSection/WriteSectionAny on the in-memory object, then SaveToSlot.
 * Load flow: LoadFromSlot, then ReadSection for every module in the orchestration layer.
 *
 * The reader is the schema owner: it passes the version it understands and the writer's
 * version has to match exactly, because a mismatched payload still deserializes into
 * plausible-looking garbage rather than failing on its own.
 *
 * Custom migration: when a section's schema changes, write a new Version and migrate
 * by reading the raw bytes (ReadSectionBytes) with your own decoder, then writing the
 * new schema. The module intentionally does not run migration callbacks for you.
 */
UCLASS()
class UTSAVERUNTIME_API UUTSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// -- Typed C++ section API -------------------------------------------------

	/** Serialize any UStruct with UPROPERTY members into a section. */
	template <typename TStruct>
	FUTResult WriteSection(FName Section, int32 Version, const TStruct& Data)
	{
		return WriteSectionAny(Section, Version, FInstancedStruct::Make<TStruct>(Data));
	}

	/**
	 * Deserialize a section into a fresh (zero-initialized) struct.
	 *
	 * ExpectedVersion is the schema this code understands. A section saved with any other
	 * version is rejected rather than reinterpreted, because the bytes of an old schema can
	 * still deserialize "successfully" into garbage - that is how a silently corrupt save
	 * happens. Read the raw bytes with ReadSectionBytes when you need to migrate instead.
	 *
	 * Fails on: missing section, type mismatch, version mismatch, truncated payload.
	 */
	template <typename TStruct>
	FUTResult ReadSection(FName Section, int32 ExpectedVersion, TStruct& OutData)
	{
		FInstancedStruct Pending = FInstancedStruct::Make<TStruct>();
		const FUTResult Result = ReadSectionAny(Section, ExpectedVersion, Pending);
		if (Result.IsSuccess()) {
			OutData = Pending.Get<TStruct>();
		}
		return Result;
	}

	// -- Blueprint section API ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult WriteSectionAny(FName Section, int32 Version, const FInstancedStruct& Data);

	/**
	 * Blueprint counterpart of ReadSection: same exact-version contract. ExpectedVersion is
	 * required rather than inferred, so a Blueprint caller cannot accidentally read a slot
	 * written by an older or newer schema.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult ReadSectionAny(FName Section, int32 ExpectedVersion, FInstancedStruct& OutData);

	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult WriteSectionBytes(FName Section, int32 Version, const FString& StructPath, const TArray<uint8>& Bytes);

	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult ReadSectionBytes(FName Section, int32& OutVersion, TArray<uint8>& OutBytes) const;

	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	bool HasSection(FName Section) const;

	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult RemoveSection(FName Section);

	// -- Slot lifecycle ------------------------------------------------------------

	/** Flush the in-memory save object to disk. */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult SaveToSlot(FName SlotName);

	/**
	 * Transactional load: the slot is loaded and validated (format version, section
	 * integrity) into a temporary object; only a fully valid result replaces Current.
	 * A failed or corrupt load leaves the previously loaded state untouched.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult LoadFromSlot(FName SlotName);

	/**
	 * Engine-async slot write; OnFinished fires on the game thread with the outcome.
	 * Writes the same in-memory object as SaveToSlot.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	void SaveToSlotAsync(FName SlotName, FUTSaveCompletedSignature OnFinished);

	/**
	 * Engine-async transactional load; OnFinished fires on the game thread. Current is
	 * replaced only when the loaded slot passes validation - never on a failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	void LoadFromSlotAsync(FName SlotName, FUTSaveCompletedSignature OnFinished);

	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	FUTResult DeleteSlot(FName SlotName);

	UFUNCTION(BlueprintPure, Category = "UT|Save")
	bool SlotExists(FName SlotName) const;

	UFUNCTION(BlueprintPure, Category = "UT|Save")
	FDateTime GetSlotTimestamp(FName SlotName) const;

	/** All *.sav slot names found in the project's SaveGames directory. */
	UFUNCTION(BlueprintCallable, Category = "UT|Save")
	TArray<FName> GetSlotNames() const;

	UFUNCTION(BlueprintPure, Category = "UT|Save")
	bool IsSlotLoaded() const { return Current != nullptr; }

	UFUNCTION(BlueprintPure, Category = "UT|Save")
	FName GetLoadedSlotName() const { return Current ? Current->SlotName : FName(); }

private:
	/** Bump when the slot record layout itself changes; readers reject other versions wholesale. */
	static constexpr int32 CurrentSaveFormatVersion = 1;

	UPROPERTY(Transient)
	TObjectPtr<UUTSaveGame> Current;

	UUTSaveGame* EnsureCurrent();
	const FUTSaveSectionRecord* FindSection(FName Section) const;

	/** Whole-object gate used by the transactional load paths (sync and async). */
	FUTResult ValidateSaveGame(const UUTSaveGame* Candidate) const;

	void HandleAsyncSaveCompleted(const FString& SlotNameString, int32 UserIndex, bool bSuccess, FName SlotName, FUTSaveCompletedSignature OnFinished);
	void HandleAsyncLoadCompleted(const FString& SlotNameString, int32 UserIndex, USaveGame* LoadedGame, FName SlotName, FUTSaveCompletedSignature OnFinished);

	/**
	 * Shared section checks for both readers: existence, exact schema version, struct type
	 * and payload integrity (DataSize must match the stored byte count).
	 *
	 * ExpectedStruct may be null for byte sections, which skips the type check.
	 */
	FUTResult ValidateRecord(FName Section, int32 ExpectedVersion, const FUTSaveSectionRecord* Record, const UScriptStruct* ExpectedStruct) const;
};
