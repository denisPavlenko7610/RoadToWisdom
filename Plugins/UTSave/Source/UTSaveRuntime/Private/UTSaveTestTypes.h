// UT Templates - save test types.
#pragma once

#include "CoreMinimal.h"
#include "UTSaveTestTypes.generated.h"

USTRUCT()
struct FUTTestSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Score = 0;

	UPROPERTY()
	TArray<FString> Names;
};
