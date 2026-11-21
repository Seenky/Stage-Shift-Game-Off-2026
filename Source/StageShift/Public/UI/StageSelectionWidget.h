#pragma once

#include "Blueprint/UserWidget.h"
#include "StageSelectionWidget.generated.h"

class AStageSelectionStation;
class UButton;
class UTextBlock;
struct FStageSelectionOption;

UCLASS(Abstract)
class STAGESHIFT_API UStageSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStation(AStageSelectionStation* InStation);
	void ShowOption(const FStageSelectionOption& Option, bool bHasOtherOptions, bool bSliding);
	void FocusSelection();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PreviousButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NextButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ErrorText;

private:
	UFUNCTION()
	void Previous();

	UFUNCTION()
	void Next();

	UFUNCTION()
	void Start();

	UFUNCTION()
	void Back();

	TWeakObjectPtr<AStageSelectionStation> Station;
	bool bStickDeflected = false;
};
