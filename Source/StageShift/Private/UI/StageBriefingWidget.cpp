#include "UI/StageBriefingWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Stage/Core/StageManager.h"

void UStageBriefingWidget::SetSceneBriefing(AStageManager* InManager, const FText& Text)
{
	Manager = InManager;
	BriefingText->SetText(Text);
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
}

void UStageBriefingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ContinueButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleContinue);
}

void UStageBriefingWidget::NativeDestruct()
{
	ContinueButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleContinue);
	Super::NativeDestruct();
}

void UStageBriefingWidget::HandleContinue()
{
	if (auto* Stage = Manager.Get())
	{
		FText Error;
		if (Stage->ContinueSceneBriefing(Error)) return;
		ErrorText->SetText(Error);
		ErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
