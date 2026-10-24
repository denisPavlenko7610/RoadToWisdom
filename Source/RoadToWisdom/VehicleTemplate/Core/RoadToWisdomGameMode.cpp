// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleTemplate/Core/RoadToWisdomGameMode.h"
#include "VehicleTemplate/Core/RoadToWisdomPlayerController.h"

ARoadToWisdomGameMode::ARoadToWisdomGameMode()
{
	PlayerControllerClass = ARoadToWisdomPlayerController::StaticClass();
}
