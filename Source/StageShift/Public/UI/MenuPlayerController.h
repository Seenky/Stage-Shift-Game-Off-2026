#pragma once

#include "GameFramework/PlayerController.h"
#include "MenuPlayerController.generated.h"

class AButton3D;
class UMenuOverlayWidget;
enum class EMenuOverlay : uint8;

UCLASS()
class STAGESHIFT_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void OpenOverlay(EMenuOverlay Panel, AButton3D* Origin);
	void CloseOverlay();

protected:
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void SetNavigationFocus(int32 Index, bool bPlaySound = true);
	void MoveFocus(int32 Direction);

	UPROPERTY(EditDefaultsOnly, Category = "Menu")
	TSubclassOf<UMenuOverlayWidget> OverlayClass;

	UPROPERTY(Transient)
	TObjectPtr<UMenuOverlayWidget> ActiveOverlay;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AButton3D>> Buttons;

	int32 FocusIndex = INDEX_NONE;
	bool bStickDeflected = false;
	bool bSavedClickEvents = false;
	bool bSavedMouseOverEvents = false;
};
