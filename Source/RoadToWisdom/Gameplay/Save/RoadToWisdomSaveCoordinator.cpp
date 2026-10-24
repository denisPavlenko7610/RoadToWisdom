#include "Gameplay/Save/RoadToWisdomSaveCoordinator.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RoadToWisdom.h"
#include "Gameplay/Player/RoadToWisdomExplorerController.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"
#include "Gameplay/Character/RoadToWisdomOnFootCharacter.h"
#include "Gameplay/Missions/RoadToWisdomMissionManager.h"
#include "Gameplay/Items/RoadToWisdomPlacedPickup.h"
#include "Gameplay/Missions/RoadToWisdomFuseBox.h"
#include "UTSaveSubsystem.h"
#include "UTWorldStateSubsystem.h"
#include "UTInventoryComponent.h"
#include "UTLever.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	constexpr int32 Version = 1;
	constexpr int32 PickupsVersion = 2;

	bool ValidPlacement(const FTransform& T)
	{
		return !T.ContainsNaN() && T.GetRotation().IsNormalized()
			&& T.GetScale3D().Equals(FVector::OneVector, 0.001f);
	}

	bool GatherPickups(UWorld* World, TMap<FName, ARoadToWisdomPlacedPickup*>& ById)
	{
		TArray<ARoadToWisdomPlacedPickup*> Pickups;
		ARoadToWisdomPlacedPickup::GetAllPlacedPickups(World, Pickups);
		for (auto* Pickup : Pickups) {
			if (Pickup->SaveStableId.IsNone() || ById.Contains(Pickup->SaveStableId)) {
				UE_LOG(LogRoadToWisdom, Error, TEXT("Save: missing/duplicate pickup id on %s."), *Pickup->GetName());
				return false;
			}
			ById.Add(Pickup->SaveStableId, Pickup);
		}
		return true;
	}
}

void URoadToWisdomSaveCoordinator::NotifyPickupCollected(FName StableId)
{
	if (!StableId.IsNone()) CollectedPickupIds.AddUnique(StableId);
}

void URoadToWisdomSaveCoordinator::DestroyCollectedPickups() const
{
	TArray<ARoadToWisdomPlacedPickup*> Pickups;
	ARoadToWisdomPlacedPickup::GetAllPlacedPickups(GetGameInstance()->GetWorld(), Pickups);
	for (auto* Pickup : Pickups) {
		if (CollectedPickupIds.Contains(Pickup->SaveStableId)) Pickup->Destroy();
	}
}

bool URoadToWisdomSaveCoordinator::SaveSession(ARoadToWisdomMissionManager* Mission)
{
	UWorld* World = GetGameInstance()->GetWorld();
	auto* Controller = Cast<ARoadToWisdomExplorerController>(UGameplayStatics::GetPlayerController(World, 0));
	auto* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	if (!World || !Mission || !Controller || !Save || !CaptureAndWriteSections(Mission, Controller)) {
		UE_LOG(LogRoadToWisdom, Warning, TEXT("SaveSession: required state unavailable/invalid; slot not written."));
		return false;
	}
	// UTSave delegates to SaveGameToSlot. Its transactional LOAD protects Current;
	// neither section writes nor actor replay promise power-loss-safe disk writes.
	const FUTResult Result = Save->SaveToSlot(GetSessionSlotName());
	UE_LOG(LogRoadToWisdom, Log, TEXT("SaveSession: success=%d %s"), Result.IsSuccess(), *Result.Message.ToString());
	return Result.IsSuccess();
}

