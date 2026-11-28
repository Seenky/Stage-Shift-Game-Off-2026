#pragma once

#include "Blueprint/UserWidget.h"
#include "MenuPanelWidget.generated.h"

class UButton;
class USoundBase;

UCLASS(Abstract)
class STAGESHIFT_API UMenuPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMenuPanelWidget(const FObjectInitializer& ObjectInitializer);

	FSimpleMulticastDelegate OnCloseRequested;

	virtual UWidget* GetInitialFocus() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	UFUNCTION()
	void RequestClose();

private:
	UFUNCTION()
	void HandleBack();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> BackSound;
};
