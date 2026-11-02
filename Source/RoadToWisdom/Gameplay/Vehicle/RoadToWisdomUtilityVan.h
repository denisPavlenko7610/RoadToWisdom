#pragma once
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"
#include "RoadToWisdomUtilityVan.generated.h"

class USpotLightComponent;
class UPointLightComponent;

/** Undamaged utility van using its own wheel skeleton and native Chaos animation. */
UCLASS()
class ROADTOWISDOM_API ARoadToWisdomUtilityVan : public ARoadToWisdomPawn
{
	GENERATED_BODY()

public:
	ARoadToWisdomUtilityVan();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void Tick(float DeltaSeconds) override;

private:
	void ToggleLights();

	UPROPERTY(VisibleAnywhere, Category="Lights")
	TObjectPtr<USpotLightComponent> HeadlightLeft;
	UPROPERTY(VisibleAnywhere, Category="Lights")
	TObjectPtr<USpotLightComponent> HeadlightRight;
	UPROPERTY(VisibleAnywhere, Category="Lights")
	TObjectPtr<UPointLightComponent> TailLightLeft;
	UPROPERTY(VisibleAnywhere, Category="Lights")
	TObjectPtr<UPointLightComponent> TailLightRight;
	UPROPERTY(EditAnywhere, Category="Lights")
	float HeadlightIntensity = 30000.f;
	UPROPERTY(EditAnywhere, Category="Lights")
	float TailIntensity = 100.f;
	UPROPERTY(EditAnywhere, Category="Lights")
	float BrakeIntensity = 800.f;
	bool bLightsOn = false;
};
