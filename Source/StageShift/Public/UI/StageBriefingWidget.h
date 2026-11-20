#pragma once

#include "Blueprint/UserWidget.h"
#include "StageBriefingWidget.generated.h"

class AStageManager;
class UButton;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageBriefingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSceneBriefing(AStageManager* InManager, const FText& Text);
	UButton* GetContinueButton() const { return ContinueButton; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> BriefingText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ErrorText;

private:
	UFUNCTION() void HandleContinue();
	TWeakObjectPtr<AStageManager> Manager;
};
