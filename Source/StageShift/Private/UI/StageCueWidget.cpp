#include "UI/StageCueWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Stage/Cues/StageCueComponent.h"
#include "Stage/Cues/StageCueWorldMarker.h"

#define LOCTEXT_NAMESPACE "StageCues"

void UStageCueSignalWidget::SetSignalCaption(const FText& Caption)
{
	if (!SignalCaption) return;
	SignalCaption->SetText(Caption);
	SignalCaption->SetVisibility(Caption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UStageCueCardWidget::Present(const FStageCueView& View)
{
	const bool bCompleted = View.State == EStageCueState::Completed;
	const bool bProblem = View.State == EStageCueState::Problem;
	const bool bUpcoming = View.State == EStageCueState::Upcoming;
	const FLinearColor Color = bCompleted ? CompletedColor : bProblem ? ProblemColor : bUpcoming ? UpcomingColor : TaskColor;
	Accent->SetBrushColor(Color);
	NumberText->SetText(FText::AsNumber(View.Number));
	StatusText->SetText(bCompleted ? LOCTEXT("Done", "ГОТОВО") : bProblem ? LOCTEXT("Problem", "НУЖНО ИСПРАВИТЬ") : bUpcoming ? LOCTEXT("Next", "СКОРО") : LOCTEXT("Now", "СЕЙЧАС"));
	StatusText->SetColorAndOpacity(FSlateColor(Color));
	TitleText->SetText(View.Title);
	DetailText->SetText(View.Details);
	DetailText->SetVisibility(View.Details.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	const bool bTimed = View.bHasDeadline && !bCompleted;
	TimeBar->SetVisibility(bTimed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	TimeText->SetVisibility(bTimed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	TimeBar->SetPercent(View.TimeFraction);
	TimeBar->SetFillColorAndOpacity(Color);
	if (bTimed)
	{
		const int32 Seconds = FMath::CeilToInt(FMath::Abs(View.SecondsRemaining));
		TimeText->SetText(FText::Format(View.bOverdue ? LOCTEXT("Late", "+{0} с") : LOCTEXT("Remaining", "{0} с"), FText::AsNumber(Seconds)));
		TimeText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UStageCueMarkerWidget::Present(const AStageCueWorldMarker* Marker)
{
	Badge->SetBrushColor(Marker->Tint);
	NumberText->SetText(FText::AsNumber(Marker->CueView.Number));
	KindText->SetText(Marker->MarkerType == EStageCueMarkerType::Zone ? LOCTEXT("Zone", "СЮДА") : Marker->MarkerType == EStageCueMarkerType::Event ? LOCTEXT("Event", "!")
	    : Marker->MarkerType == EStageCueMarkerType::Recipient ? LOCTEXT("Recipient", "ПЕРЕДАТЬ") : LOCTEXT("Object", "ВЗЯТЬ"));
}

void UStageCueHUDWidget::SetSource(UStageCueComponent* Component)
{
	if (Source.IsValid()) Source->OnCuesChanged.Remove(ChangedHandle);
	Source = Component;
	if (Component) ChangedHandle = Component->OnCuesChanged.AddUObject(this, &ThisClass::Refresh);
	if (GetCachedWidget().IsValid()) Refresh();
}

void UStageCueHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Refresh();
}

void UStageCueHUDWidget::NativeDestruct()
{
	SetSource(nullptr);
	Super::NativeDestruct();
}

void UStageCueHUDWidget::Refresh()
{
	if (!Cards || !MarkerCanvas) return;
	const auto Views = Source.IsValid() ? Source->GetVisibleCues() : TArray<FStageCueView>();
	CardArea->SetVisibility(Views.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	while (CardWidgets.Num() < Views.Num() && CardClass)
	{
		auto* Card = CreateWidget<UStageCueCardWidget>(GetOwningPlayer(), CardClass);
		if (!Card) break;
		Cards->AddChildToVerticalBox(Card);
		CardWidgets.Add(Card);
	}
	for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
	{
		CardWidgets[Index]->SetVisibility(Views.IsValidIndex(Index) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Views.IsValidIndex(Index)) CardWidgets[Index]->Present(Views[Index]);
	}
	const auto VisibleMarkers = Source.IsValid() ? Source->GetVisibleMarkers() : TArray<AStageCueWorldMarker*>();
	for (auto It = MarkerWidgets.CreateIterator(); It; ++It)
	{
		if (!VisibleMarkers.Contains(It.Key()))
		{
			if (It.Value()) It.Value()->RemoveFromParent();
			It.RemoveCurrent();
		}
	}
	if (!MarkerWidgetClass) return;
	for (AStageCueWorldMarker* Marker : VisibleMarkers)
	{
		auto& Widget = MarkerWidgets.FindOrAdd(Marker);
		if (!Widget)
		{
			Widget = CreateWidget<UStageCueMarkerWidget>(GetOwningPlayer(), MarkerWidgetClass);
			if (!Widget) continue;
			auto* MarkerSlot = MarkerCanvas->AddChildToCanvas(Widget);
			MarkerSlot->SetAutoSize(true);
			MarkerSlot->SetAlignment(FVector2D(0.5, 1.0));
		}
		Widget->Present(Marker);
	}
}

void UStageCueHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	for (const auto& Pair : MarkerWidgets)
	{
		if (!IsValid(Pair.Key) || !Pair.Value) continue;
		FVector2D Position = FVector2D::ZeroVector;
		const bool bVisible = Pair.Key->HasValidTarget()
		    && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Pair.Key->GetIndicatorLocation(), Position, true);
		if (Pair.Key->MarkerType == EStageCueMarkerType::Recipient || Pair.Key->MarkerType == EStageCueMarkerType::Event) Position += FVector2D(52.0, -6.0);
		// Use the owning player's geometry and DPI, including split-screen viewport offsets.
		const FVector2D Size = Geometry.GetLocalSize();
		const bool bInside = bVisible && Position.X >= 16.0 && Position.Y >= 16.0 && Position.X <= Size.X - 16.0 && Position.Y <= Size.Y - 16.0;
		Pair.Value->SetVisibility(bInside ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bInside)
			if (auto* MarkerSlot = Cast<UCanvasPanelSlot>(Pair.Value->Slot)) MarkerSlot->SetPosition(Position);
	}
}

#undef LOCTEXT_NAMESPACE
