#include "Components/Dialogue/DialogueTextWidget.h"

#include "SDialogueText.h"
#include "Styling/CoreStyle.h"

UDialogueTextWidget::UDialogueTextWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
	, PreviewText(NSLOCTEXT("StageShiftDialogue", "BubblePreview", "I've got a plan."))
{
}

TSharedRef<SWidget> UDialogueTextWidget::RebuildWidget()
{
	SAssignNew(Text, SDialogueText).OnOpacityChanged(FOnDialogueTextOpacityChanged::CreateUObject(this, &ThisClass::ForwardOpacity));
	Text->SetShowRevealDots(bShowRevealDots);
	return Text.ToSharedRef();
}

void UDialogueTextWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!Text)
	{
		return;
	}
	Text->SetShowRevealDots(bShowRevealDots);
	Text->SetAppearance(Font, TextColor, TextWrapWidth, Justification.GetValue());
	if (IsDesignTime())
	{
		Text->Preview(PreviewText);
	}
	else if (CurrentState.PlaybackId.IsValid())
	{
		Text->Update(CurrentState);
	}
}

void UDialogueTextWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Text.Reset();
}

void UDialogueTextWidget::ForwardOpacity(float Opacity)
{
	OnOpacityChanged.ExecuteIfBound(Opacity);
}

void UDialogueTextWidget::UpdatePlayback(const FDialoguePlaybackState& State)
{
	CurrentState = State;
	if (Text)
	{
		Text->Update(State);
	}
}

void UDialogueTextWidget::EndPlayback(FGuid PlaybackId, EDialogueEndReason Reason)
{
	if (CurrentState.PlaybackId != PlaybackId)
	{
		return;
	}
	CurrentState = {};
	if (Text)
	{
		Text->End(Reason);
	}
}

void UDialogueTextWidget::ResetPlayback()
{
	CurrentState = {};
	if (Text)
	{
		Text->Reset();
	}
}

void UDialogueTextWidget::SetShowRevealDots(bool bShowDots)
{
    bShowRevealDots = bShowDots;

    if (Text)
    {
        Text->SetShowRevealDots(bShowDots);
    }
}
