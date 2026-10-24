// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomQuietRoadGameMode.h"

#include "RoadToWisdomExplorerController.h"
#include "Character/RoadToWisdomOnFootCharacter.h"

ARoadToWisdomQuietRoadGameMode::ARoadToWisdomQuietRoadGameMode()
{
	PlayerControllerClass = ARoadToWisdomExplorerController::StaticClass();
	DefaultPawnClass = ARoadToWisdomOnFootCharacter::StaticClass();
}
