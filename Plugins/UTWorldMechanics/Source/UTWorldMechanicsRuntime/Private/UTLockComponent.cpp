#include "UTLockComponent.h"
#include "UTActionCost.h"
#include "UTConditions.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
UUTLockComponent::UUTLockComponent()
{
	SetIsReplicatedByDefault(true);
}
void UUTLockComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority()) {
		bLocked = bInitiallyLocked;
	}
}
void UUTLockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTLockComponent, bLocked);
}
bool UUTLockComponent::IsUserValid(AActor* User) const
{
	return IsValid(User) && GetOwner() && User->GetWorld() == GetWorld() && FMath::IsFinite(UnlockDistance) && UnlockDistance >= 0.f && (UnlockDistance == 0.f || FVector::DistSquared(User->GetActorLocation(), GetOwner()->GetActorLocation()) <= FMath::Square(UnlockDistance));
}
bool UUTLockComponent::CanUse(AActor* User) const
{
	return !bLocked && IsUserValid(User);
}
bool UUTLockComponent::CanUnlock(AActor* User) const
{
	if (!IsUserValid(User)) {
		return false;
	}
	if (!bLocked) {
		return true;
	}
	FUTConditionContext Context;
	Context.Subject = User;
	Context.Instigator = User;
	return (!UnlockConditions || UnlockConditions->Evaluate(Context)) && (!UnlockCost || UnlockCost->CanPay(User));
}
bool UUTLockComponent::TryUnlock(AActor* User)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bTransacting || !IsValid(User) || !User->HasAuthority()) {
		return false;
	}
	{
		TGuardValue<bool> Transaction(bTransacting, true);
		if (!CanUnlock(User)) {
			return false;
		}
		if (!bLocked) {
			return true;
		}
		if (bConsumeKey && UnlockCost && !UnlockCost->Pay(User)) {
			return false;
		}
		bLocked = false;
		GetOwner()->ForceNetUpdate();
	}
	OnRep_Locked();
	return true;
}
void UUTLockComponent::Lock()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bTransacting || bLocked) {
		return;
	}
	bLocked = true;
	GetOwner()->ForceNetUpdate();
	OnRep_Locked();
}
void UUTLockComponent::OnRep_Locked()
{
	OnLockChanged.Broadcast(bLocked);
	if (!bLocked) {
		OnUnlocked.Broadcast();
	}
}
FUTLockState UUTLockComponent::MakeSnapshot() const
{
	FUTLockState State;
	State.bLocked = bLocked;
	return State;
}
bool UUTLockComponent::RestoreFromSnapshot(const FUTLockState& State)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bTransacting) {
		return false;
	}
	bLocked = State.bLocked;
	GetOwner()->ForceNetUpdate();
	return true;
}
