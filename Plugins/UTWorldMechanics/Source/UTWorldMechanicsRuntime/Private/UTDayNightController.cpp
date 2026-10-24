// UT Templates - day/night controller implementation.
#include "UTDayNightController.h"

#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"

namespace {
	constexpr float HoursPerDay = 24.f;
}

AUTDayNightController::AUTDayNightController()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AUTDayNightController::BeginPlay()
{
	Super::BeginPlay();
	TimeOfDay = FMath::Fmod(StartHour, HoursPerDay);
	bNight = IsNight();
	UpdateSun();
	SetActorTickEnabled(bRunning);
}

void AUTDayNightController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceTime(DeltaSeconds * (HoursPerDay / FMath::Max(1.f, DayLengthSeconds)));
}

void AUTDayNightController::AdvanceTime(float DeltaHours)
{
	TimeOfDay = FMath::Fmod(TimeOfDay + DeltaHours + HoursPerDay, HoursPerDay);

	FUTTimeChangedSignature TimeEvent = OnTimeChanged;
	TimeEvent.Broadcast(TimeOfDay);

	// IsNight covers both thresholds, including the midnight wrap (23.6 -> 0.1).
	const bool bNowNight = IsNight();
	if (bNowNight != bNight) {
		bNight = bNowNight;
		FUTPhaseChangedSignature PhaseEvent = OnPhaseChanged;
		PhaseEvent.Broadcast(bNight);
	}

	UpdateSun();
}

void AUTDayNightController::SetTimeOfDay(float NewHour)
{
	TimeOfDay = FMath::Fmod(NewHour + HoursPerDay, HoursPerDay);
	FUTTimeChangedSignature TimeEvent = OnTimeChanged;
	TimeEvent.Broadcast(TimeOfDay);

	const bool bNowNight = IsNight();
	if (bNowNight != bNight) {
		bNight = bNowNight;
		FUTPhaseChangedSignature PhaseEvent = OnPhaseChanged;
		PhaseEvent.Broadcast(bNight);
	}
	UpdateSun();
}

bool AUTDayNightController::IsNight() const
{
	return TimeOfDay >= NightStartHour || TimeOfDay < DayStartHour;
}

void AUTDayNightController::SetRunning(bool bNewRunning)
{
	bRunning = bNewRunning;
	SetActorTickEnabled(bRunning);
}

void AUTDayNightController::UpdateSun() const
{
	if (!SunLight) {
		return;
	}

	// 6:00 -> horizon (0 deg), 12:00 -> zenith (90 deg), 18:00 -> opposite horizon.
	const float Pitch = FMath::Fmod(((TimeOfDay - 6.f) / HoursPerDay) * 360.f + 360.f + 180.f, 360.f) - 180.f;
	SunLight->SetActorRotation(FRotator(Pitch, SunYaw, 0.f));
}
