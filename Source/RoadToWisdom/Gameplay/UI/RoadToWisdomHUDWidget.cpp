// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/UI/RoadToWisdomHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "Engine/World.h"

void URoadToWisdomHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree)
	{
		return;
	}

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;

	// Mode label: top left corner.
	ModeText = MakeText(14, FLinearColor(0.7f, 0.75f, 0.8f));
	UCanvasPanelSlot* ModeSlot = Root->AddChildToCanvas(ModeText);
	ModeSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	ModeSlot->SetAlignment(FVector2D(0.f, 0.f));
	ModeSlot->SetPosition(FVector2D(30.f, 30.f));
	ModeSlot->SetAutoSize(true);

	// Objective: top center.
	ObjectiveText = MakeText(20, FLinearColor(0.95f, 0.92f, 0.8f));
	UCanvasPanelSlot* ObjectiveSlot = Root->AddChildToCanvas(ObjectiveText);
	ObjectiveSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
	ObjectiveSlot->SetAlignment(FVector2D(0.5f, 0.f));
	ObjectiveSlot->SetPosition(FVector2D(0.f, 30.f));
	ObjectiveSlot->SetAutoSize(true);

	// Prompt: bottom center.
	PromptText = MakeText(18, FLinearColor::White);
	UCanvasPanelSlot* PromptSlot = Root->AddChildToCanvas(PromptText);
	PromptSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
	PromptSlot->SetAlignment(FVector2D(0.5f, 1.f));
	PromptSlot->SetPosition(FVector2D(0.f, -140.f));
	PromptSlot->SetAutoSize(true);
	PromptText->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Hint: screen center.
	HintText = MakeText(18, FLinearColor(1.f, 0.85f, 0.4f));
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(HintText);
	HintSlot->SetAnchors(FAnchors(0.5f, 0.6f, 0.5f, 0.6f));
	HintSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	HintSlot->SetPosition(FVector2D::ZeroVector);
	HintSlot->SetAutoSize(true);
	HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URoadToWisdomHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HintTimer);
	}
	Super::NativeDestruct();
}

UTextBlock* URoadToWisdomHUDWidget::MakeText(int32 FontSize, FLinearColor Color) const
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FontSize);
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
	Text->SetShadowColorAndOpacity(FLinearColor::Black);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

void URoadToWisdomHUDWidget::SetMode(const FText& Mode)
{
	if (ModeText)
	{
		ModeText->SetText(Mode);
	}
}

void URoadToWisdomHUDWidget::SetObjective(const FText& Objective)
{
	if (ObjectiveText)
	{
		ObjectiveText->SetText(Objective);
	}
}

void URoadToWisdomHUDWidget::SetPrompt(const FText& Prompt)
{
	if (PromptText)
	{
		PromptText->SetText(Prompt);
		PromptText->SetVisibility(Prompt.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}

void URoadToWisdomHUDWidget::ShowHint(const FText& Hint, float Seconds)
{
	if (!HintText)
	{
		return;
	}

	HintText->SetText(Hint);
	HintText->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HintTimer);
		World->GetTimerManager().SetTimer(HintTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (HintText)
			{
				HintText->SetVisibility(ESlateVisibility::Hidden);
			}
		}), Seconds, false);
	}
}
