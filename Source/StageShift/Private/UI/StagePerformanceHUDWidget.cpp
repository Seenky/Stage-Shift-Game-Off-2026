#include "UI/StagePerformanceHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Stage/Core/StageManager.h"

namespace
{
constexpr float FeedbackFadeTime = 0.25f;

void UpdateFeedback(UWidget& Widget, double Now, double Until)
{
	Widget.SetVisibility(Now < Until ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Now < Until) Widget.SetRenderOpacity(FMath::Min(static_cast<float>(Until - Now) / FeedbackFadeTime, 1.0f));
}
}

void UStagePerformanceTagWidget::Present(const FStagePerformanceCue& Cue)
{
	if (LastNumber != Cue.Number)
	{
		LastNumber = Cue.Number;
		ActorNumber->SetText(Cue.Number > 0 ? FText::AsNumber(Cue.Number) : FText::GetEmpty());
		ActorNumber->SetVisibility(Cue.Number > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (NumberBadge) NumberBadge->SetVisibility(ActorNumber->GetVisibility());
	}
	if (CueIcon && CueIcon->GetBrush().GetResourceObject() != Cue.Icon) CueIcon->SetBrushFromTexture(Cue.Icon);
	if (!CueState) return;
	CueState->SetVisibility(Cue.Phase == EStageCueWindowPhase::Hidden ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Cue.Phase == EStageCueWindowPhase::Open) CueState->SetColorAndOpacity(OpenColor);
	else if (Cue.Phase == EStageCueWindowPhase::Closed) CueState->SetColorAndOpacity(ClosedColor);
	else CueState->SetColorAndOpacity(WaitingColor);
}

void UStagePerformanceHUDWidget::SetSource(AStageManager& Stage)
{
	Source = &Stage;
	Stage.OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	Stage.OnOpeningOverdue.AddUniqueDynamic(this, &ThisClass::HandleOpeningOverdue);
}

void UStagePerformanceHUDWidget::Refresh(const AStageManager& Stage, TConstArrayView<FStagePerformanceCue> Cues, TConstArrayView<FName> CastOrder)
{
	const EStageProductionState State = Stage.GetProductionState();
	const bool bVisible = State == EStageProductionState::Preparing || State == EStageProductionState::Playing
	    || Stage.GetState() != EStageState::Idle;
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		DeltaUntil = AudienceUntil = -1.0;
		LastScoreAt = -10.0;
		return;
	}

	const int32 SceneCount = FMath::Max(1, Stage.GetProductionSceneCount());
	const int32 Scene = FMath::Clamp(Stage.GetProductionSceneIndex() + 1, 1, SceneCount);
	if (LastScene != Scene || LastSceneCount != SceneCount)
	{
		LastScene = Scene;
		LastSceneCount = SceneCount;
		SceneText->SetText(FText::Format(NSLOCTEXT("StagePerformance", "SceneIndex", "Сцена {0} / {1}"),
		    FText::AsNumber(Scene), FText::AsNumber(SceneCount)));
	}
	const int32 Score = Stage.GetScore();
	if (LastStars == INDEX_NONE || LastScore != Score)
	{
		LastScore = Score;
		ScoreText->SetText(FText::AsNumber(Score));
	}
	const int32 Stars = Stage.GetStarCount();
	if (LastStars != Stars)
	{
		LastStars = Stars;
		UImage* StarImages[] = {StarOne, StarTwo, StarThree};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(StarImages); ++Index)
			StarImages[Index]->SetBrushFromTexture(Index < Stars ? StarFilled.Get() : StarEmpty.Get());
	}

	TaggedActors.Reset();
	CastMembers.Reset();
	if (!Cues.IsEmpty())
		for (FName Role : CastOrder)
			if (AActor* Actor = Stage.GetParticipant(Role)) CastMembers.AddUnique(Actor);

	int32 CueCount = 0;
	for (const FStagePerformanceCue& Cue : Cues)
	{
		if (auto* Tag = GetTag(*CueTags, CueTagClass, CueCount))
		{
			++CueCount;
			Tag->Present(Cue);
			Tag->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (!IsValid(Cue.Actor) || TaggedActors.Contains(Cue.Actor)) continue;
		if (auto* Tag = GetTag(*ActorLayer, ActorTagClass, TaggedActors.Num()))
		{
			Tag->Present(Cue);
			TaggedActors.Add(Cue.Actor);
		}
	}
	for (int32 Index = CueCount; Index < CueTags->GetChildrenCount(); ++Index)
		CueTags->GetChildAt(Index)->SetVisibility(ESlateVisibility::Collapsed);
	for (int32 Index = TaggedActors.Num(); Index < ActorLayer->GetChildrenCount(); ++Index)
		ActorLayer->GetChildAt(Index)->SetVisibility(ESlateVisibility::Collapsed);
}

UStagePerformanceTagWidget* UStagePerformanceHUDWidget::GetTag(UPanelWidget& Panel,
    TSubclassOf<UStagePerformanceTagWidget> TagClass, int32 Index)
{
	if (auto* Tag = Cast<UStagePerformanceTagWidget>(Panel.GetChildAt(Index))) return Tag;
	if (!TagClass) return nullptr;
	if (auto* Tag = CreateWidget<UStagePerformanceTagWidget>(GetOwningPlayer(), TagClass))
	{
		if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Panel.AddChild(Tag)))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
			Tag->SetVisibility(ESlateVisibility::Collapsed);
		}
		return Tag;
	}
	return nullptr;
}

