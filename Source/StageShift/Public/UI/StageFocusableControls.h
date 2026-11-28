#pragma once

#include "Components/Button.h"
#include "Components/Slider.h"
#include "StageFocusableControls.generated.h"

UCLASS(meta = (DisplayName = "Stage Focus Button"))
class STAGESHIFT_API UStageFocusButton : public UButton
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS(meta = (DisplayName = "Stage Focus Slider"))
class STAGESHIFT_API UStageFocusSlider : public USlider
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
