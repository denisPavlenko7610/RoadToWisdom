// UT Templates - teleporter re-entry guard subsystem.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UTTeleporterSubsystem.generated.h"

/**
 * Per-world re-entry guard shared by every teleporter of that world.
 *
 * A per-portal "recently teleported" set cannot express "not through this portal again
 * for a moment": a destination that sits inside another portal's trigger keeps bouncing
 * the actor forever. The guard lives here so all portals of one world agree, and it
 * expires on its own. Per-world state (never a file-static map): PIE instances and
 * server/client worlds each get their own guard, so entries never leak across worlds.
 *
 * Purely a teleport bookkeeping helper - teleport gameplay itself runs on the server.
 */
UCLASS()
class UTWORLDMECHANICSRUNTIME_API UUTTeleporterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr double GracePeriodSeconds = 0.25;

	static UUTTeleporterSubsystem* Get(const UObject* WorldContextObject);

	/** Record a completed/attempted teleport for Actor in this world. */
	void Grant(const AActor* Actor);

	/** True while the actor is within the grace period and must not teleport again. */
	bool IsSuppressed(const AActor* Actor) const;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

private:
	/** Prune expired and dead entries once per second from Tick. */
	TMap<TWeakObjectPtr<const AActor>, double> LastTeleportTime;
	double TimeSincePrune = 0.0;
};
