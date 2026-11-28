#pragma once

#include "Blueprint/UserWidget.h"
#include "StageBriefingWidget.generated.h"

class AStageManager;
class UButton;
class UImage;
class UPanelWidget;
class USoundBase;
class UTextBlock;
class UTextLayoutWidget;
struct FStageBriefingProp;

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageBriefingPropWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetProp(const FStageBriefingProp& Prop);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PropIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PropName;
};

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageBriefingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSceneBriefing(AStageManager* InManager, const FText& Text, TConstArrayView<FStageBriefingProp> Props = {});
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
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PropInventory;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> PropCards;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TSubclassOf<UStageBriefingPropWidget> PropCardClass;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> ContinueSound;

private:
	void SetProps(TConstArrayView<FStageBriefingProp> Props);
	UFUNCTION() void HandleContinue();
	TWeakObjectPtr<AStageManager> Manager;
};
