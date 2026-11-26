#include "UI/SettingsDisplayWidget.h"

#include "Algo/IndexOf.h"
#include "Components/Button.h"
#include "GameFramework/GameUserSettings.h"

namespace
{
	constexpr float FrameLimits[] = {0.f, 30.f, 60.f, 120.f};
}

void USettingsDisplayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	WindowModeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CycleWindowMode);
	VSyncButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ToggleVSync);
	FrameLimitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CycleFrameLimit);
}

void USettingsDisplayWidget::LoadSettings()
{
	if (const auto* Settings = UGameUserSettings::GetGameUserSettings())
	{
		WindowMode = Settings->GetFullscreenMode();
		bVSync = Settings->IsVSyncEnabled();
		FrameLimit = Settings->GetFrameRateLimit();
	}
	RefreshDisplay();
}

void USettingsDisplayWidget::ApplySettings() const
{
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
}

void USettingsDisplayWidget::RefreshDisplay()
{
	OnDisplayValuesChanged(WindowMode, bVSync, FrameLimit);
}

void USettingsDisplayWidget::CycleWindowMode()
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
	RefreshDisplay();
}

void USettingsDisplayWidget::ToggleVSync()
{
	bVSync = !bVSync;
	RefreshDisplay();
}

void USettingsDisplayWidget::CycleFrameLimit()
{
	const int32 Index = Algo::IndexOf(FrameLimits, FrameLimit);
	FrameLimit = FrameLimits[(Index + 1) % UE_ARRAY_COUNT(FrameLimits)];
	RefreshDisplay();
}
