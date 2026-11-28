#include "UI/StagePerformanceHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Image.h"
#include "Components/Interaction/InteractionOutlineComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Core/StageManager.h"

namespace
{
constexpr float FeedbackFadeTime = 0.25f;
const FName SymbolParameter(TEXT("Symbol"));
const FName ProgressParameter(TEXT("Progress"));
const FName WindowStartParameter(TEXT("WindowStart"));
const FName WindowEndParameter(TEXT("WindowEnd"));
const FName PhaseParameter(TEXT("Phase"));
const FName PreparationParameter(TEXT("Preparation"));
const FName InkParameter(TEXT("Ink"));

enum class EPerformanceSymbol : uint8 { FilledStar = 5, EmptyStar, Applause, Murmur };

void SetSymbol(UImage& Image, float Symbol)
{
	if (auto* Material = Image.GetDynamicMaterial()) Material->SetScalarParameterValue(SymbolParameter, Symbol);
}

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
	if (CueIcon && LastKind != Cue.Kind)
	{
		LastKind = Cue.Kind;
		SetSymbol(*CueIcon, static_cast<float>(Cue.Kind));
	}
	StartsAt = Cue.StartsAt;
	EndsAt = Cue.EndsAt;
	PausedProgress = Cue.PausedProgress;
	if (Timing)
	{
		Timing->SetVisibility(Cue.Phase == EStageCueWindowPhase::Hidden ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (auto* Material = Timing->GetDynamicMaterial())
		{
			Material->SetScalarParameterValue(WindowStartParameter, Cue.WindowStart);
			Material->SetScalarParameterValue(WindowEndParameter, Cue.WindowEnd);
			Material->SetScalarParameterValue(PhaseParameter, static_cast<float>(Cue.Phase));
			Material->SetScalarParameterValue(PreparationParameter, Cue.bPreparation ? 1.f : 0.f);
		}
	}
	if (LastPhase != Cue.Phase || bLastPreparation != Cue.bPreparation)
	{
		LastPhase = Cue.Phase;
		bLastPreparation = Cue.bPreparation;
		const bool bOpen = Cue.Phase == EStageCueWindowPhase::Open;
		const bool bClosed = Cue.Phase == EStageCueWindowPhase::Closed;
		const FLinearColor Ink = Cue.bPreparation ? PreparationColor : bClosed ? ClosedColor : OpenColor;
		if (auto* Material = CueFaceFabric ? CueFaceFabric->GetDynamicMaterial() : nullptr)
		{
			Material->SetScalarParameterValue(PhaseParameter, static_cast<float>(Cue.Phase));
			Material->SetScalarParameterValue(PreparationParameter, Cue.bPreparation ? 1.f : 0.f);
		}
		if (auto* Material = CueIcon ? CueIcon->GetDynamicMaterial() : nullptr)
		{
			Material->SetVectorParameterValue(InkParameter, Ink);
		}
		if (ActionPrompt)
		{
			UWidget* Status = ActionStatus ? ActionStatus.Get() : ActionPrompt.Get();
			Status->SetVisibility(Cue.bPreparation || bOpen || bClosed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (Cue.bPreparation) ActionPrompt->SetText(NSLOCTEXT("StagePerformance", "CuePrepare", "Подготовьтесь"));
			else if (bOpen) ActionPrompt->SetText(NSLOCTEXT("StagePerformance", "CueNow", "Сейчас"));
			else if (bClosed) ActionPrompt->SetText(NSLOCTEXT("StagePerformance", "CueLate", "Поздно"));
			else ActionPrompt->SetText(FText::GetEmpty());
			ActionPrompt->SetColorAndOpacity(Ink);
		}
		OnCueAppearanceChanged(Cue.bPreparation, bClosed);
	}
}

void UStagePerformanceTagWidget::PresentUntimed(EStageCueKind Kind, const FText& Caption)
{
	FStagePerformanceCue Cue;
	Cue.Kind = Kind;
	Present(Cue);
	if (!ActionPrompt) return;
	ActionPrompt->SetText(Caption);
	UWidget* Status = ActionStatus ? ActionStatus.Get() : ActionPrompt.Get();
	Status->SetVisibility(Caption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UStagePerformanceTagWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	if (!Timing || !Timing->IsVisible()) return;
	if (auto* Material = Timing->GetDynamicMaterial())
	{
		const bool bTimed = EndsAt > StartsAt;
		const double Progress = bTimed ? (GetWorld()->GetTimeSeconds() - StartsAt) / (EndsAt - StartsAt) : 0.0;
		const float DisplayProgress = PausedProgress.Get(FMath::Clamp(static_cast<float>(Progress), 0.0f, 1.0f));
		Material->SetScalarParameterValue(ProgressParameter, bTimed ? DisplayProgress : -1.0f);
	}
}

void UStagePerformanceHUDWidget::SetSource(AStageManager& Stage)
{
	Source = &Stage;
	Stage.OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	Stage.OnOpeningOverdue.AddUniqueDynamic(this, &ThisClass::HandleAudienceMurmur);
	Stage.OnPlayerNoticedByAudience.AddUObject(this, &ThisClass::HandleAudienceMurmur);
}

void UStagePerformanceHUDWidget::Refresh(const AStageManager& Stage, TConstArrayView<FStagePerformanceCue> Cues, TConstArrayView<FName> CastOrder)
{
	const EStageProductionState State = Stage.GetProductionState();
	const bool bVisible = State == EStageProductionState::Preparing || State == EStageProductionState::Playing
	    || Stage.GetState() != EStageState::Idle;
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		UpdatePickupCueOutline();
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
			SetSymbol(*StarImages[Index], static_cast<float>(Index < Stars ? EPerformanceSymbol::FilledStar : EPerformanceSymbol::EmptyStar));
	}

	TaggedActors.Reset();
	CastMembers.Reset();
	if (!Cues.IsEmpty())
		for (FName Role : CastOrder)
			if (AActor* Actor = Stage.GetParticipant(Role)) CastMembers.AddUnique(Actor);

	int32 CueCount = 0;
	TArray<AStagePhysicsActor*> PickupTargets;
	for (const FStagePerformanceCue& Cue : Cues)
	{
		if (auto* Tag = GetTag(*CueTags, CueTagClass, CueCount))
		{
			++CueCount;
			Tag->Present(Cue);
			Tag->SetVisibility(ESlateVisibility::HitTestInvisible);
			for (AStagePhysicsActor* Prop : Cue.PickupTargets) PickupTargets.AddUnique(Prop);
		}
		if (Cue.bPreparation || !IsValid(Cue.Actor) || TaggedActors.Contains(Cue.Actor)) continue;
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
	UpdatePickupCueOutline(PickupTargets);
}

void UStagePerformanceHUDWidget::UpdatePickupCueOutline(TConstArrayView<AStagePhysicsActor*> Props)
{
	APawn* Pawn = Props.IsEmpty() ? nullptr : GetOwningPlayerPawn();
	auto* Outline = Pawn ? Pawn->FindComponentByClass<UInteractionOutlineComponent>() : nullptr;
	TSet<TWeakObjectPtr<AStagePhysicsActor>> NewTargets;
	if (Outline)
		for (AStagePhysicsActor* Prop : Props)
			if (IsValid(Prop)) NewTargets.Add(Prop);
	for (const auto& WeakProp : HighlightedPickups)
		if (AStagePhysicsActor* Prop = WeakProp.Get(); Prop && !NewTargets.Contains(WeakProp)) Prop->SetPickupCueHighlighted(*this, false);
	for (const auto& WeakProp : NewTargets)
		if (AStagePhysicsActor* Prop = WeakProp.Get(); Prop && !HighlightedPickups.Contains(WeakProp)) Prop->SetPickupCueHighlighted(*this, true);
	HighlightedPickups = MoveTemp(NewTargets);

	if (Outline == PickupCueOutline.Get()) return;
	if (auto* Previous = PickupCueOutline.Get()) Previous->SetPickupCueHighlight(*this, false);
	PickupCueOutline = Outline;
	if (Outline) Outline->SetPickupCueHighlight(*this, true);
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

void UStagePerformanceHUDWidget::HandleAudienceMurmur()
{
	ShowAudienceReaction(false);
}

void UStagePerformanceHUDWidget::ShowAudienceReaction(bool bPositive)
{
	AudienceUntil = GetWorld()->GetTimeSeconds() + 1.55;
	AudienceText->SetText(bPositive ? NSLOCTEXT("StagePerformance", "AudienceApplause", "Аплодисменты")
	    : NSLOCTEXT("StagePerformance", "AudienceMurmur", "Ропот"));
	AudienceText->SetColorAndOpacity(bPositive ? PositiveColor : NegativeColor);
	SetSymbol(*AudienceIcon, static_cast<float>(bPositive ? EPerformanceSymbol::Applause : EPerformanceSymbol::Murmur));
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
	const FBox2D Viewport(FVector2D::ZeroVector, ViewSize);
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
			if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Head, ActorPosition, true)
			    && Viewport.IsInsideOrOn(ActorPosition))
			{
				const FVector2D HalfSize = (Tag->GetDesiredSize() * 0.5).ComponentMax(FVector2D(13.0));
				const double Side = HalfSize.X + 17.0;
				const FVector2D Offsets[] = {
				    {Side, -HalfSize.Y}, {-Side, -HalfSize.Y},
				    {Side, 8.0}, {-Side, 8.0},
				    {Side, 28.0}, {-Side, 28.0},
				    {Side, 46.0}, {-Side, 46.0},
				    {0.0, -HalfSize.Y - 8.0},
				    {Side, -HalfSize.Y - 24.0}, {-Side, -HalfSize.Y - 24.0},
				    {Side + 22.0, 8.0}, {-Side - 22.0, 8.0}
				};
				const FVector2D Min = SafeArea.Min + HalfSize;
				const FVector2D Max = SafeArea.Max - HalfSize;
				if (Min.X > Max.X || Min.Y > Max.Y)
				{
					Tag->SetVisibility(ESlateVisibility::Collapsed);
					continue;
				}
				for (const FVector2D& Offset : Offsets)
				{
					const FVector2D Position = (ActorPosition + Offset).ComponentMax(Min).ComponentMin(Max);
					const double DistanceSquared = FVector2D::DistSquared(Position, ActorPosition);
					if (DistanceSquared < FMath::Square(HalfSize.GetMax() + 5.0) || DistanceSquared > FMath::Square(75.0)) continue;
					const FBox2D Bounds(Position - HalfSize, Position + HalfSize);
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
	UpdatePickupCueOutline();
	if (AStageManager* Stage = Source.Get())
	{
		Stage->OnScoreChanged.RemoveAll(this);
		Stage->OnOpeningOverdue.RemoveDynamic(this, &ThisClass::HandleAudienceMurmur);
		Stage->OnPlayerNoticedByAudience.RemoveAll(this);
	}
	Source.Reset();
	Super::NativeDestruct();
}
