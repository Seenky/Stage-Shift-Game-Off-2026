// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/Dialogue/DialogueBubbleWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Dialogue/DialogueTextWidget.h"
#include "Components/Dialogue/DialogueViewModel.h"

bool UDialogueBubbleWidget::GetScreenBounds(FBox2D& Bounds, bool bAbsolute) const
{
	if (!DialogueBorder || !DialogueBorder->IsVisible() || DialogueBorder->GetBrushColor().A <= 0.01f) return false;
	const FGeometry& Geometry = DialogueBorder->GetCachedGeometry();
	FVector2D Min = Geometry.GetAbsolutePosition();
	FVector2D Max = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
	if (!bAbsolute)
	{
		FVector2D ViewportPosition;
		USlateBlueprintLibrary::AbsoluteToViewport(this, Geometry.GetAbsolutePosition(), Min, ViewportPosition);
		USlateBlueprintLibrary::AbsoluteToViewport(this, Geometry.LocalToAbsolute(Geometry.GetLocalSize()), Max, ViewportPosition);
	}
	if (!FMath::IsFinite(Min.X) || !FMath::IsFinite(Min.Y) || !FMath::IsFinite(Max.X) || !FMath::IsFinite(Max.Y) ||
	    Max.X <= Min.X || Max.Y <= Min.Y) return false;
	Bounds = FBox2D(Min, Max);
	return true;
}

void UDialogueBubbleWidget::SetDialogueViewModel(UDialogueViewModel* ViewModel)
{
	if (BoundViewModel.Get() == ViewModel)
	{
		return;
	}

	if (UDialogueViewModel* Previous = BoundViewModel.Get())
	{
		Previous->OnPlaybackStateChanged.RemoveAll(this);
		Previous->OnDialogueLineEnded.RemoveDynamic(this, &ThisClass::EndBubble);
	}

	if (DialogueText && DialogueBorder)
	{
		if (!DialogueText->OnOpacityChanged.IsBoundToObject(this))
		{
			BorderColor = DialogueBorder->GetBrushColor();
			DialogueText->OnOpacityChanged.BindUObject(this, &ThisClass::SetBorderOpacity);
		}
		DialogueText->ResetPlayback();
		SetBorderOpacity(0.0f);
	}

	if (SpeakersName)
	{
		SpeakersName->SetShowRevealDots(false);
		SpeakersName->ResetPlayback();
		if (!bShowSpeakerName) SpeakersName->SetVisibility(ESlateVisibility::Collapsed);
	}

	BoundViewModel = ViewModel;
	if (ViewModel)
	{
		ViewModel->OnPlaybackStateChanged.AddUObject(this, &ThisClass::UpdateBubble);
		ViewModel->OnDialogueLineEnded.AddUniqueDynamic(this, &ThisClass::EndBubble);
	}
	OnDialogueViewModelSet(ViewModel);
}

void UDialogueBubbleWidget::UpdateBubble(const FDialoguePlaybackState& State)
{
	if (SpeakersName && bShowSpeakerName)
	{
		FDialoguePlaybackState NameState = State;
		NameState.Line.Text = State.Line.SpeakersName;
		NameState.Line.bItalic = false;
		NameState.Line.bShakeLetters = false;
		NameState.VisibleText = State.Line.SpeakersName;
		NameState.Phase = EDialoguePlaybackPhase::Holding;

		SpeakersName->SetVisibility(State.Line.SpeakersName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

		SpeakersName->UpdatePlayback(NameState);
	}

	if (DialogueText)
	{
		DialogueText->UpdatePlayback(State);
	}
}

void UDialogueBubbleWidget::EndBubble(FGuid PlaybackId, EDialogueEndReason Reason)
{
	if (SpeakersName)
	{
		SpeakersName->EndPlayback(PlaybackId, Reason);
	}

	if (DialogueText)
	{
		DialogueText->EndPlayback(PlaybackId, Reason);
	}
}

void UDialogueBubbleWidget::SetBorderOpacity(float Opacity)
{
	FLinearColor Color = BorderColor;
	Color.A *= Opacity;
	if (DialogueBorder && DialogueBorder->GetBrushColor() != Color)
	{
		DialogueBorder->SetBrushColor(Color);
	}
}

void UDialogueBubbleWidget::OnDialogueViewModelSet_Implementation(UDialogueViewModel* ViewModel)
{
}
