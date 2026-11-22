#pragma once

#include "Blueprint/UserWidget.h"
#include "GenericPlatform/GenericWindow.h"
#include "MenuOverlayWidget.generated.h"

class UButton;
class UProgressBar;
class USlider;
class UTextBlock;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EMenuOverlay : uint8
{
	Settings,
	Coop,
	Exit
};

UCLASS(Abstract)
class STAGESHIFT_API UMenuOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowPanel(EMenuOverlay Panel);
	UButton* GetInitialFocus() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	void SelectSettingsPage(int32 Index);
	void UpdateDisplayLabels();

	UFUNCTION()
	void ShowAudio();

	UFUNCTION()
	void ShowDisplay();

	UFUNCTION()
	void ShowControls();

	UFUNCTION()
	void UpdateAudioLabels(float Value);

	UFUNCTION()
	void CycleWindowMode();

	UFUNCTION()
	void ToggleVSync();

	UFUNCTION()
	void CycleFrameLimit();

	UFUNCTION()
	void Close();

	UFUNCTION()
	void ApplyOrConfirm();

	UPROPERTY(EditDefaultsOnly, Category = "Style|Tabs")
	FLinearColor SelectedTab = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Tabs")
	FLinearColor UnselectedTab = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Tabs")
	FLinearColor SelectedTabText = FLinearColor::Black;

	UPROPERTY(EditDefaultsOnly, Category = "Style|Tabs")
	FLinearColor UnselectedTabText = FLinearColor::White;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Panels;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> SettingsPages;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AudioTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DisplayTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ControlsTab;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> MasterSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> EffectsSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> MasterProgress;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> MusicProgress;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> EffectsProgress;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MasterValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MusicValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EffectsValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> WindowModeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> VSyncButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> FrameLimitButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WindowModeValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VSyncValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FrameLimitValue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Message;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConfirmLabel;

	EMenuOverlay CurrentPanel = EMenuOverlay::Settings;
	EWindowMode::Type WindowMode = EWindowMode::WindowedFullscreen;
	float FrameLimit = 0.f;
	bool bVSync = false;
};
