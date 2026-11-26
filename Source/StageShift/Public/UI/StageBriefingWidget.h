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
	void SetProductionResult(AStageManager* InManager, const FText& Text, int32 Score, int32 Stars, int32 TotalStars);
	TSubclassOf<UStageBriefingWidget> GetProductionResultClass() const { return ProductionResultClass; }
	UButton* GetContinueButton() const { return ContinueButton; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Appearance")
	void OnProductionResultPresented(int32 Score, int32 Stars, int32 TotalStars);

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	TSubclassOf<UStageBriefingWidget> ProductionResultClass;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextLayoutWidget> BriefingText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> ContinueSound;

private:
	UFUNCTION() void HandleContinue();
	TWeakObjectPtr<AStageManager> Manager;
};
