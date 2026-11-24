#pragma once

#include "Blueprint/UserWidget.h"
#include "StageBriefingWidget.generated.h"

class AStageManager;
class UButton;
class USoundBase;
class UTextBlock;
class UTextLayoutWidget;

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageBriefingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSceneBriefing(AStageManager* InManager, const FText& Text);
	void SetProductionResult(AStageManager* InManager, const FText& Text);
	UButton* GetContinueButton() const { return ContinueButton; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextLayoutWidget> BriefingText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> ContinueSound;

private:
	UFUNCTION() void HandleContinue();
	TWeakObjectPtr<AStageManager> Manager;
};
