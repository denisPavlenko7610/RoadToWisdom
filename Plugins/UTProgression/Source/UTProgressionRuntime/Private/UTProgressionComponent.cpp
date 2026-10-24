// UT Templates - progression component implementation.
#include "UTProgressionComponent.h"

#include "Net/UnrealNetwork.h"

UUTProgressionComponent::UUTProgressionComponent()
{
	SetIsReplicatedByDefault(true);
}

void UUTProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UUTProgressionComponent, TotalXp);
	DOREPLIFETIME(UUTProgressionComponent, CurrentLevel);
}

void UUTProgressionComponent::OnRep_TotalXp()
{
	// Level changes ride along with XP replication; clients announce level-ups here.
	while (LastSeenLevel < CurrentLevel) {
		const int32 OldLevel = LastSeenLevel;
		++LastSeenLevel;
		FUTLevelUpSignature LevelUpEvent = OnLevelUp;
		LevelUpEvent.Broadcast(LastSeenLevel, OldLevel);
	}
	LastSeenLevel = CurrentLevel;

	FUTXpChangedSignature XpEvent = OnXpChanged;
	XpEvent.Broadcast(TotalXp, 0);
}

int32 UUTProgressionComponent::AddXp(int32 Amount)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return 0;
	}

	if (Amount <= 0) {
		return 0;
	}

	const int32 OldLevel = CurrentLevel;
	TotalXp += FMath::Min<int64>(Amount, MAX_int64 - TotalXp);
	CurrentLevel = LevelFromXp(TotalXp);

	FUTXpChangedSignature XpEvent = OnXpChanged;
	XpEvent.Broadcast(TotalXp, Amount);

	for (int32 Level = OldLevel + 1; Level <= CurrentLevel; ++Level) {
		FUTLevelUpSignature LevelUpEvent = OnLevelUp;
		LevelUpEvent.Broadcast(Level, Level - 1);
	}
	return CurrentLevel - OldLevel;
}

int32 UUTProgressionComponent::GetXpToNextLevel() const
{
	if (IsMaxLevel()) {
		return -1;
	}
	return static_cast<int32>(XpForLevel(CurrentLevel + 1) - TotalXp);
}

float UUTProgressionComponent::GetLevelProgress() const
{
	if (IsMaxLevel()) {
		return 1.f;
	}

	const int64 LevelStart = XpForLevel(CurrentLevel);
	const int64 NextLevelStart = XpForLevel(CurrentLevel + 1);
	const int64 Span = NextLevelStart - LevelStart;
	return Span > 0 ? static_cast<float>(TotalXp - LevelStart) / static_cast<float>(Span) : 0.f;
}

bool UUTProgressionComponent::IsMaxLevel() const
{
	return CurrentLevel >= GetMaxLevel();
}

FUTProgressionSnapshot UUTProgressionComponent::MakeSnapshot() const
{
	FUTProgressionSnapshot Snapshot;
	Snapshot.TotalXp = TotalXp;
	return Snapshot;
}

void UUTProgressionComponent::RestoreFromSnapshot(const FUTProgressionSnapshot& Snapshot)
{
	if (GetOwner() && !GetOwner()->HasAuthority()) {
		return;
	}
	TotalXp = FMath::Max<int64>(0, Snapshot.TotalXp);
	CurrentLevel = LevelFromXp(TotalXp);
	// No gameplay events: the loading orchestrator rebuilds derived state once at the end.
}

int64 UUTProgressionComponent::XpForLevel(int32 Level) const
{
	if (Level <= 1) {
		return 0;
	}
	if (!LevelXpThresholds.IsEmpty()) {
		const int32 Index = FMath::Clamp(Level - 1, 0, LevelXpThresholds.Num() - 1);
		return static_cast<int64>(LevelXpThresholds[Index]);
	}
	// Linear fallback: level L costs (L-1) * BaseXpPerLevel cumulative XP.
	return static_cast<int64>(Level - 1) * static_cast<int64>(FMath::Max(1, BaseXpPerLevel));
}

int32 UUTProgressionComponent::GetMaxLevel() const
{
	return !LevelXpThresholds.IsEmpty() ? LevelXpThresholds.Num() : FMath::Max(1, MaxLevel);
}

int32 UUTProgressionComponent::LevelFromXp(int64 InXp) const
{
	int32 Level = 1;
	while (Level < GetMaxLevel() && InXp >= XpForLevel(Level + 1)) {
		++Level;
	}
	return Level;
}
