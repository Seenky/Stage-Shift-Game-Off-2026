#pragma once

#include "Blueprint/UserWidget.h"
#include "SettingsAudioWidget.generated.h"

class USlider;

UCLASS(Abstract)
class STAGESHIFT_API USettingsAudioWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void LoadSettings();
	void ApplySettings() const;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Settings", meta = (DisplayName = "On Volume Values Changed"))
	void OnVolumeValuesChanged(float Master, float Music, float Effects);

private:
	UFUNCTION()
	void HandleSliderChanged(float Value);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> EffectsSlider;
};
