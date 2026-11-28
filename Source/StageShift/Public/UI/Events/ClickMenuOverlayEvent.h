#pragma once

#include "UI/MenuPanel.h"
#include "UI/UIClickEventBase.h"
#include "ClickMenuOverlayEvent.generated.h"

UCLASS()
class STAGESHIFT_API UClickMenuOverlayEvent : public UUIClickEventBase
{
	GENERATED_BODY()

public:
	virtual void OnClicked_Implementation() override;

private:
	UPROPERTY(EditAnywhere, Category = "Menu")
	EMenuOverlay Panel = EMenuOverlay::Settings;
};
