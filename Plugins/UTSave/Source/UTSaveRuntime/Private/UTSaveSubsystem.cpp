// UT Templates - save subsystem implementation.
#include "UTSaveSubsystem.h"

#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

FUTResult UUTSaveSubsystem::WriteSectionAny(FName Section, int32 Version, const FInstancedStruct& Data)
{
	UScriptStruct* Struct = const_cast<UScriptStruct*>(Data.GetScriptStruct());
	if (Section.IsNone() || !Struct) {
		return FUTResult::Failure(TEXT("InvalidSection"),
								  FText::FromString(TEXT("WriteSectionAny requires a section name and a filled instanced struct.")));
	}

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, /*bIsPersistent*/ true);
	FObjectAndNameAsStringProxyArchive Archive(Writer, false);
	Struct->SerializeItem(Archive, const_cast<uint8*>(Data.GetMemory()), nullptr);
	if (Archive.IsError()) {
		return FUTResult::Failure("SerializeFailed", FText::FromString("Section serialization failed."));
	}
	return WriteSectionBytes(Section, Version, Struct->GetPathName(), Bytes);
}

FUTResult UUTSaveSubsystem::ReadSectionAny(FName Section, int32 ExpectedVersion, FInstancedStruct& OutData)
{
	UScriptStruct* Struct = const_cast<UScriptStruct*>(OutData.GetScriptStruct());
	if (!Struct) {
		return FUTResult::Failure(TEXT("StructMismatch"),
								  FText::FromString(TEXT("OutData has no struct type; initialize it (e.g. Make FInstancedStruct) before reading.")));
	}

	const FUTSaveSectionRecord* Record = FindSection(Section);
	const FUTResult Check = ValidateRecord(Section, ExpectedVersion, Record, Struct);
	if (!Check.IsSuccess()) {
		return Check;
	}

	FMemoryReader Reader(Record->Bytes, /*bIsPersistent*/ true);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true);
	FInstancedStruct Pending;
	Pending.InitializeAs(Struct);
	Struct->SerializeItem(Archive, Pending.GetMutableMemory(), nullptr);
	if (Archive.IsError()) {
		return FUTResult::Failure("CorruptSection", FText::FromString("Section deserialization failed."));
	}
	OutData = MoveTemp(Pending);
	return FUTResult::Success();
}

FUTResult UUTSaveSubsystem::ValidateRecord(FName Section, int32 ExpectedVersion, const FUTSaveSectionRecord* Record, const UScriptStruct* ExpectedStruct) const
{
	if (!Record) {
		return FUTResult::Failure(TEXT("SectionNotFound"),
								  FText::FromString(FString::Printf(TEXT("Section '%s' is not in the loaded slot."), *Section.ToString())));
	}

	// Exact match, not ">=": old bytes deserialize into a valid-looking struct without
	// any error, so an outdated section has to be refused instead of read.
	if (Record->Version != ExpectedVersion) {
		return FUTResult::Failure(TEXT("VersionMismatch"),
								  FText::FromString(FString::Printf(TEXT("Section '%s' has schema version %d, this reader expects %d. Bump the version and migrate via ReadSectionBytes."),
																	*Section.ToString(), Record->Version, ExpectedVersion)));
	}

	if (ExpectedStruct && !Record->StructPath.IsNull() && Record->StructPath != FSoftObjectPath(ExpectedStruct)) {
		return FUTResult::Failure(TEXT("StructMismatch"),
								  FText::FromString(FString::Printf(TEXT("Section '%s' was written as '%s', read as '%s'."),
																	*Section.ToString(), *Record->StructPath.ToString(), *ExpectedStruct->GetPathName())));
	}

	// Integrity only. DataSize is the serialized byte count, so comparing it with the
	// in-memory struct size would reject every valid save - the two numbers are unrelated
	// for TArray, FString and padding. It only proves the payload arrived intact.
	if (Record->DataSize != Record->Bytes.Num()) {
		return FUTResult::Failure(TEXT("TruncatedSection"),
								  FText::FromString(FString::Printf(TEXT("Section '%s' is corrupt (recorded %d bytes, holds %d)."),
																	*Section.ToString(), Record->DataSize, Record->Bytes.Num())));
	}

	return FUTResult::Success();
}

FUTResult UUTSaveSubsystem::WriteSectionBytes(FName Section, int32 Version, const FString& StructPath, const TArray<uint8>& Bytes)
{
	UUTSaveGame* SaveObject = EnsureCurrent();
	if (!SaveObject) {
		return FUTResult::Failure(TEXT("InternalError"), FText::FromString(TEXT("Could not create the save object.")));
	}

	FUTSaveSectionRecord Record;
	Record.Section = Section;
	Record.Version = Version;
	Record.StructPath = FSoftObjectPath(StructPath);
	Record.DataSize = Bytes.Num();
	Record.Bytes = Bytes;
	SaveObject->Sections.Add(Section, MoveTemp(Record));
	return FUTResult::Success();
}

