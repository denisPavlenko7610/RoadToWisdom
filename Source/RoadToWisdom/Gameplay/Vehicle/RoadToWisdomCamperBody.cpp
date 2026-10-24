#include "Gameplay/Vehicle/RoadToWisdomCamperBody.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"

ARoadToWisdomCamperBody::ARoadToWisdomCamperBody()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WalkCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WalkCollision"));
	WalkCollision->SetupAttachment(Body);
	WalkCollision->SetRelativeLocation(FVector(-5, 0, 155));
	WalkCollision->SetBoxExtent(FVector(220, 106, 100));
	WalkCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WalkCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	WalkCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	WalkCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	WalkCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	for (int32 I = 0; I < 4; ++I) {
		auto* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"), I));
		Wheel->SetupAttachment(Body);
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheels.Add(Wheel);
		auto* Hub = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Hub%d"), I));
		Hub->SetupAttachment(Wheel);
		Hub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hubs.Add(Hub);
	}
}

void ARoadToWisdomCamperBody::AttachPresentation()
{
	if (!IsValid(Van) || !Body->GetStaticMesh()) return;
	auto* Chassis = Van->GetMesh();
	AttachToComponent(Chassis, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	// Keep suspension/bone animation running although the placeholder skin is hidden.
	Chassis->SetVisibility(false, false);
	Chassis->SetCastShadow(false);
	Chassis->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	static const FName Bones[] = { TEXT("Phys_Wheel_FL"), TEXT("Phys_Wheel_FR"), TEXT("Phys_Wheel_BL"), TEXT("Phys_Wheel_BR") };
	for (int32 I = 0; I < 4; ++I) {
		Wheels[I]->AttachToComponent(Chassis, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bones[I]);
		Wheels[I]->SetRelativeLocation(FVector(I % 2 == 0 ? 16.f : -16.f, 0, 0));
		Wheels[I]->SetRelativeRotation(FRotator(90, 0, 0));
		Wheels[I]->SetRelativeScale3D(FVector(0.72f, 0.72f, 0.26f));
		Hubs[I]->SetRelativeScale3D(FVector(0.52f, 0.52f, 1.05f));
	}
}

void ARoadToWisdomCamperBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AttachPresentation();
}

void ARoadToWisdomCamperBody::BeginPlay()
{
	Super::BeginPlay();
	AttachPresentation();
}
