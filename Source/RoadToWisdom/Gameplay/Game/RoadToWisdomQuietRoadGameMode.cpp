// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/Game/RoadToWisdomQuietRoadGameMode.h"

#include "Gameplay/Player/RoadToWisdomExplorerController.h"
#include "Gameplay/Character/RoadToWisdomOnFootCharacter.h"

ARoadToWisdomQuietRoadGameMode::ARoadToWisdomQuietRoadGameMode()
{
	PlayerControllerClass = ARoadToWisdomExplorerController::StaticClass();
	DefaultPawnClass = ARoadToWisdomOnFootCharacter::StaticClass();
}
