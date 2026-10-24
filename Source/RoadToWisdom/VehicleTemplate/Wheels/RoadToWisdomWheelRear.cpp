// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleTemplate/Wheels/RoadToWisdomWheelRear.h"
#include "UObject/ConstructorHelpers.h"

URoadToWisdomWheelRear::URoadToWisdomWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}