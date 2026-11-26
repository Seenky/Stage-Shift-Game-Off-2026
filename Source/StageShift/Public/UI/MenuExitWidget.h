#pragma once

#include "UI/MenuPanelWidget.h"
#include "MenuExitWidget.generated.h"

UCLASS(Abstract)
class STAGESHIFT_API UMenuExitWidget : public UMenuPanelWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void ConfirmExit();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;
};
