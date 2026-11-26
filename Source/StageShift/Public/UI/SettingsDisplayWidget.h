#pragma once

#include "Blueprint/UserWidget.h"
#include "GenericPlatform/GenericWindow.h"
#include "SettingsDisplayWidget.generated.h"

class UButton;

UCLASS(Abstract)
class STAGESHIFT_API USettingsDisplayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void LoadSettings();
	void ApplySettings() const;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Settings", meta = (DisplayName = "On Display Values Changed"))
	void OnDisplayValuesChanged(EWindowMode::Type Mode, bool bVSyncEnabled, float Limit);

private:
	void RefreshDisplay();

	UFUNCTION()
	void CycleWindowMode();

	UFUNCTION()
	void ToggleVSync();

	UFUNCTION()
	void CycleFrameLimit();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WindowModeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> VSyncButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FrameLimitButton;

	EWindowMode::Type WindowMode = EWindowMode::WindowedFullscreen;
	float FrameLimit = 0.f;
	bool bVSync = false;
};
