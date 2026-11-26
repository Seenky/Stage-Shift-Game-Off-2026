#include "UI/SettingsWidget.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "UI/SettingsAudioWidget.h"
#include "UI/SettingsDisplayWidget.h"

void USettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	AudioTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAudio);
	DisplayTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowDisplay);
	ControlsTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowControls);
	ApplyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Apply);
}

void USettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	auto* Audio = Cast<USettingsAudioWidget>(SettingsPages->GetChildAt(static_cast<int32>(EMenuSettingsTab::Audio)));
	auto* Display = Cast<USettingsDisplayWidget>(SettingsPages->GetChildAt(static_cast<int32>(EMenuSettingsTab::Display)));
	const bool bHasSettingsPages = Audio && Display && SettingsPages->GetChildAt(static_cast<int32>(EMenuSettingsTab::Controls));
	if (ensureMsgf(bHasSettingsPages, TEXT("SettingsPages must contain Audio, Display and Controls in that order.")))
	{
		Audio->LoadSettings();
		Display->LoadSettings();
	}
	ApplyButton->SetIsEnabled(bHasSettingsPages);
	ShowAudio();
}

UWidget* USettingsWidget::GetInitialFocus() const
{
	return AudioTab;
}

void USettingsWidget::SelectTab(EMenuSettingsTab Tab)
{
	SettingsPages->SetActiveWidgetIndex(static_cast<int32>(Tab));
	OnSettingsTabChanged(Tab);
}

void USettingsWidget::ShowAudio()
{
	SelectTab(EMenuSettingsTab::Audio);
}

void USettingsWidget::ShowDisplay()
{
	SelectTab(EMenuSettingsTab::Display);
}

void USettingsWidget::ShowControls()
{
	SelectTab(EMenuSettingsTab::Controls);
}

void USettingsWidget::Apply()
{
	auto* Audio = Cast<USettingsAudioWidget>(SettingsPages->GetChildAt(static_cast<int32>(EMenuSettingsTab::Audio)));
	auto* Display = Cast<USettingsDisplayWidget>(SettingsPages->GetChildAt(static_cast<int32>(EMenuSettingsTab::Display)));
	if (!Audio || !Display) return;

	Audio->ApplySettings();
	Display->ApplySettings();
	RequestClose();
}
