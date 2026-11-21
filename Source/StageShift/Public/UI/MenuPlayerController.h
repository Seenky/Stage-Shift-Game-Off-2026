#pragma once

#include "GameFramework/PlayerController.h"
#include "MenuPlayerController.generated.h"

class AButton3D;

UCLASS()
class STAGESHIFT_API AMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void SetNavigationFocus(int32 Index);
	void MoveFocus(int32 Direction);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AButton3D>> Buttons;

	int32 FocusIndex = INDEX_NONE;
	bool bStickDeflected = false;
};
