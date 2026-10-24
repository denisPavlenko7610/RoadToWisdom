// UT Templates - damage dispatch implementation.
#include "UTDamageDispatch.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
static TAutoConsoleVariable<int32> CVarUTDebugCombat(TEXT("ut.Debug.Combat"), 0, TEXT("Enable toolkit combat diagnostics."), ECVF_Cheat);
#endif

#include "GameFramework/Actor.h"
#include "UObject/Interface.h"

namespace UTCombat
{
UObject* ResolveDamageReceiver(const AActor* Actor)
{
	if (!Actor) {
		return nullptr;
	}

	// Actor-level implementation wins (it can coordinate several components).
	if (Actor->Implements<UUTDamageReceiver>()) {
		return const_cast<AActor*>(Actor);
	}

	// Component-level: the common case - an ordinary pawn with a UUTHealthComponent.
	UObject* Result = nullptr;
	Actor->ForEachComponent<UActorComponent>(/*bIncludeFromChildActors*/ false, [&Result](const UActorComponent* Component) {
		if (!Result && Component && Component->Implements<UUTDamageReceiver>()) {
			Result = const_cast<UActorComponent*>(Component);
		}
	});
	return Result;
}

bool CanDamageActor(const AActor* Actor)
{
	UObject* Receiver = ResolveDamageReceiver(Actor);
	return Receiver && IUTDamageReceiver::Execute_CanReceiveDamage(Receiver);
}

FUTDamageResult ApplyDamageToActor(AActor* Actor, const FUTDamageContext& Context)
{
	if (!IsValid(Actor) || !Actor->HasAuthority()) {
		return FUTDamageResult();
	}
	UObject* Receiver = ResolveDamageReceiver(Actor);
	const FUTDamageResult Result = Receiver ? IUTDamageReceiver::Execute_ApplyDamage(Receiver, Context) : FUTDamageResult();
#if !UE_BUILD_SHIPPING
	if (CVarUTDebugCombat.GetValueOnGameThread() != 0) {
		UE_LOG(LogTemp, Display, TEXT("UTCombat authority=server source=%s causer=%s target=%s requested=%.2f applied=%.2f accepted=%d"),
			   *GetNameSafe(Context.Instigator.Get()), *GetNameSafe(Context.Causer.Get()), *GetNameSafe(Actor), Context.BaseAmount, Result.AmountApplied, Result.bApplied);
		if (IsValid(Actor)) {
			DrawDebugString(Actor->GetWorld(), Actor->GetActorLocation(), FString::Printf(TEXT("Damage %.1f (server)"), Result.AmountApplied), nullptr, FColor::Red, 2.f);
		}
	}
#endif
	return Result;
}
} // namespace UTCombat
