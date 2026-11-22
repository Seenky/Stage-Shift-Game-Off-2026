#include "UI/MenuOverlayWidget.h"

#include "Algo/IndexOf.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Settings/StageAudioSettings.h"
#include "UI/MenuPlayerController.h"

#define LOCTEXT_NAMESPACE "StageShiftMenu"

namespace
{
	constexpr float FrameLimits[] = {0.f, 30.f, 60.f, 120.f};
}

void UMenuOverlayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	AudioTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowAudio);
	DisplayTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowDisplay);
	ControlsTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowControls);
	MasterSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::UpdateAudioLabels);
	MusicSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::UpdateAudioLabels);
	EffectsSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::UpdateAudioLabels);
	WindowModeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CycleWindowMode);
	VSyncButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleVSync);
	FrameLimitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CycleFrameLimit);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Close);
	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ApplyOrConfirm);
}

void UMenuOverlayWidget::ShowPanel(EMenuOverlay Panel)
{
	CurrentPanel = Panel;
	Panels->SetActiveWidgetIndex(Panel == EMenuOverlay::Settings ? 0 : 1);
	ConfirmButton->SetVisibility(Panel == EMenuOverlay::Coop ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	switch (Panel)
	{
	case EMenuOverlay::Settings:
		Title->SetText(LOCTEXT("Settings", "Настройки"));
		ConfirmLabel->SetText(LOCTEXT("Apply", "Применить"));
		if (const auto* Audio = GetGameInstance()->GetSubsystem<UStageAudioSettings>())
		{
			const auto& Volumes = Audio->GetVolumes();
			MasterSlider->SetValue(Volumes.Master);
			MusicSlider->SetValue(Volumes.Music);
			EffectsSlider->SetValue(Volumes.Effects);
		}
		if (const auto* Settings = UGameUserSettings::GetGameUserSettings())
		{
			WindowMode = Settings->GetFullscreenMode();
			bVSync = Settings->IsVSyncEnabled();
			FrameLimit = Settings->GetFrameRateLimit();
		}
		UpdateAudioLabels(0.f);
		UpdateDisplayLabels();
		ShowAudio();
		break;
	case EMenuOverlay::Coop:
		Title->SetText(LOCTEXT("Coop", "Кооператив"));
		Message->SetText(LOCTEXT("CoopUnavailable", "Кооперативный режим пока недоступен."));
		break;
	case EMenuOverlay::Exit:
		Title->SetText(LOCTEXT("ExitTitle", "Выйти из игры?"));
		Message->SetText(FText::GetEmpty());
		ConfirmLabel->SetText(LOCTEXT("Exit", "Выйти"));
		break;
	}
}

UButton* UMenuOverlayWidget::GetInitialFocus() const
{
	return CurrentPanel == EMenuOverlay::Settings ? AudioTab.Get() : BackButton.Get();
}

FReply UMenuOverlayWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (!Event.IsRepeat()) Close();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UMenuOverlayWidget::SelectSettingsPage(int32 Index)
{
	SettingsPages->SetActiveWidgetIndex(Index);

	UButton* Tabs[] = {AudioTab, DisplayTab, ControlsTab};
	for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(Tabs); ++TabIndex)
	{
		const bool bSelected = TabIndex == Index;
		Tabs[TabIndex]->SetBackgroundColor(bSelected ? SelectedTab : UnselectedTab);
		if (auto* Label = Cast<UTextBlock>(Tabs[TabIndex]->GetChildAt(0)))
		{
			Label->SetColorAndOpacity(bSelected ? SelectedTabText : UnselectedTabText);
		}
	}
}

void UMenuOverlayWidget::ShowAudio()
{
	SelectSettingsPage(0);
}

void UMenuOverlayWidget::ShowDisplay()
{
	SelectSettingsPage(1);
}

void UMenuOverlayWidget::ShowControls()
{
	SelectSettingsPage(2);
}

void UMenuOverlayWidget::UpdateAudioLabels(float)
{
	const auto Update = [](const USlider* Slider, UProgressBar* Progress, UTextBlock* Value)
	{
		const float Fraction = Slider->GetValue();
		Progress->SetPercent(Fraction);
		Value->SetText(FText::AsPercent(Fraction));
	};
	Update(MasterSlider, MasterProgress, MasterValue);
	Update(MusicSlider, MusicProgress, MusicValue);
	Update(EffectsSlider, EffectsProgress, EffectsValue);
}

void UMenuOverlayWidget::CycleWindowMode()
{
	switch (WindowMode)
	{
	case EWindowMode::Fullscreen:
		WindowMode = EWindowMode::WindowedFullscreen;
		break;
	case EWindowMode::WindowedFullscreen:
		WindowMode = EWindowMode::Windowed;
		break;
	default:
		WindowMode = EWindowMode::Fullscreen;
		break;
	}
	UpdateDisplayLabels();
}

void UMenuOverlayWidget::ToggleVSync()
{
	bVSync = !bVSync;
	UpdateDisplayLabels();
}

void UMenuOverlayWidget::CycleFrameLimit()
{
	const int32 Index = Algo::IndexOf(FrameLimits, FrameLimit);
	FrameLimit = FrameLimits[(Index + 1) % UE_ARRAY_COUNT(FrameLimits)];
	UpdateDisplayLabels();
}

void UMenuOverlayWidget::UpdateDisplayLabels()
{
	switch (WindowMode)
	{
	case EWindowMode::Fullscreen:
		WindowModeValue->SetText(LOCTEXT("Fullscreen", "Полный экран"));
		break;
	case EWindowMode::WindowedFullscreen:
		WindowModeValue->SetText(LOCTEXT("Borderless", "Без рамки"));
		break;
	default:
		WindowModeValue->SetText(LOCTEXT("Windowed", "В окне"));
		break;
	}
	VSyncValue->SetText(bVSync ? LOCTEXT("On", "Вкл.") : LOCTEXT("Off", "Выкл."));
	FrameLimitValue->SetText(FrameLimit > 0.f ? FText::AsNumber(FrameLimit) : LOCTEXT("Uncapped", "Без ограничения"));
}

void UMenuOverlayWidget::Close()
{
	if (auto* Controller = GetOwningPlayer<AMenuPlayerController>()) Controller->CloseOverlay();
}

void UMenuOverlayWidget::ApplyOrConfirm()
{
	if (CurrentPanel == EMenuOverlay::Exit)
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, true);
		return;
	}
	if (CurrentPanel != EMenuOverlay::Settings) return;

	if (auto* Audio = GetGameInstance()->GetSubsystem<UStageAudioSettings>())
	{
		Audio->SetVolumes({MasterSlider->GetValue(), MusicSlider->GetValue(), EffectsSlider->GetValue()});
	}
	if (auto* Settings = UGameUserSettings::GetGameUserSettings();
	    Settings && (Settings->GetFullscreenMode() != WindowMode || Settings->IsVSyncEnabled() != bVSync
	                 || !FMath::IsNearlyEqual(Settings->GetFrameRateLimit(), FrameLimit)))
	{
		Settings->SetFullscreenMode(WindowMode);
		Settings->SetVSyncEnabled(bVSync);
		Settings->SetFrameRateLimit(FrameLimit);
		Settings->ConfirmVideoMode();
		Settings->ApplySettings(false);
	}
	Close();
}

#undef LOCTEXT_NAMESPACE
