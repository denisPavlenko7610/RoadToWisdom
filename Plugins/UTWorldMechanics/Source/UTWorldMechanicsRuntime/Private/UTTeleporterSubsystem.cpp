// UT Templates - teleporter re-entry guard subsystem implementation.
#include "UTTeleporterSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace {
	constexpr double PruneIntervalSeconds = 1.0;
}

UUTTeleporterSubsystem* UUTTeleporterSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine && WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UUTTeleporterSubsystem>() : nullptr;
}

void UUTTeleporterSubsystem::Grant(const AActor* Actor)
{
	if (!Actor) {
		return;
	}
	LastTeleportTime.Add(Actor, FPlatformTime::Seconds());
}

bool UUTTeleporterSubsystem::IsSuppressed(const AActor* Actor) const
{
	const double* Last = LastTeleportTime.Find(Actor);
	return Last != nullptr && (FPlatformTime::Seconds() - *Last) < GracePeriodSeconds;
}

void UUTTeleporterSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSincePrune += DeltaTime;
	if (TimeSincePrune < PruneIntervalSeconds) {
		return;
	}
	TimeSincePrune = 0.0;

	const double Now = FPlatformTime::Seconds();
	for (auto It = LastTeleportTime.CreateIterator(); It; ++It) {
		if (!It->Key.IsValid() || Now - It->Value >= GracePeriodSeconds) {
			It.RemoveCurrent();
		}
	}
}

TStatId UUTTeleporterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UUTTeleporterSubsystem, STATGROUP_Tickables);
}
