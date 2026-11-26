#pragma once

#include "Components/WidgetSwitcher.h"
#include "StageInputSwitcher.generated.h"

enum class EStageInputDevice : uint8;
class UStageInputDeviceSubsystem;

// Place keyboard/mouse, Xbox and PlayStation children in this order.
UCLASS(meta = (DisplayName = "Stage Input Switcher"))
class STAGESHIFT_API UStageInputSwitcher : public UWidgetSwitcher
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void ShowDevice(EStageInputDevice Device);
	TWeakObjectPtr<UStageInputDeviceSubsystem> InputSubsystem;
};
