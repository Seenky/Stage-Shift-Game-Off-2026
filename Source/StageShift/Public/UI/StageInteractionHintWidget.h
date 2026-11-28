#pragma once

#include "Blueprint/UserWidget.h"
#include "StageInteractionHintWidget.generated.h"

class UWidgetSwitcher;

UCLASS(Abstract)
class STAGESHIFT_API UStageInteractionHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Present(bool bSoundSelection);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Hints;
};