bool URoadToWisdomSaveCoordinator::CaptureAndWriteSections(
	ARoadToWisdomMissionManager* Mission, ARoadToWisdomExplorerController* Controller
)
{
	UWorld* World = GetGameInstance()->GetWorld();
	auto* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	auto* WorldState = World->GetSubsystem<UUTWorldStateSubsystem>();
	if (!WorldState || !Mission->FuseBox || !Mission->Lever || !Controller->VanPawn
		|| !Controller->OnFootCharacter || !Controller->OnFootCharacter->GetInventory()
		|| !Controller->GetPawn() || Controller->bHasPendingRestore
		|| Controller->VanPawn->GetVelocity().Size() > Controller->ExitMaxSpeed)
		return false;
	TMap<FName, ARoadToWisdomPlacedPickup*> ById;
	if (!GatherPickups(World, ById)) return false;
	FQRPickupsSection Pickups;
	Pickups.CollectedIds = CollectedPickupIds;
	for (const auto& Pair : ById) {
		if (CollectedPickupIds.Contains(Pair.Key) || !Pair.Value->GetItem().IsValid()) return false;
		auto& Entry = Pickups.Remaining.AddDefaulted_GetRef();
		Entry.StableId = Pair.Key;
		Entry.Item = Pair.Value->GetItem();
	}
	FUTWorldStateSnapshot Snapshot;
	if (!WorldState->CaptureWorldState(Snapshot).IsSuccess() || Snapshot.Entries.IsEmpty()) return false;
	FQRMissionSection MissionSection;
	MissionSection.Stage = static_cast<int32>(Mission->GetStage());
	MissionSection.bLeverOn = Mission->Lever->bIsOn;
	FQRVehicleSection Vehicle;
	Vehicle.Transform = Controller->VanPawn->GetActorTransform();
	Vehicle.bValid = ValidPlacement(Vehicle.Transform);
	FQRPlayerSection Player;
	Player.Transform = Controller->GetPawn()->GetActorTransform();
	Player.bOnFoot = !Controller->IsDriving();
	Player.bValid = ValidPlacement(Player.Transform);
	if (!Vehicle.bValid || !Player.bValid) return false;
	// Capture all mandatory components first. Every save overwrites all six sections.
	return Save->WriteSection(TEXT("Mission"), Version, MissionSection).IsSuccess()
		&& Save->WriteSection(TEXT("Inventory"), Version, Controller->OnFootCharacter->GetInventory()->MakeSnapshot()).IsSuccess()
		&& Save->WriteSection(TEXT("World"), Version, Snapshot).IsSuccess()
		&& Save->WriteSection(TEXT("Pickups"), PickupsVersion, Pickups).IsSuccess()
		&& Save->WriteSection(TEXT("Vehicle"), Version, Vehicle).IsSuccess()
		&& Save->WriteSection(TEXT("Player"), Version, Player).IsSuccess();
}

