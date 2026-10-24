#pragma once
#include "UObject/Object.h"
#include "UTProgressionTestObserver.generated.h"

UCLASS()
class UUTProgressionTestObserver : public UObject
{
	GENERATED_BODY()

public:
	TArray<int32> Levels;
	int64 LastBalance = -1;
	int32 Accepted = 0;

	UFUNCTION()
	void BalanceChanged(FName Currency, int64 Value)
	{
		LastBalance = Value;
	}

	UFUNCTION()
	void QuestAccepted(class UUTQuestDefinition* Definition)
	{
		++Accepted;
	}

	UFUNCTION()
	void LevelUp(int32 NewLevel, int32 OldLevel)
	{
		Levels.Add(NewLevel);
	}
};
