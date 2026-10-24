// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleTemplate/Wheels/RoadToWisdomWheelFront.h"
#include "UObject/ConstructorHelpers.h"

URoadToWisdomWheelFront::URoadToWisdomWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}