#pragma once

#include "GameFramework/PlayerController.h"
#include "UI/MenuPanel.h"
#include "MenuPlayerController.generated.h"

class AButton3D;
class UMenuExitWidget;
class UMenuPanelWidget;
class USettingsWidget;

UCLASS()
class STAGESHIFT_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void OpenOverlay(EMenuOverlay Panel, AButton3D* Origin);

protected:
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void CloseOverlay();
	void SetNavigationFocus(int32 Index, bool bPlaySound = true);
	void MoveFocus(int32 Direction);

	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	TSubclassOf<USettingsWidget> SettingsClass;

	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	TSubclassOf<UMenuPanelWidget> CoopClass;

	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	TSubclassOf<UMenuExitWidget> ExitClass;

	UPROPERTY(Transient)
	TObjectPtr<UMenuPanelWidget> ActiveOverlay;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AButton3D>> Buttons;

	int32 FocusIndex = INDEX_NONE;
	bool bStickDeflected = false;
	bool bSavedClickEvents = false;
	bool bSavedMouseOverEvents = false;
};
