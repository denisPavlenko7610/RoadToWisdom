// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomGameMode.h"
#include "RoadToWisdomPlayerController.h"

ARoadToWisdomGameMode::ARoadToWisdomGameMode()
{
	PlayerControllerClass = ARoadToWisdomPlayerController::StaticClass();
}
