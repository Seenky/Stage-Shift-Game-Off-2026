#include "UI/StageSelectionWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Stage/Environment/StageSelectionStation.h"
#include "UI/StageFocusableControls.h"

void UStageSelectionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	// The station owns browse, confirm and back sounds, including keyboard shortcuts.
	for (UButton* Control : {PreviousButton.Get(), NextButton.Get(), StartButton.Get(), BackButton.Get()})
	{
		if (auto* Button = Cast<UStageFocusButton>(Control)) Button->SetClickSound(nullptr);
	}
	PreviousButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Previous);
	NextButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Next);
	StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Start);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Back);
}

void UStageSelectionWidget::SetStation(AStageSelectionStation* InStation)
{
	Station = InStation;
}

void UStageSelectionWidget::ShowOption(const FStageSelectionOption& Option, bool bHasOtherOptions, bool bSliding)
{
	TitleText->SetText(Option.Title);
	StatusText->SetVisibility(Option.CanStart() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	StatusText->SetText(NSLOCTEXT("StageSelection", "Locked", "Закрыто"));
	StartButton->SetIsEnabled(Option.CanStart() && !bSliding);
	PreviousButton->SetIsEnabled(bHasOtherOptions);
	NextButton->SetIsEnabled(bHasOtherOptions);
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	if (bSliding) OnSelectionPreviewChanged();
}

void UStageSelectionWidget::FocusSelection()
{
	SetUserFocus(GetOwningPlayer());
}

void UStageSelectionWidget::Previous()
{
	if (auto* Owner = Station.Get()) Owner->BrowseStage(-1);
}

void UStageSelectionWidget::Next()
{
	if (auto* Owner = Station.Get()) Owner->BrowseStage(1);
}

void UStageSelectionWidget::Start()
{
	if (!StartButton->GetIsEnabled()) return;
	if (auto* Owner = Station.Get(); Owner && Owner->TryStartStage()) return;

	ErrorText->SetText(NSLOCTEXT("StageSelection", "Unavailable", "Постановка недоступна"));
	ErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UStageSelectionWidget::Back()
{
	if (auto* Owner = Station.Get()) Owner->CloseSelection();
}

FReply UStageSelectionWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		Back();
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		if (!Event.IsRepeat()) Previous();
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		if (!Event.IsRepeat()) Next();
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_FaceButton_Bottom || (Key == EKeys::Enter && HasUserFocus(GetOwningPlayer())))
	{
		if (!Event.IsRepeat()) Start();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UStageSelectionWidget::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
	if (Event.GetKey() != EKeys::Gamepad_LeftX)
	{
		return Super::NativeOnAnalogValueChanged(Geometry, Event);
	}

	const float Axis = Event.GetAnalogValue();
	if (FMath::Abs(Axis) < .35f) bStickDeflected = false;
	else if (FMath::Abs(Axis) >= .65f && !bStickDeflected)
	{
		bStickDeflected = true;
		if (Axis > 0.f) Next();
		else Previous();
	}
	return FReply::Handled();
}
