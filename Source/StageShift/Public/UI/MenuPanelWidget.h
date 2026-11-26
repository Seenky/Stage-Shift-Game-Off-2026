#pragma once

#include "Blueprint/UserWidget.h"
#include "MenuPanelWidget.generated.h"

class UButton;

UCLASS(Abstract)
class STAGESHIFT_API UMenuPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnCloseRequested;

	virtual UWidget* GetInitialFocus() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	UFUNCTION()
	void RequestClose();

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;
};
