#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadToWisdomCamperBody.generated.h"
class ARoadToWisdomPawn;
class UStaticMeshComponent;
class UBoxComponent;

/** Presentation attached to the existing Chaos chassis; never replaces its physics. */
UCLASS()
class ARoadToWisdomCamperBody : public AActor
{
	GENERATED_BODY()

public:
	ARoadToWisdomCamperBody();

	UPROPERTY(EditInstanceOnly, Category = "QuietRoad|Camper")
	TObjectPtr<ARoadToWisdomPawn> Van;
	UPROPERTY(VisibleAnywhere, Category = "QuietRoad|Camper")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "QuietRoad|Camper")
	TObjectPtr<UBoxComponent> WalkCollision;
	UPROPERTY(VisibleAnywhere, Category = "QuietRoad|Camper")
	TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
	UPROPERTY(VisibleAnywhere, Category = "QuietRoad|Camper")
	TArray<TObjectPtr<UStaticMeshComponent>> Hubs;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "QuietRoad|Camper")
	void AttachPresentation();
};
