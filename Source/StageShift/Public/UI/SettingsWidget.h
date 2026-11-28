#pragma once

#include "UI/MenuPanelWidget.h"
#include "SettingsWidget.generated.h"

class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EMenuSettingsTab : uint8
{
	Audio,
	Display,
	Controls
};

UCLASS(Abstract)
class STAGESHIFT_API USettingsWidget : public UMenuPanelWidget
{
	GENERATED_BODY()

public:
	virtual UWidget* GetInitialFocus() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Settings", meta = (DisplayName = "On Settings Tab Changed"))
	void OnSettingsTabChanged(EMenuSettingsTab Tab);

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidget))
	TObjectPtr<UButton> AudioTab;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidget))
	TObjectPtr<UButton> DisplayTab;

	UPROPERTY(BlueprintReadOnly, Category = "Settings", meta = (BindWidget))
	TObjectPtr<UButton> ControlsTab;

private:
	void SelectTab(EMenuSettingsTab Tab);

	UFUNCTION()
	void ShowAudio();

	UFUNCTION()
	void ShowDisplay();

	UFUNCTION()
	void ShowControls();

	UFUNCTION()
	void Apply();

	// Direct children in Audio, Display, Controls order.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> SettingsPages;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ApplyButton;
};