FUTResult UUTSaveSubsystem::ReadSectionBytes(FName Section, int32& OutVersion, TArray<uint8>& OutBytes) const
{
	const FUTSaveSectionRecord* Record = FindSection(Section);
	if (!Record) {
		return FUTResult::Failure(TEXT("SectionNotFound"),
								  FText::FromString(FString::Printf(TEXT("Section '%s' is not in the loaded slot."), *Section.ToString())));
	}

	OutVersion = Record->Version;
	OutBytes = Record->Bytes;
	return FUTResult::Success();
}

bool UUTSaveSubsystem::HasSection(FName Section) const
{
	return FindSection(Section) != nullptr;
}

FUTResult UUTSaveSubsystem::RemoveSection(FName Section)
{
	UUTSaveGame* SaveObject = EnsureCurrent();
	if (!SaveObject || !SaveObject->Sections.Remove(Section)) {
		return FUTResult::Failure(TEXT("SectionNotFound"),
								  FText::FromString(FString::Printf(TEXT("Section '%s' is not in the loaded slot."), *Section.ToString())));
	}
	return FUTResult::Success();
}

FUTResult UUTSaveSubsystem::SaveToSlot(FName SlotName)
{
	UUTSaveGame* SaveObject = EnsureCurrent();
	if (SlotName.IsNone() || !SaveObject) {
		return FUTResult::Failure(TEXT("InvalidSlot"), FText::FromString(TEXT("SaveToSlot requires a slot name.")));
	}

	SaveObject->SlotName = SlotName;
	SaveObject->Timestamp = FDateTime::UtcNow();
	if (!UGameplayStatics::SaveGameToSlot(SaveObject, SlotName.ToString(), /*UserIndex*/ 0)) {
		return FUTResult::Failure(TEXT("WriteFailed"),
								  FText::FromString(FString::Printf(TEXT("Could not write slot '%s' to disk."), *SlotName.ToString())));
	}
	return FUTResult::Success();
}

FUTResult UUTSaveSubsystem::ValidateSaveGame(const UUTSaveGame* Candidate) const
{
	if (!Candidate) {
		return FUTResult::Failure(TEXT("ReadFailed"), FText::FromString(TEXT("Slot is not a UT save.")));
	}

	// Whole-save contract: a slot written by another record layout is refused wholesale
	// instead of being half-read into plausible-looking garbage.
	if (Candidate->FormatVersion != CurrentSaveFormatVersion) {
		return FUTResult::Failure(TEXT("FormatVersionMismatch"),
								  FText::FromString(FString::Printf(TEXT("Slot format version %d, this build expects %d."),
																	Candidate->FormatVersion, CurrentSaveFormatVersion)));
	}

	for (const TPair<FName, FUTSaveSectionRecord>& Pair : Candidate->Sections) {
		if (Pair.Value.DataSize != Pair.Value.Bytes.Num()) {
			return FUTResult::Failure(TEXT("TruncatedSection"),
									  FText::FromString(FString::Printf(TEXT("Section '%s' is corrupt (recorded %d bytes, holds %d)."),
																		*Pair.Key.ToString(), Pair.Value.DataSize, Pair.Value.Bytes.Num())));
		}
	}
	return FUTResult::Success();
}

FUTResult UUTSaveSubsystem::LoadFromSlot(FName SlotName)
{
	if (SlotName.IsNone()) {
		return FUTResult::Failure(TEXT("InvalidSlot"), FText::FromString(TEXT("LoadFromSlot requires a slot name.")));
	}

	if (!SlotExists(SlotName)) {
		return FUTResult::Failure(TEXT("SlotNotFound"),
								  FText::FromString(FString::Printf(TEXT("Slot '%s' does not exist."), *SlotName.ToString())));
	}

	// Transactional: load and validate a temporary object; Current is replaced only
	// after full validation, so a corrupt or foreign slot never destroys live state.
	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName.ToString(), /*UserIndex*/ 0);
	UUTSaveGame* SaveObject = Cast<UUTSaveGame>(Loaded);
	const FUTResult Check = ValidateSaveGame(SaveObject);
	if (!Check.IsSuccess()) {
		return FUTResult::Failure(Check.Code,
								  FText::FromString(FString::Printf(TEXT("Slot '%s' rejected: %s"), *SlotName.ToString(), *Check.Message.ToString())));
	}

	Current = SaveObject;
	return FUTResult::Success();
}

void UUTSaveSubsystem::SaveToSlotAsync(FName SlotName, FUTSaveCompletedSignature OnFinished)
{
	UUTSaveGame* SaveObject = EnsureCurrent();
	if (SlotName.IsNone() || !SaveObject) {
		OnFinished.ExecuteIfBound(FUTResult::Failure(TEXT("InvalidSlot"), FText::FromString(TEXT("SaveToSlotAsync requires a slot name."))), SlotName);
		return;
	}

	SaveObject->SlotName = SlotName;
	SaveObject->Timestamp = FDateTime::UtcNow();

	FAsyncSaveGameToSlotDelegate Delegate = FAsyncSaveGameToSlotDelegate::CreateUObject(
		this, &UUTSaveSubsystem::HandleAsyncSaveCompleted, SlotName, OnFinished);
	UGameplayStatics::AsyncSaveGameToSlot(SaveObject, SlotName.ToString(), /*UserIndex*/ 0, Delegate);
}

