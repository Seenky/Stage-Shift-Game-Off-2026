#include "Stage/Cues/StageCueComponent.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/WidgetComponent.h"
#include "Core/MainCameraActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/StageCondition.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Cues/StageCueWorldMarker.h"
#include "Stage/Environment/StageSoundControl.h"
#include "UI/StageCueWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double SignalSize = 72.0;
constexpr double SignalGap = 8.0;

FVector OffsetSignalOnScreen(const APlayerController& Player, const FVector& MarkerLocation, const FVector2D& PixelOffset,
    double Padding, TConstArrayView<FBox2D> DialogueBounds)
{
	FVector2D Original;
	if (!Player.ProjectWorldLocationToScreen(MarkerLocation, Original)) return MarkerLocation;
	FVector2D ScreenPosition = Original + PixelOffset;
	for (const FBox2D& Bounds : DialogueBounds)
	{
		if (Bounds.ExpandBy(Padding).IsInsideOrOn(ScreenPosition))
			ScreenPosition.Y = Bounds.Min.Y - Padding;
	}
	if (ScreenPosition.Equals(Original)) return MarkerLocation;

	FVector RayStart, RayDirection;
	if (!Player.DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayStart, RayDirection)) return MarkerLocation;
	FVector CameraLocation;
	FRotator CameraRotation;
	Player.GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector CameraForward = CameraRotation.Vector();
	const double Alignment = FVector::DotProduct(RayDirection, CameraForward);
	if (Alignment <= UE_SMALL_NUMBER) return MarkerLocation;
	const double Depth = FVector::DotProduct(MarkerLocation - RayStart, CameraForward);
	return RayStart + RayDirection * (Depth / Alignment);
}
}

