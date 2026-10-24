// UT Templates - day/night controller actor.
#pragma once

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "UTDayNightController.generated.h"

class ADirectionalLight;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTTimeChangedSignature, float, Hour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUTPhaseChangedSignature, bool, bIsNight);

/**
 * Time-of-day bookkeeping and sun rotation. 6:00 maps to sun pitch 0 (east horizon),
 * 12:00 to zenith, 18:00 to the opposite horizon. Sky material parameters, light
 * intensity curves and fog stay in game/Blueprint code subscribed to OnTimeChanged -
 * only the clock and the sun angle live here.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API AUTDayNightController : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "UT|DayNight")
	FUTTimeChangedSignature OnTimeChanged;

	/** Fires when crossing the day/night threshold; bool is the new "is night" state. */
	UPROPERTY(BlueprintAssignable, Category = "UT|DayNight")
	FUTPhaseChangedSignature OnPhaseChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UT|DayNight")
	TObjectPtr<USceneComponent> Root;

	/** Directional light to rotate; optional - the clock runs without it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight")
	TObjectPtr<ADirectionalLight> SunLight;

	/** Sun azimuth (yaw) in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float SunYaw = 0.f;

	/** Start hour in [0, 24). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float StartHour = 8.f;

	/** Real seconds for one full 24-hour cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight", meta = (ClampMin = "1.0"))
	float DayLengthSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight")
	bool bRunning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float NightStartHour = 19.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT|DayNight", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float DayStartHour = 5.5f;

	UFUNCTION(BlueprintCallable, Category = "UT|DayNight")
	void SetTimeOfDay(float NewHour);

	UFUNCTION(BlueprintPure, Category = "UT|DayNight")
	float GetTimeOfDay() const { return TimeOfDay; }

	UFUNCTION(BlueprintPure, Category = "UT|DayNight")
	bool IsNight() const;

	UFUNCTION(BlueprintCallable, Category = "UT|DayNight")
	void SetRunning(bool bNewRunning);

	AUTDayNightController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	float TimeOfDay = 8.f;
	bool bNight = false;

	void AdvanceTime(float DeltaHours);
	void UpdateSun() const;
};
