#pragma once

#include "Components/Button.h"
#include "Components/Slider.h"
#include "StageFocusableControls.generated.h"

class USoundBase;

UCLASS(meta = (DisplayName = "Stage Focus Button"))
class STAGESHIFT_API UStageFocusButton : public UButton
{
	GENERATED_BODY()

public:
	UStageFocusButton();
	void PlayFocusSound() const;
	void SetClickSound(USoundBase* Sound);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FReply HandleFeedbackClicked();
	void HandleFeedbackHovered();

	UPROPERTY(EditAnywhere, Category = "Audio")
	TObjectPtr<USoundBase> FocusSound;
	UPROPERTY(EditAnywhere, Category = "Audio")
	TObjectPtr<USoundBase> ClickSound;
};

UCLASS(meta = (DisplayName = "Stage Focus Slider"))
class STAGESHIFT_API UStageFocusSlider : public USlider
{
	GENERATED_BODY()

public:
	UStageFocusSlider();
	void PlayFocusSound() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void HandleFeedbackValueChanged(float NewValue);

	UPROPERTY(EditAnywhere, Category = "Audio")
	TObjectPtr<USoundBase> FeedbackSound;
	double LastValueSoundTime = -1.;
};