UStageCueComponent::UStageCueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SignalWidgetClass = TSoftClassPtr<UStageCueSignalWidget>(FSoftObjectPath(TEXT("/Game/DEV/UI/StageSignals/W_StageCueSignal.W_StageCueSignal_C")));
	static ConstructorHelpers::FObjectFinder<UTexture2D> General(TEXT("/Game/DEV/UI/StageSignals/Icons/T_Cue_General"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Props(TEXT("/Game/DEV/UI/StageSignals/Icons/T_Cue_Props"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Lighting(TEXT("/Game/DEV/UI/StageSignals/Icons/T_Cue_Lighting"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Mechanism(TEXT("/Game/DEV/UI/StageSignals/Icons/T_Cue_Mechanism"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Sound(TEXT("/Game/DEV/UI/StageSignals/Icons/T_Cue_Sound"));
	SignalIcons = {
	    {EStageCueKind::General, General.Object},
	    {EStageCueKind::Props, Props.Object},
	    {EStageCueKind::Lighting, Lighting.Object},
	    {EStageCueKind::Mechanism, Mechanism.Object},
	    {EStageCueKind::Sound, Sound.Object},
	};
}

void UStageCueComponent::BeginPlay()
{
	Super::BeginPlay();
	Manager = Cast<AStageManager>(GetOwner());
	if (Manager.IsValid()) Manager->OnStageEvent.AddUniqueDynamic(this, &ThisClass::HandleStageEvent);
	GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, 0.1f, true);
}

bool UStageCueComponent::ShowCue(const FStageCueDefinition& Definition)
{
	if (bEndingPlay || !GetWorld() || Definition.CueId.IsNone() || !FMath::IsFinite(Definition.TimeLimit) || Definition.TimeLimit < 0.0f)
		return false;
	FStageCueRuntime* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == Definition.CueId; });
	if (!Cue)
	{
		Cue = &Cues.AddDefaulted_GetRef();
		Cue->View.Number = NextNumber++;
	}
	// Repeated state events must not restart an active countdown or duplicate cards.
	else if (Cue->View.State != EStageCueState::Completed) return true;
	Cue->Definition = Definition;
	Cue->View.CueId = Definition.CueId;
	Cue->View.Title = Definition.Title;
	Cue->View.Details = Definition.Details;
	Cue->View.State = !Definition.ActivateAtStepId.IsNone() ? EStageCueState::Upcoming : Definition.bIsProblem ? EStageCueState::Problem : EStageCueState::Active;
	Cue->View.bOverdue = false;
	Cue->ActorOverride.Reset();
	Cue->CompletedAt = 0.0;
	Cue->SoundEarliestAt = Cue->SoundLatestAt = Cue->SoundFeedbackUntil = 0.0;
	Cue->SoundFeedback = FText::GetEmpty();
	Cue->bSoundWindowClosed = false;
	SetCueTimeRemaining(Definition.CueId, Definition.TimeLimit);
	Refresh();
	return true;
}

void UStageCueComponent::ActivateCue(FName CueId)
{
	if (auto* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		if (Cue->View.State != EStageCueState::Upcoming) return;
		Cue->View.State = Cue->Definition.bIsProblem ? EStageCueState::Problem : EStageCueState::Active;
		SetCueTimeRemaining(CueId, Cue->TimerDuration);
		Refresh();
	}
}

void UStageCueComponent::SetCueTimeRemaining(FName CueId, float Seconds)
{
	if (!GetWorld() || !FMath::IsFinite(Seconds) || Seconds < 0.0f) return;
	if (auto* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		if (Cue->View.State == EStageCueState::Completed) return;
		Cue->TimerDuration = Seconds;
		Cue->View.bHasDeadline = Seconds > 0.0f && Cue->View.State != EStageCueState::Upcoming;
		Cue->SignalStartedAt = GetWorld()->GetTimeSeconds();
		Cue->Deadline = Cue->View.bHasDeadline ? Cue->SignalStartedAt + Seconds : 0.0;
		Cue->View.SecondsRemaining = Seconds;
		Cue->View.TimeFraction = 1.0f;
		Cue->View.bOverdue = false;
	}
}

void UStageCueComponent::ResolveCue(FName CueId)
{
	if (!GetWorld()) return;
	if (auto* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		if (Cue->View.State == EStageCueState::Completed) return;
		Cue->View.State = EStageCueState::Completed;
		Cue->View.bHasDeadline = false;
		Cue->CompletedAt = GetWorld()->GetTimeSeconds();
		UpdatePresentation();
	}
}

void UStageCueComponent::SetSoundWindow(FName CueId, const FStageSoundWindow& Window)
{
	if (!GetWorld()) return;
	if (auto* Cue = Cues.FindByPredicate([CueId](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		Cue->SoundEarliestAt = Window.EarliestAt;
		Cue->SoundLatestAt = Window.LatestAt;
		SetCueTimeRemaining(CueId, FMath::Max(0.0f, Window.ExpiresAt - static_cast<float>(GetWorld()->GetTimeSeconds())));
		Refresh();
	}
}

void UStageCueComponent::ReportSoundResult(FName CueId, EStageSoundResult Result, bool bWindowActive)
{
	if (!GetWorld()) return;
	auto* Cue = Cues.FindByPredicate([CueId](const auto& Entry) { return Entry.View.CueId == CueId; });
	if (!Cue) return;
	switch (Result)
	{
	case EStageSoundResult::Early: Cue->SoundFeedback = NSLOCTEXT("StageCues", "SoundEarly", "РАНО"); break;
	case EStageSoundResult::OnTime: Cue->SoundFeedback = NSLOCTEXT("StageCues", "SoundPerfect", "ВОВРЕМЯ"); break;
	case EStageSoundResult::Late: Cue->SoundFeedback = NSLOCTEXT("StageCues", "SoundLate", "ПОЗДНО"); break;
	case EStageSoundResult::Missed: Cue->SoundFeedback = NSLOCTEXT("StageCues", "SoundMissed", "ПРОПУЩЕНО"); break;
	case EStageSoundResult::WrongSound: Cue->SoundFeedback = NSLOCTEXT("StageCues", "SoundWrong", "ДРУГОЙ ЗВУК"); break;
	default: return;
	}
	Cue->bSoundWindowClosed = !bWindowActive;
	Cue->SoundFeedbackUntil = GetWorld()->GetTimeSeconds() + (bWindowActive ? 0.6 : 0.9);
	UpdatePresentation();
}

void UStageCueComponent::ReportProblem(FName CueId, const FText& Details)
{
	if (auto* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		if (Cue->View.State == EStageCueState::Completed) return;
		if (Cue->View.State != EStageCueState::Problem && GetWorld()) Cue->SignalStartedAt = GetWorld()->GetTimeSeconds();
		Cue->View.State = EStageCueState::Problem;
		if (!Details.IsEmpty()) Cue->View.Details = Details;
		UpdatePresentation();
	}
}

void UStageCueComponent::DismissCue(FName CueId)
{
	Cues.RemoveAll([&](const auto& Entry) { return Entry.View.CueId == CueId; });
	UpdatePresentation();
}

void UStageCueComponent::ShowActorProblem(FName CueId, const FText& Title, const FText& Details, AActor* Actor, float TimeLimit)
{
	if (!IsValid(Actor) || Actor->GetWorld() != GetWorld()) return;
	FStageCueDefinition Definition;
	Definition.CueId = CueId;
	Definition.Title = Title;
	Definition.Details = Details;
	Definition.bIsProblem = true;
	Definition.TimeLimit = TimeLimit;
	FStageCueTarget Target;
	Target.Type = EStageCueMarkerType::Event;
	Definition.Targets.Add(Target);
	if (ShowCue(Definition))
	{
		if (auto* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == CueId; })) Cue->ActorOverride = Actor;
		UpdatePresentation();
	}
}

void UStageCueComponent::StartStage(UStageDataAsset* Data)
{
	ResetCues();
	DefinitionSource = Data;
}

void UStageCueComponent::NotifyStepStarted(FName StepId)
{
	if (!DefinitionSource || StepId.IsNone()) return;
	for (const FStageCueDefinition& Definition : DefinitionSource->PlayerCues)
	{
		if (Definition.HideAtStepId == StepId) DismissCue(Definition.CueId);
		if (Definition.ShowAtStepId == StepId) ShowCue(Definition);
		if (Definition.ActivateAtStepId == StepId) ActivateCue(Definition.CueId);
	}
}

void UStageCueComponent::HandleStageEvent(FStageEvent Event)
{
	if (!DefinitionSource || !Manager.IsValid()) return;
	for (const FStageCueDefinition& Definition : DefinitionSource->PlayerCues)
	{
		if (Definition.TriggerEvent.IsValid() && Event.EventTag.MatchesTag(Definition.TriggerEvent)
		    && (Definition.SubjectRoleId.IsNone() || Manager->GetParticipant(Definition.SubjectRoleId) == Event.SubjectActor)) ShowCue(Definition);
	}
}

void UStageCueComponent::Refresh()
{
	if (bEndingPlay || !GetWorld()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	Cues.RemoveAll([&](const auto& Cue) {
		return (Cue.bSoundWindowClosed && Now >= Cue.SoundFeedbackUntil)
		    || (Cue.View.State == EStageCueState::Completed && Now - Cue.CompletedAt >= CompletedDisplayTime);
	});
	for (auto& Cue : Cues)
	{
		if (Cue.View.State == EStageCueState::Completed) continue;
		if (Manager.IsValid() && Cue.Definition.CompletionCondition && Cue.Definition.CompletionCondition->Evaluate(Manager.Get()))
		{
			Cue.View.State = EStageCueState::Completed;
			Cue.View.bHasDeadline = false;
			Cue.CompletedAt = Now;
			continue;
		}
		if (!Cue.View.bHasDeadline) continue;
		Cue.View.SecondsRemaining = static_cast<float>(Cue.Deadline - Now);
		Cue.View.TimeFraction = FMath::Clamp(Cue.View.SecondsRemaining / Cue.TimerDuration, 0.0f, 1.0f);
		if (Cue.View.SecondsRemaining <= 0.0f && !Cue.View.bOverdue)
		{
			Cue.View.bOverdue = true;
			Cue.View.State = EStageCueState::Problem;
			if (!Cue.Definition.OverdueDetails.IsEmpty()) Cue.View.Details = Cue.Definition.OverdueDetails;
		}
	}
	UpdatePresentation();
}

TArray<int32> UStageCueComponent::GetVisibleIndices(bool bSignalsOnly) const
{
	TArray<int32> Indices;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		const auto& Cue = Cues[Index];
		const auto& View = Cue.View;
		if (bSignalsOnly)
		{
			if (View.State != EStageCueState::Active && View.State != EStageCueState::Problem) continue;
			if (Now >= Cue.SoundFeedbackUntil && (View.bHasDeadline ? Now >= Cue.Deadline : Now - Cue.SignalStartedAt >= UntimedSignalDuration)) continue;
		}
		Indices.Add(Index);
	}
	const auto Priority = [](EStageCueState State) { return State == EStageCueState::Problem ? 3 : State == EStageCueState::Active ? 2 : State == EStageCueState::Upcoming ? 1 : 0; };
	Indices.Sort([&](int32 A, int32 B)
	{
		const auto& Left = Cues[A].View;
		const auto& Right = Cues[B].View;
		if (Priority(Left.State) != Priority(Right.State)) return Priority(Left.State) > Priority(Right.State);
		if (Left.bHasDeadline != Right.bHasDeadline) return Left.bHasDeadline;
		if (Left.bHasDeadline && Left.SecondsRemaining != Right.SecondsRemaining) return Left.SecondsRemaining < Right.SecondsRemaining;
		return Left.Number < Right.Number;
	});
	if (bSignalsOnly)
	{
		TSet<EStageCueKind> Seen;
		Indices.RemoveAll([&](int32 Index)
		{
			bool bAlreadySeen = false;
			Seen.Add(Cues[Index].Definition.Kind, &bAlreadySeen);
			return bAlreadySeen;
		});
	}
	Indices.SetNum(FMath::Min(Indices.Num(), FMath::Clamp(MaxCards, 1, 5)));
	return Indices;
}

TArray<FStageCueView> UStageCueComponent::GetVisibleCues() const
{
	TArray<FStageCueView> Result;
	for (int32 Index : GetVisibleIndices()) Result.Add(Cues[Index].View);
	return Result;
}

AActor* UStageCueComponent::ResolveTarget(const FStageCueTarget& Target, const FStageCueRuntime& Cue) const
{
	AActor* Actor = Cue.ActorOverride.Get();
	if (!Actor && Manager.IsValid())
	{
		Actor = Manager->GetTarget(Target.BindingId);
		if (!Actor) Actor = Manager->GetParticipant(Target.BindingId);
	}
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == GetWorld() ? Actor : nullptr;
}

void UStageCueComponent::UpdatePresentation()
{
	if (bEndingPlay || !GetWorld() || GetNetMode() == NM_DedicatedServer) return;
	if (bUseWorldSignals)
	{
		UpdateWorldSignals();
		OnCuesChanged.Broadcast();
		return;
	}
	if (!WorldSignals.IsEmpty()) ClearWorldSignals();
	if (WidgetClass)
	{
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) continue;
			if (Widgets.ContainsByPredicate([&](const TObjectPtr<UStageCueHUDWidget>& Widget) { return IsValid(Widget) && Widget->GetOwningPlayer() == PC; })) continue;
			if (auto* Widget = CreateWidget<UStageCueHUDWidget>(PC, WidgetClass))
			{
				Widgets.Add(Widget);
				Widget->SetSource(this);
				Widget->AddToPlayerScreen(20);
			}
		}
	}
	TSet<FString> Desired;
	TSet<AActor*> Highlighted;
	struct FDesiredMarker
	{
		FString Key;
		AActor* Actor;
		const FStageCueTarget* Target;
		const FStageCueView* View;
	};
	TArray<FDesiredMarker> DesiredMarkers;
	for (int32 Index : GetVisibleIndices())
	{
		const auto& Cue = Cues[Index];
		if (Cue.View.State == EStageCueState::Completed) continue;
		for (int32 TargetIndex = 0; TargetIndex < Cue.Definition.Targets.Num() && TargetIndex < 3; ++TargetIndex)
		{
			const auto& Target = Cue.Definition.Targets[TargetIndex];
			AActor* Actor = ResolveTarget(Target, Cue);
			if (!Actor || Highlighted.Contains(Actor) || !MarkerClass) continue;
			Highlighted.Add(Actor);
			const FString Key = Cue.View.CueId.ToString() + TEXT("/") + FString::FromInt(TargetIndex);
			Desired.Add(Key);
			DesiredMarkers.Add({Key, Actor, &Target, &Cue.View});
		}
	}
	for (auto It = Markers.CreateIterator(); It; ++It)
	{
		if (!Desired.Contains(It.Key()) || !IsValid(It.Value()))
		{
			if (IsValid(It.Value())) It.Value()->Destroy();
			It.RemoveCurrent();
		}
	}
	// Release old overlays before another cue takes over the same mesh.
	for (const FDesiredMarker& DesiredMarker : DesiredMarkers)
	{
		auto& Marker = Markers.FindOrAdd(DesiredMarker.Key);
		if (!IsValid(Marker))
		{
			FActorSpawnParameters Params;
			Params.Owner = GetOwner();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Marker = GetWorld()->SpawnActor<AStageCueWorldMarker>(MarkerClass, FTransform::Identity, Params);
		}
		if (Marker) Marker->Configure(DesiredMarker.Actor, *DesiredMarker.Target, *DesiredMarker.View);
	}
	OnCuesChanged.Broadcast();
}

void UStageCueComponent::UpdateWorldSignals()
{
	const TArray<int32> Indices = GetVisibleIndices(true);
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Indices.IsEmpty() || !Player || !Player->IsLocalController())
	{
		ClearWorldSignals();
		return;
	}
	const auto* Camera = Cast<AMainCameraActor>(Player->GetViewTarget());
	const double Dpi = UWidgetLayoutLibrary::GetViewportScale(Player);
	TArray<FBox2D, TInlineAllocator<4>> DialogueBounds;
	if (Manager.IsValid())
	{
		if (const UStageDataAsset* Stage = Manager->GetActiveStage())
			for (FName Role : Stage->GetRoleOptions())
				if (AActor* Actor = Manager->GetParticipant(Role))
					for (const auto* Widget : TInlineComponentArray<UWidgetComponent*>(Actor))
					{
						const auto* Bubble = Cast<UDialogueBubbleWidget>(Widget->GetUserWidgetObject());
						FBox2D Bounds;
						if (Bubble && Widget->IsVisible() && Widget->GetWidgetSpace() == EWidgetSpace::Screen && Bubble->GetScreenBounds(Bounds))
							DialogueBounds.Add(Bounds);
					}
		DialogueBounds.Sort([](const FBox2D& A, const FBox2D& B) { return A.Max.Y > B.Max.Y; });
	}
	const AActor* Anchor = SignalAnchor.Get();
	const bool bHasAnchor = IsValid(Anchor) && !Anchor->IsActorBeingDestroyed() && Anchor->GetWorld() == GetWorld();
	FVector Origin;
	if (bHasAnchor) Origin = Anchor->GetActorLocation();
	else if (Camera)
		Origin = Camera->GetActorLocation() - Camera->GetActorForwardVector().GetSafeNormal2D() * 220.0 + FVector(0, 0, 430);
	else Origin = GetOwner()->GetActorLocation() + FVector(0, 0, 215);
	TMap<AActor*, int32> ActorSlots;
	TSet<EStageCueKind> Visible;
	for (int32 Slot = 0; Slot < Indices.Num(); ++Slot)
	{
		const FStageCueRuntime& Cue = Cues[Indices[Slot]];
		const EStageCueKind Kind = Cue.Definition.Kind;
		UTexture2D* Icon = Cue.Definition.Icon.Get();
		if (!Icon) Icon = SignalIcons.FindRef(Kind);
		if (!Icon) Icon = SignalIcons.FindRef(EStageCueKind::General);
		if (!Icon) continue;

		AActor* TrackedActor = Cue.ActorOverride.Get();
		FVector TargetOffset = FVector::ZeroVector;
		if (!TrackedActor && Manager.IsValid())
		{
			for (const FStageCueTarget& Target : Cue.Definition.Targets)
			{
				if (Target.Type != EStageCueMarkerType::Recipient) continue;
				if (AActor* Actor = Manager->GetParticipant(Target.BindingId))
				{
					TrackedActor = Actor;
					TargetOffset = Target.Offset;
					break;
				}
			}
			if (!TrackedActor && !Cue.Definition.SubjectRoleId.IsNone()) TrackedActor = Manager->GetParticipant(Cue.Definition.SubjectRoleId);
			if (!TrackedActor)
				for (const FStageCueTarget& Target : Cue.Definition.Targets)
					if (AActor* Actor = ResolveTarget(Target, Cue))
					{
						TrackedActor = Actor;
						TargetOffset = Target.Offset;
						break;
					}
		}

		USceneComponent* Parent = nullptr;
		FVector Location = Origin;
		FVector2D ScreenOffset((Slot - (Indices.Num() - 1) * 0.5) * (SignalSize + SignalGap), 0.0);
		if (IsValid(TrackedActor) && !TrackedActor->IsActorBeingDestroyed() && TrackedActor->GetWorld() == GetWorld())
		{
			Parent = TrackedActor->GetRootComponent();
			FVector Center, Extent;
			if (const auto* Character = Cast<ACharacter>(TrackedActor))
			{
				const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
				Center = Capsule->GetComponentLocation();
				Extent = FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
			}
			else TrackedActor->GetActorBounds(true, Center, Extent);
			Location = Center + FVector(0, 0, Extent.Z + 18.0) + TargetOffset;
			const int32 ActorSlot = ActorSlots.FindOrAdd(TrackedActor)++;
			ScreenOffset = FVector2D(0.0, -SignalSize * 0.5 - ActorSlot * (SignalSize + SignalGap));
		}
		Location = OffsetSignalOnScreen(*Player, Location + SignalOffset, ScreenOffset * Dpi, (SignalSize * 0.5 + SignalGap) * Dpi, DialogueBounds);
		auto& Signal = WorldSignals.FindOrAdd(Kind);
		if (!IsValid(Signal))
		{
			auto* Widget = CreateWidget<UStageCueSignalWidget>(Player, SignalWidgetClass.LoadSynchronous());
			if (!Widget) continue;
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			Signal = NewObject<UWidgetComponent>(GetOwner(), NAME_None, RF_Transient);
			GetOwner()->AddInstanceComponent(Signal);
			Signal->SetWidgetSpace(EWidgetSpace::Screen);
			Signal->SetDrawSize(FVector2D(SignalSize));
			Signal->SetOwnerPlayer(Player->GetLocalPlayer());
			Signal->SetInitialSharedLayerName(TEXT("StageCueSignals"));
			Signal->SetInitialLayerZOrder(1);
			Signal->SetWindowFocusable(false);
			Signal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Signal->SetCanEverAffectNavigation(false);
			Signal->SetWidget(Widget);
			Signal->SetWorldLocation(Location);
			Signal->RegisterComponentWithWorld(GetWorld());
		}
		if (Signal->GetAttachParent() != Parent)
		{
			if (Parent) Signal->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
			else Signal->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
		Signal->SetWorldLocation(Location);
		if (auto* Widget = Cast<UStageCueSignalWidget>(Signal->GetUserWidgetObject()))
		{
			const double Now = GetWorld()->GetTimeSeconds();
			FText Caption;
			float Remaining = Cue.View.SecondsRemaining;
			float Fraction = Cue.View.TimeFraction;
			if (Now < Cue.SoundFeedbackUntil) Caption = Cue.SoundFeedback;
			else if (Cue.SoundLatestAt > 0.0)
			{
				const bool bWaiting = Now < Cue.SoundEarliestAt;
				const bool bOnTime = !bWaiting && Now <= Cue.SoundLatestAt;
				Caption = bWaiting ? NSLOCTEXT("StageCues", "SoundWait", "ЖДИ") : bOnTime ? NSLOCTEXT("StageCues", "SoundNow", "СЕЙЧАС")
				    : NSLOCTEXT("StageCues", "SoundWindowLate", "ПОЗДНО");
				const double Start = bWaiting ? Cue.SignalStartedAt : bOnTime ? Cue.SoundEarliestAt : Cue.SoundLatestAt;
				const double End = bWaiting ? Cue.SoundEarliestAt : bOnTime ? Cue.SoundLatestAt : Cue.Deadline;
				Remaining = FMath::Max(End - Now, 0.0);
				Fraction = FMath::Clamp(Remaining / FMath::Max(End - Start, 0.01), 0.0, 1.0);
			}
			Widget->UpdateSignal(Icon, Fraction, Cue.View.bHasDeadline && !Cue.bSoundWindowClosed, Remaining);
			Widget->SetSignalCaption(Caption);
		}
		FVector2D ScreenCenter;
		if (Player->ProjectWorldLocationToScreen(Location, ScreenCenter))
		{
			const FVector2D HalfSize(SignalSize * Dpi * 0.5);
			DialogueBounds.Emplace(ScreenCenter - HalfSize, ScreenCenter + HalfSize);
			DialogueBounds.Sort([](const FBox2D& A, const FBox2D& B) { return A.Max.Y > B.Max.Y; });
		}
		Visible.Add(Kind);
	}
	for (auto It = WorldSignals.CreateIterator(); It; ++It)
	{
		if (Visible.Contains(It.Key())) continue;
		if (IsValid(It.Value())) It.Value()->DestroyComponent();
		It.RemoveCurrent();
	}
}

void UStageCueComponent::ClearWorldSignals()
{
	for (const auto& Pair : WorldSignals) if (IsValid(Pair.Value)) Pair.Value->DestroyComponent();
	WorldSignals.Reset();
}

TArray<AStageCueWorldMarker*> UStageCueComponent::GetVisibleMarkers() const
{
	TArray<AStageCueWorldMarker*> Result;
	for (const auto& Pair : Markers) if (IsValid(Pair.Value)) Result.Add(Pair.Value);
	return Result;
}

void UStageCueComponent::ResetCues()
{
	ClearWorldSignals();
	Cues.Reset();
	DefinitionSource = nullptr;
	NextNumber = 1;
	for (const auto& Pair : Markers) if (IsValid(Pair.Value)) Pair.Value->Destroy();
	Markers.Reset();
	OnCuesChanged.Broadcast();
}

void UStageCueComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	if (Manager.IsValid()) Manager->OnStageEvent.RemoveDynamic(this, &ThisClass::HandleStageEvent);
	ResetCues();
	for (UStageCueHUDWidget* Widget : Widgets) if (IsValid(Widget)) { Widget->SetSource(nullptr); Widget->RemoveFromParent(); }
	Widgets.Reset();
	Super::EndPlay(Reason);
}
