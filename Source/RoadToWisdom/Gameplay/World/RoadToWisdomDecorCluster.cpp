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
	switch (Slot)
	{
	case 0: Target = TreeTrunks; break;
	case 1: Target = TreeCrowns; break;
	case 2: Target = Bushes; break;
	case 3: Target = Rocks; break;
	default:
		UE_LOG(LogRoadToWisdom, Warning, TEXT("DecorCluster: unknown slot %d"), Slot);
		return;
	}

	// Editor-time changes from scripts are only serialized when marked transactional.
	// AddInstance feeds the transient render manager, which syncs PerInstanceSMData on
	// a later editor tick; a headless script saves before that tick runs, so write the
	// bulk-serialized array directly (UE 5.8: InstancedStaticMesh.cpp Serialize()).
	Target->Modify();
	Target->PerInstanceSMData.Reset(Instances.Num());
	for (const FTransform& Transform : Instances)
	{
		Target->PerInstanceSMData.Emplace(Transform.ToMatrixWithScale());
	}
	Target->MarkRenderStateDirty();
	UE_LOG(LogRoadToWisdom, Log, TEXT("DecorCluster slot %d: %d instances written"), Slot, Target->GetNumInstances());
}
