// UT Templates - door actor implementation.
#include "UTDoor.h"

#include "Net/UnrealNetwork.h"

namespace {
	// Seconds; floor on OpenTime so Tick never divides by zero (matches OpenTime's ClampMin).
	constexpr float MinOpenTime = 0.01f;
}

AUTDoor::AUTDoor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	RootComponent = Hinge;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void AUTDoor::BeginPlay()
{
	Super::BeginPlay();

	ClosedRelative = DoorMesh->GetRelativeTransform();

	FTransform OpenTransform = ClosedRelative;
	if (MoveMode == EUTDoorMoveMode::Rotate) {
		const FQuat OpenQuat = ClosedRelative.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(OpenAngle));
		OpenTransform.SetRotation(OpenQuat);
	}
	else {
		OpenTransform.SetLocation(ClosedRelative.GetLocation() + SlideOffset);
	}
	OpenRelative = OpenTransform;

	if (HasAuthority()) {
		if (bStartOpen) {
			bIsOpen = true;
			Alpha = TargetAlpha = 1.f;
			ApplyDoorTransform();
		}
		InitialPose.bOpen = bIsOpen;
		InitialPose.Alpha = Alpha;
	}
	else {
		ApplyInitialPose();
	}
}

void AUTDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTDoor, bIsOpen);
	DOREPLIFETIME_CONDITION_NOTIFY(AUTDoor, InitialPose, COND_InitialOnly, REPNOTIFY_Always);
}

void AUTDoor::OnRep_bIsOpen()
{
	// Replicated state is the animation target; the motion eases from wherever the
	// local visual currently is, so the door swings open on clients the same way it
	// does on the server.
	UpdateMotionTargets();
	SetActorTickEnabled(!FMath::IsNearlyEqual(Alpha, TargetAlpha));
}
void AUTDoor::OnRep_InitialPose()
{
	if (HasActorBegunPlay()) {
		ApplyInitialPose();
	}
}
void AUTDoor::ApplyInitialPose()
{
	bIsOpen = InitialPose.bOpen;
	Alpha = FMath::Clamp(InitialPose.Alpha, 0.f, 1.f);
	UpdateMotionTargets();
	ApplyDoorTransform();
	SetActorTickEnabled(!FMath::IsNearlyEqual(Alpha, TargetAlpha));
}

void AUTDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Step = DeltaSeconds / FMath::Max(MinOpenTime, OpenTime);
	Alpha = TargetAlpha > Alpha
				? FMath::Min(Alpha + Step, TargetAlpha)
				: FMath::Max(Alpha - Step, TargetAlpha);

	ApplyDoorTransform();
	if (HasAuthority()) {
		InitialPose.Alpha = Alpha;
	}

	if (FMath::IsNearlyEqual(Alpha, TargetAlpha)) {
		SetActorTickEnabled(false);
		if (bIsOpen) {
			FUTDoorOpenedSignature OpenedEvent = OnOpened;
			OpenedEvent.Broadcast();
		}
		else {
			FUTDoorClosedSignature ClosedEvent = OnClosed;
			ClosedEvent.Broadcast();
		}
	}
}

void AUTDoor::Open()
{
	SetOpen(true);
}

void AUTDoor::Close()
{
	SetOpen(false);
}

void AUTDoor::SetOpen(bool bNewOpen)
{
	// Server-authoritative: clients derive the door state from replication.
	if (!HasAuthority()) {
		return;
	}

	if (bIsOpen == bNewOpen) {
		return;
	}

	bIsOpen = bNewOpen;
	InitialPose.bOpen = bIsOpen;
	InitialPose.Alpha = Alpha;
	ForceNetUpdate();
	UpdateMotionTargets();
	SetActorTickEnabled(true);
}

bool AUTDoor::IsMoving() const
{
	return IsActorTickEnabled();
}

FInstancedStruct AUTDoor::CaptureState_Implementation() const
{
	return FInstancedStruct::Make<FUTDoorSaveState>({bIsOpen});
}

bool AUTDoor::RestoreState_Implementation(const FInstancedStruct& State)
{
	const FUTDoorSaveState* DoorState = State.GetPtr<FUTDoorSaveState>();
	if (!HasAuthority() || !DoorState) {
		return false;
	}

	// Restores publish no gameplay events: snap to the saved pose silently.
	bIsOpen = DoorState->bOpen;
	Alpha = DoorState->bOpen ? 1.f : 0.f;
	TargetAlpha = Alpha;
	UpdateMotionTargets();
	ApplyDoorTransform();
	InitialPose.bOpen = bIsOpen;
	InitialPose.Alpha = Alpha;
	SetActorTickEnabled(false);
	ForceNetUpdate();
	return true;
}

void AUTDoor::ApplyDoorTransform()
{
	// FTransform has no arithmetic ops, so compose the eased transform from its parts.
	const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);
	const FQuat Rotation = FQuat::Slerp(ClosedRelative.GetRotation(), OpenRelative.GetRotation(), Eased);
	const FVector Location = FMath::Lerp(ClosedRelative.GetLocation(), OpenRelative.GetLocation(), Eased);
	const FVector Scale = FMath::Lerp(ClosedRelative.GetScale3D(), OpenRelative.GetScale3D(), Eased);
	DoorMesh->SetRelativeTransform(FTransform(Rotation, Location, Scale));
}

void AUTDoor::UpdateMotionTargets()
{
	TargetAlpha = bIsOpen ? 1.f : 0.f;
}