bool URoadToWisdomSaveCoordinator::TryRestoreSession(ARoadToWisdomExplorerController* Controller)
{
	// GameInstance survives map travel: never retain the previous session's history.
	CollectedPickupIds.Reset();
	if (Controller) {
		Controller->bHasPendingRestore = false;
		Controller->bHasPendingInventory = false;
		Controller->PendingInventory = FUTInventorySnapshot();
	}
	auto* Save = GetGameInstance()->GetSubsystem<UUTSaveSubsystem>();
	UWorld* World = GetGameInstance()->GetWorld();
	if (!Save || !World || !Controller || !Controller->VanPawn) return false;
	if (!Save->LoadFromSlot(GetSessionSlotName()).IsSuccess()) return false;
	FQRMissionSection MissionSection;
	FQRVehicleSection Vehicle;
	FQRPlayerSection Player;
	FQRPickupsSection Pickups;
	FUTInventorySnapshot Inventory;
	FUTWorldStateSnapshot Snapshot;
	// Read all data before applying ANY gameplay state.
	if (!Save->ReadSection(TEXT("Mission"), Version, MissionSection).IsSuccess()
		|| !Save->ReadSection(TEXT("Vehicle"), Version, Vehicle).IsSuccess()
		|| !Save->ReadSection(TEXT("Player"), Version, Player).IsSuccess()
		|| !Save->ReadSection(TEXT("Inventory"), Version, Inventory).IsSuccess()
		|| !Save->ReadSection(TEXT("World"), Version, Snapshot).IsSuccess()
		|| !Save->ReadSection(TEXT("Pickups"), PickupsVersion, Pickups).IsSuccess()) {
		UE_LOG(
			LogRoadToWisdom, Warning,
			TEXT("Restore rejected: missing/corrupt/obsolete section (Pickups requires v2). World unchanged.")
		);
		return false;
	}
	auto* WorldState = World->GetSubsystem<UUTWorldStateSubsystem>();
	TArray<AActor*> Managers;
	UGameplayStatics::GetAllActorsOfClass(World, ARoadToWisdomMissionManager::StaticClass(), Managers);
	if (!WorldState || Managers.Num() != 1 || !Vehicle.bValid || !Player.bValid
		|| !ValidPlacement(Vehicle.Transform) || !ValidPlacement(Player.Transform)
		|| MissionSection.Stage < 0 || MissionSection.Stage > static_cast<int32>(EQRRelayMissionStage::Completed))
		return false;
	auto* Mission = CastChecked<ARoadToWisdomMissionManager>(Managers[0]);
	if (!Mission->FuseBox || !Mission->Lever) return false;
	// This slice owns exactly one saveable type. UT ApplyWorldState is sequential,
	// so validate its identity, schema and stage consistency before invoking it.
	FUTWorldStateSnapshot Current;
	if (!WorldState->CaptureWorldState(Current).IsSuccess() || Current.Entries.Num() != 1 || Snapshot.Entries.Num() != 1)
		return
			false;
	const auto& Entry = Snapshot.Entries[0];
	const auto* Fuse = Entry.State.GetPtr<FQRFuseBoxState>();
	if (!Fuse || Entry.StableId != Mission->FuseBox->GetSaveStableId_Implementation()
		|| Entry.StableId != Current.Entries[0].StableId)
		return false;
	if (Fuse->bFuseInstalled != (MissionSection.Stage >= static_cast<int32>(EQRRelayMissionStage::PullLever))
		|| MissionSection.bLeverOn != (MissionSection.Stage == static_cast<int32>(EQRRelayMissionStage::Completed)))
		return false;
	const auto* Defaults = GetDefault<ARoadToWisdomOnFootCharacter>()->GetInventory();
	if (!Defaults || (Defaults->MaxSlots > 0 && Inventory.Stacks.Num() + Inventory.Instances.Num() > Defaults->MaxSlots))
		return
			false;
	TSet<FGuid> InstanceIds;
	int64 FuseCount = 0;
	for (const auto& Stack : Inventory.Stacks) {
		if (Stack.ItemId.IsNone() || Stack.Count <= 0) return false;
		if (Stack.ItemId == Mission->FuseBox->FuseItemId) FuseCount += Stack.Count;
	}
	for (const auto& Instance : Inventory.Instances) {
		if (!Instance.IsValid() || InstanceIds.Contains(Instance.InstanceId)) return false;
		InstanceIds.Add(Instance.InstanceId);
	}
	TMap<FName, ARoadToWisdomPlacedPickup*> ById;
	if (!GatherPickups(World, ById)) return false;
	TSet<FName> Seen;
	for (FName Id : Pickups.CollectedIds) {
		if (Id.IsNone() || Seen.Contains(Id) || !ById.Contains(Id)) return false;
		Seen.Add(Id);
	}
	for (const auto& Remaining : Pickups.Remaining) {
		auto** Target = ById.Find(Remaining.StableId);
		if (!Target || Seen.Contains(Remaining.StableId) || !Remaining.Item.IsValid()
			|| Remaining.Item.GetItemId() != (*Target)->InitialItem.GetItemId()
			|| Remaining.Item.bUnique != (*Target)->InitialItem.bUnique
			|| Remaining.Item.GetCount() > (*Target)->InitialItem.GetCount())
			return false;
		Seen.Add(Remaining.StableId);
		if (Remaining.Item.GetItemId() == Mission->FuseBox->FuseItemId) FuseCount += Remaining.Item.GetCount();
	}
	if (Seen.Num() != ById.Num() || FuseCount + (Fuse->bFuseInstalled ? 1 : 0) != 1) return false;
	// AUTWorldItem refuses payload reassignment. For partial stacks, prepare a fresh
	// authored actor through the supported initialization path, retaining originals
	// until every spawn succeeds. No item-transfer logic is copied from Toolkit.
	TArray<ARoadToWisdomPlacedPickup*> Prepared, Replaced;
	for (const auto& Remaining : Pickups.Remaining) {
		auto* Original = ById[Remaining.StableId];
		if (!Remaining.Item.bUnique && Remaining.Item.GetCount() == Original->GetItem().GetCount()) continue;
		FActorSpawnParameters Params;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Replacement = World->SpawnActor<ARoadToWisdomPlacedPickup>(
			Original->GetClass(), Original->GetActorTransform(), Params
		);
		if (!Replacement || !Replacement->InitializeItem(Remaining.Item)) {
			if (Replacement) Replacement->Destroy();
			for (auto* Pending : Prepared) Pending->Destroy();
			return false;
		}
		Replacement->SaveStableId = Original->SaveStableId;
		Replacement->InitialItem = Original->InitialItem;
		Replacement->PickupDistance = Original->PickupDistance;
		Replacement->Mesh->SetStaticMesh(Original->Mesh->GetStaticMesh());
		Replacement->Mesh->SetRelativeScale3D(Original->Mesh->GetRelativeScale3D());
		for (int32 I = 0; I < Original->Mesh->GetNumMaterials(); ++I)
			Replacement->Mesh->SetMaterial(
				I, Original->Mesh->GetMaterial(I)
			);
		Prepared.Add(Replacement);
		Replaced.Add(Original);
	}
	FUTResult ApplyResult;
	const int32 Applied = WorldState->ApplyWorldState(Snapshot, ApplyResult);
	if (!ApplyResult.IsSuccess() || Applied != Snapshot.Entries.Num()) {
		for (auto* Pending : Prepared) Pending->Destroy();
		UE_LOG(LogRoadToWisdom, Error, TEXT("WorldState rejected (%d entries): %s"), Applied, *ApplyResult.Message.ToString());
		return false;
	}
	for (int32 I = 0; I < Prepared.Num(); ++I) {
		const FTransform Transform = Replaced[I]->GetActorTransform();
		Replaced[I]->Destroy();
		Prepared[I]->FinishSpawning(Transform);
	}
	CollectedPickupIds = Pickups.CollectedIds;
	DestroyCollectedPickups();
	Mission->ApplyState(static_cast<EQRRelayMissionStage>(MissionSection.Stage), MissionSection.bLeverOn);
	Controller->VanPawn->SetActorLocationAndRotation(
		Vehicle.Transform.GetLocation(), Vehicle.Transform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics
	);
	Controller->VanPawn->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Controller->VanPawn->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Controller->VanPawn->DoThrottle(0.f);
	Controller->VanPawn->DoHandbrakeStart();
	Controller->PendingPlayerSection = Player;
	Controller->PendingInventory = Inventory;
	Controller->bHasPendingInventory = true;
	Controller->bHasPendingRestore = true;
	UE_LOG(LogRoadToWisdom, Log, TEXT("RestoreSession: validated six sections, applied %d world entries."), Applied);
	return true;
}

FName URoadToWisdomSaveCoordinator::GetSessionSlotName()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (FParse::Param(FCommandLine::Get(), TEXT("QuietRoadAutomation")))
        return TEXT("QuietRoad_Automation");
#endif
    return FName(SlotName);
}
