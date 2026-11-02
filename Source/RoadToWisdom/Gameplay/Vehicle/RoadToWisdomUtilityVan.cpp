#include "Gameplay/Vehicle/RoadToWisdomUtilityVan.h"
#include "VehicleTemplate/OffroadCar/RoadToWisdomOffroadWheelFront.h"
#include "VehicleTemplate/OffroadCar/RoadToWisdomOffroadWheelRear.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"

ARoadToWisdomUtilityVan::ARoadToWisdomUtilityVan()
{
	HeadlightLeft = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightLeft"));
	HeadlightRight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadlightRight"));
	TailLightLeft = CreateDefaultSubobject<UPointLightComponent>(TEXT("TailLightLeft"));
	TailLightRight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TailLightRight"));
	for (USpotLightComponent* Light : { HeadlightLeft.Get(), HeadlightRight.Get() }) {
		Light->SetupAttachment(GetMesh());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetLightColor(FLinearColor(1.f, .9f, .72f));
		Light->SetAttenuationRadius(3500.f);
		Light->SetOuterConeAngle(42.f);
		Light->SetIntensity(0.f);
	}
	HeadlightLeft->SetRelativeLocation(FVector(245.f, -70.f, 80.f));
	HeadlightRight->SetRelativeLocation(FVector(245.f, 70.f, 80.f));
	for (UPointLightComponent* Light : { TailLightLeft.Get(), TailLightRight.Get() }) {
		Light->SetupAttachment(GetMesh());
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetLightColor(FLinearColor::Red);
		Light->SetAttenuationRadius(250.f);
		Light->SetIntensity(0.f);
		Light->SetCastShadows(false);
	}
	TailLightLeft->SetRelativeLocation(FVector(-230.f, -75.f, 90.f));
	TailLightRight->SetRelativeLocation(FVector(-230.f, 75.f, 90.f));
	GetBackSpringArm()->TargetArmLength = 850.f;
	GetBackSpringArm()->bDoCollisionTest = true;
	GetBackSpringArm()->SetRelativeLocation(FVector(0, 0, 110));
	GetFrontSpringArm()->SetRelativeLocation(FVector(195, -45, 145));
	UChaosWheeledVehicleMovementComponent* Movement = GetChaosVehicleMovement();
	Movement->ChassisHeight = 190.f;
	Movement->ChassisWidth = 210.f;
	Movement->Mass = 1850.f;
	Movement->DragCoefficient = .35f;
	Movement->DownforceCoefficient = .1f;
	Movement->CenterOfMassOverride = FVector(0, 0, 75);
	Movement->bEnableCenterOfMassOverride = true;
	Movement->bLegacyWheelFrictionPosition = false;
	Movement->WheelSetups.SetNum(4);
	const FName Bones[] = { TEXT("Wheel_FL"),TEXT("Wheel_FR"),TEXT("Wheel_BL"),TEXT("Wheel_BR") };
	for (int32 I = 0; I < 4; ++I) {
		Movement->WheelSetups[I].BoneName = Bones[I];
		Movement->WheelSetups[I].WheelClass = I < 2
			? URoadToWisdomOffroadWheelFront::StaticClass()
			: URoadToWisdomOffroadWheelRear::StaticClass();
	}
	Movement->EngineSetup.MaxTorque = 450.f;
	Movement->EngineSetup.MaxRPM = 4500.f;
	Movement->EngineSetup.EngineIdleRPM = 900.f;
	Movement->EngineSetup.EngineBrakeEffect = .08f;
	Movement->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;
	Movement->DifferentialSetup.FrontRearSplit = .5f;
	Movement->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	Movement->SteeringSetup.AngleRatio = .7f;
}

void ARoadToWisdomUtilityVan::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ARoadToWisdomUtilityVan::ToggleLights);
}

void ARoadToWisdomUtilityVan::ToggleLights()
{
	bLightsOn = !bLightsOn;
	HeadlightLeft->SetIntensity(bLightsOn ? HeadlightIntensity : 0.f);
	HeadlightRight->SetIntensity(bLightsOn ? HeadlightIntensity : 0.f);
}

void ARoadToWisdomUtilityVan::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bBraking = GetChaosVehicleMovement()->GetBrakeInput() > 0.01f || GetChaosVehicleMovement()->GetHandbrakeInput();
	const float Intensity = bBraking ? BrakeIntensity : (bLightsOn ? TailIntensity : 0.f);
	TailLightLeft->SetIntensity(Intensity);
	TailLightRight->SetIntensity(Intensity);
}
