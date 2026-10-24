// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadToWisdomGameMode.h"
#include "RoadToWisdomQuietRoadGameMode.generated.h"

/**
 *  Quiet Road game mode: the player starts on foot; the van is a placed actor in
 *  the map (GDD 7.2 — the vehicle is a persistent world object, not a spawned
 *  default pawn). Template maps keep using BP_VehicleAdvGameMode, so this mode is
 *  bound per-map (WorldSettings DefaultGameMode) instead of globally.
 */
UCLASS()
class ARoadToWisdomQuietRoadGameMode : public ARoadToWisdomGameMode
{
	GENERATED_BODY()

public:
	ARoadToWisdomQuietRoadGameMode();
};
