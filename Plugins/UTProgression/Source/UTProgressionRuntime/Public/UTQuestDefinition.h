// UT Templates - quest definition asset.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UTQuestDefinition.generated.h"

/** One countable objective: advances when the message bus broadcasts EventTag (or any child tag). */
USTRUCT(BlueprintType)
struct UTPROGRESSIONRUNTIME_API FUTObjectiveDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;
};

/**
 * Quest description as data. The quest component advances objectives from message bus
 * broadcasts and calls the game on completion - rewards, journal UI and dialogue stay
 * in game code subscribed to OnQuestCompleted / OnQuestAccepted.
 */
UCLASS(BlueprintType)
class UTPROGRESSIONRUNTIME_API UUTQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable identity used in saves and prerequisites. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT", meta = (MultiLine = "true"))
	FText Description;

	/** All objectives must reach RequiredCount for completion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	TArray<FUTObjectiveDef> Objectives;

	/** QuestIds that must be completed before this one can be accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UT")
	TArray<FName> PrerequisiteQuestIds;
};