void UUTSaveSubsystem::HandleAsyncSaveCompleted(const FString& SlotNameString, int32 UserIndex, bool bSuccess, FName SlotName, FUTSaveCompletedSignature OnFinished)
{
	FUTResult Result = bSuccess
						   ? FUTResult::Success()
						   : FUTResult::Failure(TEXT("WriteFailed"),
												FText::FromString(FString::Printf(TEXT("Could not write slot '%s' to disk."), *SlotNameString)));
	OnFinished.ExecuteIfBound(Result, SlotName);
}

void UUTSaveSubsystem::LoadFromSlotAsync(FName SlotName, FUTSaveCompletedSignature OnFinished)
{
	if (SlotName.IsNone()) {
		OnFinished.ExecuteIfBound(FUTResult::Failure(TEXT("InvalidSlot"), FText::FromString(TEXT("LoadFromSlotAsync requires a slot name."))), SlotName);
		return;
	}

	FAsyncLoadGameFromSlotDelegate Delegate = FAsyncLoadGameFromSlotDelegate::CreateUObject(
		this, &UUTSaveSubsystem::HandleAsyncLoadCompleted, SlotName, OnFinished);
	UGameplayStatics::AsyncLoadGameFromSlot(SlotName.ToString(), /*UserIndex*/ 0, Delegate);
}

void UUTSaveSubsystem::HandleAsyncLoadCompleted(const FString& SlotNameString, int32 UserIndex, USaveGame* LoadedGame, FName SlotName, FUTSaveCompletedSignature OnFinished)
{
	if (!LoadedGame) {
		OnFinished.ExecuteIfBound(FUTResult::Failure(TEXT("SlotNotFound"),
													 FText::FromString(FString::Printf(TEXT("Slot '%s' could not be read."), *SlotNameString))),
								  SlotName);
		return;
	}

	// Same transactional gate as the sync path: replace Current only on full validation.
	UUTSaveGame* SaveObject = Cast<UUTSaveGame>(LoadedGame);
	const FUTResult Check = ValidateSaveGame(SaveObject);
	if (!Check.IsSuccess()) {
		OnFinished.ExecuteIfBound(FUTResult::Failure(Check.Code,
													 FText::FromString(FString::Printf(TEXT("Slot '%s' rejected: %s"), *SlotNameString, *Check.Message.ToString()))),
								  SlotName);
		return;
	}

	Current = SaveObject;
	OnFinished.ExecuteIfBound(FUTResult::Success(), SlotName);
}

FUTResult UUTSaveSubsystem::DeleteSlot(FName SlotName)
{
	if (SlotName.IsNone() || !SlotExists(SlotName)) {
		return FUTResult::Failure(TEXT("SlotNotFound"),
								  FText::FromString(FString::Printf(TEXT("Slot '%s' does not exist."), *SlotName.ToString())));
	}
	if (!UGameplayStatics::DeleteGameInSlot(SlotName.ToString(), /*UserIndex*/ 0)) {
		return FUTResult::Failure(TEXT("DeleteFailed"),
								  FText::FromString(FString::Printf(TEXT("Could not delete slot '%s'."), *SlotName.ToString())));
	}

	// A deleted slot must not stay usable in memory: keeping Current would let the removed
	// data be read and then written straight back to disk.
	if (Current && Current->SlotName == SlotName) {
		Current = nullptr;
	}
	return FUTResult::Success();
}

bool UUTSaveSubsystem::SlotExists(FName SlotName) const
{
	return !SlotName.IsNone() && UGameplayStatics::DoesSaveGameExist(SlotName.ToString(), /*UserIndex*/ 0);
}

FDateTime UUTSaveSubsystem::GetSlotTimestamp(FName SlotName) const
{
	if (!SlotExists(SlotName)) {
		return FDateTime();
	}
	const USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName.ToString(), /*UserIndex*/ 0);
	const UUTSaveGame* SaveObject = Cast<UUTSaveGame>(Loaded);
	return SaveObject ? SaveObject->Timestamp : FDateTime();
}

TArray<FName> UUTSaveSubsystem::GetSlotNames() const
{
	TArray<FName> SlotNames;

	const FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
	if (!IFileManager::Get().DirectoryExists(*SaveDirectory)) {
		return SlotNames;
	}

	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(SaveDirectory / TEXT("*.sav")), /*Files*/ true, /*Directories*/ false);
	for (const FString& FileName : FileNames) {
		SlotNames.Add(FName(*FPaths::GetBaseFilename(FileName)));
	}
	return SlotNames;
}

UUTSaveGame* UUTSaveSubsystem::EnsureCurrent()
{
	if (!Current) {
		Current = NewObject<UUTSaveGame>(GetOuter());
	}
	return Current;
}

const FUTSaveSectionRecord* UUTSaveSubsystem::FindSection(FName Section) const
{
	return Current ? Current->Sections.Find(Section) : nullptr;
}
