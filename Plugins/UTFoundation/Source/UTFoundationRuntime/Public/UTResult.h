// UT Templates - lightweight success/failure outcome.
#pragma once

#include "CoreMinimal.h"
#include "UTResult.generated.h"

/**
 * Outcome of an operation that can fail in expected, reportable ways.
 * Code is a stable machine-readable token (e.g. "Success", "InsufficientFunds", "SlotFull");
 * Message is a human-readable explanation for logs and debugging.
 */
USTRUCT(BlueprintType)
struct UTFOUNDATIONRUNTIME_API FUTResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FName Code = TEXT("Success");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UT")
	FText Message;

	static FUTResult Success()
	{
		FUTResult Result;
		Result.bSuccess = true;
		Result.Code = TEXT("Success");
		return Result;
	}

	static FUTResult Failure(const FName& InCode, const FText& InMessage)
	{
		FUTResult Result;
		Result.bSuccess = false;
		Result.Code = InCode;
		Result.Message = InMessage;
		return Result;
	}

	bool IsSuccess() const { return bSuccess; }
};
