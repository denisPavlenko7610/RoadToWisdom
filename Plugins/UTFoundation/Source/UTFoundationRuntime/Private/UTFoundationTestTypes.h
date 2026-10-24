// UT Templates - foundation test types.
#pragma once

#include "CoreMinimal.h"
#include "UTFoundationTestTypes.generated.h"

USTRUCT()
struct FUTTestPingMessage
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};
