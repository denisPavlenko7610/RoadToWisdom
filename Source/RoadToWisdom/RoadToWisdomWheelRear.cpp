// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoadToWisdomWheelRear.h"
#include "UObject/ConstructorHelpers.h"

URoadToWisdomWheelRear::URoadToWisdomWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}