void UStagePerformanceHUDWidget::HandleScoreChanged(int32 Amount)
{
	const double Now = GetWorld()->GetTimeSeconds();
	RecentScoreDelta = Now - LastScoreAt < 0.2 ? RecentScoreDelta + Amount : Amount;
	LastScoreAt = Now;
	DeltaUntil = RecentScoreDelta == 0 ? -1.0 : Now + 1.45;
	const FText Number = FText::AsNumber(FMath::Abs(RecentScoreDelta));
	const FText SignedNumber = RecentScoreDelta < 0
	    ? FText::Format(NSLOCTEXT("StagePerformance", "ScoreLost", "−{0}"), Number)
	    : FText::Format(NSLOCTEXT("StagePerformance", "ScoreGained", "+{0}"), Number);
	ScoreDelta->SetText(SignedNumber);
	ScoreDelta->SetColorAndOpacity(RecentScoreDelta > 0 ? PositiveColor : NegativeColor);
	ShowAudienceReaction(Amount > 0);
}

void UStagePerformanceHUDWidget::HandleOpeningOverdue()
{
	ShowAudienceReaction(false);
}

void UStagePerformanceHUDWidget::ShowAudienceReaction(bool bPositive)
{
	AudienceUntil = GetWorld()->GetTimeSeconds() + 1.55;
	AudienceText->SetText(bPositive ? NSLOCTEXT("StagePerformance", "AudienceApplause", "Аплодисменты")
	    : NSLOCTEXT("StagePerformance", "AudienceMurmur", "Ропот"));
	AudienceText->SetColorAndOpacity(bPositive ? PositiveColor : NegativeColor);
	AudienceIcon->SetBrushFromTexture(bPositive ? ApplauseIcon.Get() : MurmurIcon.Get());
	AudienceIcon->SetColorAndOpacity(bPositive ? PositiveColor : NegativeColor);
}

void UStagePerformanceHUDWidget::PositionActorTags(const FGeometry& Geometry)
{
	if (TaggedActors.IsEmpty() || !GetOwningPlayer()) return;
	TArray<FBox2D, TInlineAllocator<10>> Occupied;
	for (const TWeakObjectPtr<AActor>& Participant : CastMembers)
	{
		if (!Participant.IsValid()) continue;
		for (const UWidgetComponent* Widget : TInlineComponentArray<UWidgetComponent*>(Participant.Get()))
		{
			const auto* Bubble = Cast<UDialogueBubbleWidget>(Widget->GetUserWidgetObject());
			FBox2D Bounds;
			if (Bubble && Widget->IsVisible() && Widget->GetWidgetSpace() == EWidgetSpace::Screen && Bubble->GetScreenBounds(Bounds, true))
				Occupied.Emplace(Geometry.AbsoluteToLocal(Bounds.Min), Geometry.AbsoluteToLocal(Bounds.Max));
		}
	}
	const FVector2D ViewSize = Geometry.GetLocalSize();
	const FBox2D SafeArea(FVector2D(12.0, 80.0), ViewSize - FVector2D(12.0, 72.0));
	for (int32 Index = 0; Index < TaggedActors.Num(); ++Index)
	{
		UWidget* Tag = ActorLayer->GetChildAt(Index);
		auto* CanvasSlot = CastChecked<UCanvasPanelSlot>(Tag->Slot);
		const AActor* Actor = TaggedActors[Index].Get();
		FVector2D ActorPosition;
		bool bPlaced = false;
		if (IsValid(Actor) && !Actor->IsHidden())
		{
			FVector Center, Extent;
			if (const auto* Character = Cast<ACharacter>(Actor))
			{
				const auto* Capsule = Character->GetCapsuleComponent();
				Center = Capsule->GetComponentLocation();
				Extent = FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
			}
			else Actor->GetActorBounds(true, Center, Extent);
			const FVector Head = Center + FVector(0, 0, Extent.Z);
			if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Head, ActorPosition, true))
			{
				const FVector2D HalfSize = (Tag->GetDesiredSize() * 0.5).ComponentMax(FVector2D(13.0));
				const double Side = HalfSize.X + 17.0;
				const FVector2D Offsets[] = {
				    {Side, 8.0}, {-Side, 8.0},
				    {Side, 28.0}, {-Side, 28.0},
				    {Side, 46.0}, {-Side, 46.0},
				    {Side + 22.0, 8.0}, {-Side - 22.0, 8.0}
				};
				for (const FVector2D& Offset : Offsets)
				{
					const FVector2D Position = ActorPosition + Offset;
					const FBox2D Bounds(Position - HalfSize, Position + HalfSize);
					if (!SafeArea.IsInsideOrOn(Bounds.Min) || !SafeArea.IsInsideOrOn(Bounds.Max)) continue;
					if (Occupied.ContainsByPredicate([&](const FBox2D& Other) { return Other.ExpandBy(4.0).Intersect(Bounds); })) continue;
					CanvasSlot->SetPosition(Position);
					Occupied.Add(Bounds);
					bPlaced = true;
					break;
				}
			}
		}
		Tag->SetVisibility(bPlaced ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UStagePerformanceHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	const double Now = GetWorld()->GetTimeSeconds();
	UpdateFeedback(*ScoreFeedback, Now, DeltaUntil);
	UpdateFeedback(*AudienceFeedback, Now, AudienceUntil);
	PositionActorTags(Geometry);
}

void UStagePerformanceHUDWidget::NativeDestruct()
{
	if (AStageManager* Stage = Source.Get())
	{
		Stage->OnScoreChanged.RemoveAll(this);
		Stage->OnOpeningOverdue.RemoveDynamic(this, &ThisClass::HandleOpeningOverdue);
	}
	Source.Reset();
	Super::NativeDestruct();
}
