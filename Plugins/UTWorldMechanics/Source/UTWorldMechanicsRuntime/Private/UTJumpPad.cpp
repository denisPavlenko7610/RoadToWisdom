// UT Templates - jump pad actor implementation.
#include "UTJumpPad.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"

namespace {
	// Entries are pruned once expired for this multiple of PerCharacterCooldown.
	constexpr double StaleEntryFactor = 2.0;
}

AUTJumpPad::AUTJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetBoxExtent(FVector(80.f, 80.f, 30.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
}

void AUTJumpPad::BeginPlay()
{
	Super::BeginPlay();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AUTJumpPad::HandleOverlapBegin);
}

void AUTJumpPad::HandleOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
									UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!HasAuthority() || !Character || !Character->GetCharacterMovement()) {
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (PerCharacterCooldown > 0.f) {
		// Prune stale entries so the map cannot grow with long-running levels.
		for (auto It = CooldownUntil.CreateIterator(); It; ++It) {
			if (!It->Key.IsValid() || It->Value < Now - PerCharacterCooldown * StaleEntryFactor) {
				It.RemoveCurrent();
			}
		}

		if (const double* ReadyAt = CooldownUntil.Find(Character)) {
			if (Now < *ReadyAt) {
				return;
			}
		}
		CooldownUntil.Add(Character, Now + PerCharacterCooldown);
	}

	Character->LaunchCharacter(LaunchVelocity, /*bXYOverride*/ bOverrideVelocity, /*bZOverride*/ bOverrideVelocity);

	FUTLaunchedSignature LaunchedEvent = OnLaunched;
	LaunchedEvent.Broadcast(this, Character);
}
