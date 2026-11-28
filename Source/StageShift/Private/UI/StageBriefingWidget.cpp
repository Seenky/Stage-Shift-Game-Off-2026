#include "UI/StageBriefingWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "UI/StageFocusableControls.h"

void UStageBriefingPropWidget::SetProp(const FStageBriefingProp& Prop)
{
	PropName->SetText(Prop.Name);
	PropIcon->SetBrushFromTexture(Prop.Icon);
}

void UStageBriefingWidget::SetSceneBriefing(AStageManager* InManager, const FText& Text, TConstArrayView<FStageBriefingProp> Props)
{
	Manager = InManager;
	if (auto* RichText = Cast<URichTextBlock>(BriefingText))
	{
		RichText->SetText(Text);
	}
	else if (auto* PlainText = Cast<UTextBlock>(BriefingText))
	{
		FString DisplayText = Text.ToString();
		const TCHAR* CueTags[] = { TEXT("<Light>"), TEXT("<Sound>"), TEXT("<Props>"), TEXT("</>"),
		    TEXT(" (<img id=\"Knock\"/>)"), TEXT(" (<img id=\"Creak\"/>)"), TEXT(" (<img id=\"Impact\"/>)") };
		for (const TCHAR* Tag : CueTags)
			DisplayText.ReplaceInline(Tag, TEXT(""), ESearchCase::CaseSensitive);
		PlainText->SetText(DisplayText == Text.ToString() ? Text : FText::FromString(MoveTemp(DisplayText)));
	}
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	SetProps(Props);
}

void UStageBriefingWidget::SetProps(TConstArrayView<FStageBriefingProp> Props)
{
	if (!PropInventory || !PropCards) return;
	PropCards->ClearChildren();
	if (PropCardClass)
		for (const FStageBriefingProp& Prop : Props)
		{
			if (!Prop.Icon || Prop.Name.IsEmpty()) continue;
			if (auto* Card = CreateWidget<UStageBriefingPropWidget>(GetOwningPlayer(), PropCardClass))
			{
				Card->SetProp(Prop);
				PropCards->AddChild(Card);
			}
		}
	PropInventory->SetVisibility(PropCards->HasAnyChildren() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UStageBriefingWidget::SetProductionResult(AStageManager* InManager, const FText& Text, int32 Score, int32 Stars, int32 TotalStars)
{
	SetSceneBriefing(InManager, Text);
	if (auto* Header = Cast<UTextBlock>(GetWidgetFromName(TEXT("Header"))))
		Header->SetText(NSLOCTEXT("StageBriefing", "ResultHeader", "Итог спектакля"));
	OnProductionResultPresented(Score, Stars, TotalStars);
}

void UStageBriefingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* Button = Cast<UStageFocusButton>(ContinueButton); Button && ContinueSound) Button->SetClickSound(nullptr);
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
		if (Stage->ContinueSceneBriefing(Error))
		{
			if (ContinueSound) UGameplayStatics::PlaySound2D(Stage, ContinueSound);
			return;
		}
		ErrorText->SetText(Error);
		ErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
