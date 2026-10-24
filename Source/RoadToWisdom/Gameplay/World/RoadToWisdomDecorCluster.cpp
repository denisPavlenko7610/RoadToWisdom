// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomDecorCluster.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "RoadToWisdom.h"

ARoadToWisdomDecorCluster::ARoadToWisdomDecorCluster()
{
	PrimaryActorTick.bCanEverTick = false;

	TreeTrunks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TreeTrunks"));
	SetRootComponent(TreeTrunks);
	// Script-placed instances must serialize into the placed actor, not inherit from the CDO.
	TreeTrunks->bEditableWhenInherited = true;

	TreeCrowns = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("TreeCrowns"));
	TreeCrowns->SetupAttachment(TreeTrunks);
	TreeCrowns->bEditableWhenInherited = true;
	TreeCrowns->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Bushes never block movement; trunks/crowns/rocks keep the static-mesh default.
	Bushes = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Bushes"));
	Bushes->SetupAttachment(TreeTrunks);
	Bushes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bushes->bEditableWhenInherited = true;

	Rocks = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Rocks"));
	Rocks->SetupAttachment(TreeTrunks);
	Rocks->bEditableWhenInherited = true;
}

void ARoadToWisdomDecorCluster::SetInstances(int32 Slot, const TArray<FTransform>& Instances)
{
	UHierarchicalInstancedStaticMeshComponent* Target = nullptr;
	switch (Slot) {
		case 0: Target = TreeTrunks;
			break;
		case 1: Target = TreeCrowns;
			break;
		case 2: Target = Bushes;
			break;
		case 3: Target = Rocks;
			break;
		default:
			UE_LOG(LogRoadToWisdom, Warning, TEXT("DecorCluster: unknown slot %d"), Slot);
			return;
	}

	// Official API only: both AddInstance and the batch AddInstances write straight
	// into the bulk-serialized PerInstanceSMData (UE 5.8 InstancedStaticMesh.cpp),
	// verified by a reload check after Save Level. The earlier direct-array write is
	// unnecessary; the actual persistence bug was scripts editing a stale current
	// world (now fixed by load_level in the generator).
	Target->Modify();
	Target->ClearInstances();
	for (const FTransform& Transform : Instances) {
		Target->AddInstance(Transform, true);
	}
	UE_LOG(LogRoadToWisdom, Log, TEXT("DecorCluster slot %d: %d instances written"), Slot, Target->GetNumInstances());
}
