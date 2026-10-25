#include "Gameplay/Vehicle/RoadToWisdomUtilityVan.h"
#include "VehicleTemplate/OffroadCar/RoadToWisdomOffroadWheelFront.h"
#include "VehicleTemplate/OffroadCar/RoadToWisdomOffroadWheelRear.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

ARoadToWisdomUtilityVan::ARoadToWisdomUtilityVan()
{
    GetBackSpringArm()->TargetArmLength=850.f;
    GetBackSpringArm()->bDoCollisionTest=true;
    GetBackSpringArm()->SetRelativeLocation(FVector(0,0,110));
    GetFrontSpringArm()->SetRelativeLocation(FVector(195,-45,145));
    UChaosWheeledVehicleMovementComponent* Movement=GetChaosVehicleMovement();
    Movement->ChassisHeight=190.f;
    Movement->ChassisWidth=210.f;
    Movement->Mass=1850.f;
    Movement->DragCoefficient=.35f;
    Movement->DownforceCoefficient=.1f;
    Movement->CenterOfMassOverride=FVector(0,0,75);
    Movement->bEnableCenterOfMassOverride=true;
    Movement->bLegacyWheelFrictionPosition=false;
    Movement->WheelSetups.SetNum(4);
    const FName Bones[]={TEXT("Wheel_FL"),TEXT("Wheel_FR"),TEXT("Wheel_BL"),TEXT("Wheel_BR")};
    for (int32 I=0;I<4;++I)
    {
        Movement->WheelSetups[I].BoneName=Bones[I];
        Movement->WheelSetups[I].WheelClass=I<2?URoadToWisdomOffroadWheelFront::StaticClass():URoadToWisdomOffroadWheelRear::StaticClass();
    }
    Movement->EngineSetup.MaxTorque=450.f;
    Movement->EngineSetup.MaxRPM=4500.f;
    Movement->EngineSetup.EngineIdleRPM=900.f;
    Movement->EngineSetup.EngineBrakeEffect=.08f;
    Movement->DifferentialSetup.DifferentialType=EVehicleDifferential::AllWheelDrive;
    Movement->DifferentialSetup.FrontRearSplit=.5f;
    Movement->SteeringSetup.SteeringType=ESteeringType::AngleRatio;
    Movement->SteeringSetup.AngleRatio=.7f;
}
