// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoadToWisdomHUDWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

/**
 *  Minimal mission HUD for the vertical slice (GDD §13): current objective,
 *  interaction prompt and a transient hint line for blocked-action reasons.
 *
 *  The widget tree is built in code on purpose: no Blueprint asset is required to
 *  run the slice, and a later WBP subclass can override the layout while keeping
 *  this class's API. Text is styled with the engine default font.
 */
UCLASS()
class URoadToWisdomHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Top line: what the player should be doing now (from the mission manager). */
	UFUNCTION(BlueprintCallable, Category = "QuietRoad|HUD")
	void SetObjective(const FText& Objective);

	/** Bottom line: focused interaction verb (from UTInteractor) or vehicle exit hint. */
	UFUNCTION(BlueprintCallable, Category = "QuietRoad|HUD")
	void SetPrompt(const FText& Prompt);

	/** Center line for blocked-action reasons ("Остановите фургон"); auto-hides. */
	UFUNCTION(BlueprintCallable, Category = "QuietRoad|HUD")
	void ShowHint(const FText& Hint, float Seconds = 2.5f);

protected:
	/** Creates a styled text block (caller adds it to the canvas). */
	UTextBlock* MakeText(int32 FontSize, FLinearColor Color) const;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> Root;

	UPROPERTY()
	TObjectPtr<UTextBlock> ObjectiveText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HintText;

	FTimerHandle HintTimer;
};
