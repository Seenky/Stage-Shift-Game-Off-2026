#include "UI/SettingsAudioWidget.h"

#include "Components/Slider.h"
#include "Engine/GameInstance.h"
#include "Settings/StageAudioSettings.h"

void USettingsAudioWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	MasterSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderChanged);
	MusicSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderChanged);
	EffectsSlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleSliderChanged);
}

void USettingsAudioWidget::LoadSettings()
{
	if (const auto* Audio = UGameInstance::GetSubsystem<UStageAudioSettings>(GetGameInstance()))
	{
		const FStageAudioVolumes& Volumes = Audio->GetVolumes();
		MasterSlider->SetValue(Volumes.Master);
		MusicSlider->SetValue(Volumes.Music);
		EffectsSlider->SetValue(Volumes.Effects);
	}
	OnVolumeValuesChanged(MasterSlider->GetValue(), MusicSlider->GetValue(), EffectsSlider->GetValue());
}

void USettingsAudioWidget::ApplySettings() const
{
	if (auto* Audio = UGameInstance::GetSubsystem<UStageAudioSettings>(GetGameInstance()))
	{
		Audio->SetVolumes({MasterSlider->GetValue(), MusicSlider->GetValue(), EffectsSlider->GetValue()});
	}
}

void USettingsAudioWidget::HandleSliderChanged(float)
{
	OnVolumeValuesChanged(MasterSlider->GetValue(), MusicSlider->GetValue(), EffectsSlider->GetValue());
}
