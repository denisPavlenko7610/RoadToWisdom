// UT Templates - moving platform actor implementation.
#include "UTMovingPlatform.h"

namespace {
	// Seconds; floor on MoveTime so Tick never divides by zero (matches MoveTime's ClampMin).
	constexpr float MinMoveTime = 0.05f;
}

AUTMovingPlatform::AUTMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	RootComponent = PlatformMesh;
	PlatformMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AUTMovingPlatform::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	SetActorTickEnabled(HasAuthority() && bActive);
}

void AUTMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) {
		return;
	}

	if (bPaused) {
		PauseRemaining -= DeltaSeconds;
		if (PauseRemaining > 0.f) {
			return;
		}
		bPaused = false;
	}

	Alpha += Direction * (DeltaSeconds / FMath::Max(MinMoveTime, MoveTime));

	if (Alpha >= 1.f) {
		Alpha = 1.f;
		FUTPlatformReachedEndSignature ReachedEndEvent = OnReachedEnd;
		ReachedEndEvent.Broadcast();

		if (bPingPong) {
			Direction = -1;
		}
		else {
			Alpha = 0.f;
			Direction = 1;
		}
		PauseRemaining = PauseAtEnds;
		bPaused = PauseAtEnds > 0.f;
	}
	else if (Alpha <= 0.f && Direction < 0) {
		Alpha = 0.f;
		Direction = 1;
		PauseRemaining = PauseAtEnds;
		bPaused = PauseAtEnds > 0.f;

		FUTPlatformReachedStartSignature ReachedStartEvent = OnReachedStart;
		ReachedStartEvent.Broadcast();
	}

	SetActorLocation(GetLocationForAlpha(Alpha), /*bSweep*/ true);
}

void AUTMovingPlatform::SetActive(bool bNewActive)
{
	if (!HasAuthority()) {
		return;
	}
	bActive = bNewActive;
	SetActorTickEnabled(bActive);
}

FVector AUTMovingPlatform::GetLocationForAlpha(float InAlpha) const
{
	const float Eased = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(InAlpha, 0.f, 1.f));
	return BaseLocation + EndOffset * Eased;
}
