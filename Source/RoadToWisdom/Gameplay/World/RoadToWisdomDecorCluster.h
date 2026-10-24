// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadToWisdomDecorCluster.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 *  Editor-time decor carrier for the M2 visual reference (GDD §24): one actor holding
 *  HISM components for mass vegetation/rocks so a tree never becomes its own actor.
 *
 *  Meshes and instance transforms are filled by the editor generator script
 *  (Tools/Editor/GenerateM2Decor.py) and saved into the map; at runtime this actor is
 *  pure static geometry. Decor has no stable ids and is never part of a save.
 */
UCLASS()
class ARoadToWisdomDecorCluster : public AActor
{
	GENERATED_BODY()

public:
	ARoadToWisdomDecorCluster();

	/** Replace every instance of one slot (0 trunks, 1 crowns, 2 bushes, 3 rocks). */
	UFUNCTION(BlueprintCallable, Category = "QuietRoad|Decor")
	void SetInstances(int32 Slot, const TArray<FTransform>& Instances);

protected:
	UPROPERTY(VisibleAnywhere, Category = "QuietRoad")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeTrunks;

	UPROPERTY(VisibleAnywhere, Category = "QuietRoad")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeCrowns;

	UPROPERTY(VisibleAnywhere, Category = "QuietRoad")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Bushes;

	UPROPERTY(VisibleAnywhere, Category = "QuietRoad")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Rocks;
};
