// UT Templates - lever actor implementation.
#include "UTLever.h"
#include "Net/UnrealNetwork.h"

AUTLever::AUTLever()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;
	BaseMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(BaseMesh);
}

void AUTLever::BeginPlay()
{
	Super::BeginPlay();
	ApplyHandleRotation();
}

void AUTLever::Interact_Implementation(const FUTInteractionInfo& Info)
{
	Toggle();
}

void AUTLever::SetOn(bool bNewOn)
{
	if (!HasAuthority() || !bUsable || bIsOn == bNewOn) {
		return;
	}

	bIsOn = bNewOn;
	ApplyHandleRotation();

	FUTLeverSwitchedSignature SwitchedEvent = OnSwitched;
	SwitchedEvent.Broadcast(this, bIsOn);
}

void AUTLever::Toggle()
{
	SetOn(!bIsOn);
}

void AUTLever::ApplyHandleRotation()
{
	if (HandleMesh) {
		HandleMesh->SetRelativeRotation(bIsOn ? HandleOnRotation : FRotator::ZeroRotator);
	}
}

void AUTLever::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTLever, bIsOn);
}
void AUTLever::OnRep_IsOn()
{
	ApplyHandleRotation();
	OnSwitched.Broadcast(this, bIsOn);
}
