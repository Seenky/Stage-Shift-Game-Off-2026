#include "Stage/Cues/StageCueComponent.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/MainCameraActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Physics/PhysicsActor.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "Stage/Commands/PickUpCommand.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/StageCondition.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Cues/StageCueWorldMarker.h"
#include "Stage/Environment/StageSoundControl.h"
#include "UI/StageCueWidget.h"
#include "UI/StagePerformanceHUDWidget.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr double SignalSize = 72.0;
constexpr double SignalGap = 8.0;

const UPickUpCommand* FindPickupInTree(const UStageMemberCommand* Command)
{
	if (!IsValid(Command)) return nullptr;
	if (const auto* Pickup = Cast<UPickUpCommand>(Command)) return Pickup;
	for (const UStageMemberCommand* Child : Command->GetChildCommands())
		if (const auto* Pickup = FindPickupInTree(Child)) return Pickup;
	return nullptr;
}

const UPickUpCommand* FindCuePickup(const AStageManager& Stage, const FStageCueDefinition& Definition, const AActor& Actor)
{
	const FName StepId = Definition.ActivateAtStepId.IsNone() ? Definition.ShowAtStepId : Definition.ActivateAtStepId;
	if (const FStageStep* Step = Stage.FindPreparedStep(StepId))
		for (const auto& Assignment : Step->Commands)
			if (Stage.GetParticipant(Assignment.GetEffectiveRoleId()) == &Actor)
				if (const auto* Pickup = FindPickupInTree(Assignment.Command)) return Pickup;
	return nullptr;
}

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

float ElapsedFractionAt(const FStageCueRuntime& Cue, double Time)
{
	return Cue.View.bHasDeadline && Cue.TimerDuration > 0.0f
	    ? FMath::Clamp(static_cast<float>((Time - (Cue.Deadline - Cue.TimerDuration)) / Cue.TimerDuration), 0.0f, 1.0f)
	    : 0.0f;
}

TOptional<TRange<double>> GetActiveThrowWindow(const AActor& Recipient)
{
	const auto* Member = Recipient.FindComponentByClass<UStageMemberComponent>();
	return Member ? Member->GetStagePropThrowWindow() : TOptional<TRange<double>>{};
}

EStageCueWindowPhase GetPropSignalPhase(const FStageCueRuntime& Cue, const AActor& Recipient,
    const TOptional<TRange<double>>& ActiveWindow, double Now)
{
	const auto& Window = ActiveWindow.IsSet() ? ActiveWindow : Cue.PropThrowWindow;
	if (Window.IsSet() && (!ActiveWindow.IsSet() || Now >= Window->GetUpperBoundValue())) return EStageCueWindowPhase::Closed;
	const auto* Manipulation = Recipient.FindComponentByClass<UPhysicsManipulationComponent>();
	if (!Manipulation) return EStageCueWindowPhase::Hidden;
	return Cue.View.State != EStageCueState::Upcoming && Manipulation->CanReceiveStageProp()
	    ? EStageCueWindowPhase::Open : EStageCueWindowPhase::Waiting;
}

void PresentWorldSignal(UStageCueSignalWidget& Widget, FStageCueRuntime& Cue, UTexture2D* Icon, const AActor* Recipient, double Now, bool bShowPreparation)
{
	const bool bTimed = Cue.View.bHasDeadline && !Cue.bSoundWindowClosed;
	float Start = 0.0f;
	float End = 0.0f;
	FText Caption;
	EStageCueWindowPhase Phase = EStageCueWindowPhase::Hidden;

	if (Cue.View.State == EStageCueState::Completed)
	{
		Caption = NSLOCTEXT("StageCues", "RetainedCueComplete", "ГОТОВО");
	}
	else if (Cue.bDisplayPaused)
	{
		Phase = EStageCueWindowPhase::Waiting;
		Caption = NSLOCTEXT("StageCues", "ActorRecovering", "АКТЁР ВСТАЁТ");
	}
	else if (Cue.bActionEnded && !Cue.bSoundWindowClosed)
	{
		Phase = Cue.View.bOverdue ? EStageCueWindowPhase::Closed : EStageCueWindowPhase::Waiting;
		Caption = Cue.View.bOverdue ? NSLOCTEXT("StageCues", "CueOverdue", "ПОЗДНО") : NSLOCTEXT("StageCues", "CueWait", "ЖДИ");
	}
	else if (Cue.SoundLatestAt > 0.0)
	{
		Start = ElapsedFractionAt(Cue, Cue.SoundEarliestAt);
		End = ElapsedFractionAt(Cue, Cue.SoundLatestAt);
		if (Cue.bSoundWindowClosed || Now > Cue.SoundLatestAt)
		{
			Phase = EStageCueWindowPhase::Closed;
			Caption = NSLOCTEXT("StageCues", "SoundWindowLate", "ПОЗДНО");
		}
		else if (Now < Cue.SoundEarliestAt)
		{
			Phase = EStageCueWindowPhase::Waiting;
			Caption = bShowPreparation ? NSLOCTEXT("StagePerformance", "CuePrepare", "Подготовьтесь") : NSLOCTEXT("StageCues", "SoundWait", "ЖДИ");
		}
		else
		{
			Phase = EStageCueWindowPhase::Open;
			Caption = NSLOCTEXT("StageCues", "SoundNow", "СЕЙЧАС");
		}
		if (Now < Cue.SoundFeedbackUntil || (Cue.bSoundWindowClosed && Cue.bKeepUntilStepEnds)) Caption = Cue.SoundFeedback;
	}
	else if (Cue.View.State == EStageCueState::Upcoming
	    && (Cue.Definition.Kind == EStageCueKind::Lighting || Cue.Definition.Kind == EStageCueKind::Sound))
	{
		Phase = EStageCueWindowPhase::Waiting;
		Caption = Cue.Definition.Kind == EStageCueKind::Sound
		    ? NSLOCTEXT("StagePerformance", "CuePrepare", "Подготовьтесь")
		    : NSLOCTEXT("StageCues", "LightPrepare", "ГОТОВЬСЯ");
	}
	else if (Cue.Definition.Kind == EStageCueKind::Props && IsValid(Recipient))
	{
		const auto ActiveWindow = GetActiveThrowWindow(*Recipient);
		if (ActiveWindow.IsSet()) Cue.PropThrowWindow = ActiveWindow;
		if (bTimed && Cue.PropThrowWindow.IsSet())
		{
			Start = ElapsedFractionAt(Cue, Cue.PropThrowWindow->GetLowerBoundValue());
			End = ElapsedFractionAt(Cue, Cue.PropThrowWindow->GetUpperBoundValue());
		}
		Phase = GetPropSignalPhase(Cue, *Recipient, ActiveWindow, Now);
		switch (Phase)
		{
		case EStageCueWindowPhase::Closed: Caption = NSLOCTEXT("StageCues", "PropWindowClosed", "ПОЗДНО"); break;
		case EStageCueWindowPhase::Open: Caption = NSLOCTEXT("StageCues", "PropThrow", "БРОСАЙ!"); break;
		case EStageCueWindowPhase::Waiting: Caption = NSLOCTEXT("StageCues", "PropPrepare", "ГОТОВЬСЯ"); break;
		default: break;
		}
	}

	Widget.UpdateSignal(Icon, Cue.View.TimeFraction, bTimed, FMath::Max(0.f, Cue.View.SecondsRemaining));
	Widget.SetSignalWindow(Start, End, Phase);
	Widget.SetSignalCaption(Caption);
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
	return ShowCueInternal(Definition, nullptr);
}

bool UStageCueComponent::ShowActionCue(const FStageCueDefinition& Definition, UStageMemberComponent& Performer)
{
	return ShowCueInternal(Definition, &Performer);
}

bool UStageCueComponent::ShowCueInternal(const FStageCueDefinition& Definition, UStageMemberComponent* Performer)
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
	else if (!Performer && Cue->View.State != EStageCueState::Completed) return true;
	Cue->Definition = Definition;
	Cue->View.CueId = Definition.CueId;
	Cue->View.Title = Definition.Title;
	Cue->View.Details = Definition.Details;
	Cue->View.State = !Definition.ActivateAtStepId.IsNone() ? EStageCueState::Upcoming : Definition.bIsProblem ? EStageCueState::Problem : EStageCueState::Active;
	Cue->View.bOverdue = false;
	Cue->ActorOverride.Reset();
	Cue->Performer = Performer;
	Cue->CompletedAt = 0.0;
	Cue->PropThrowWindow.Reset();
	Cue->SoundEarliestAt = Cue->SoundLatestAt = Cue->SoundFeedbackUntil = 0.0;
	Cue->SoundFeedback = FText::GetEmpty();
	Cue->bSoundWindowClosed = false;
	Cue->bSoundSucceeded = false;
	Cue->bKeepSignalVisible = false;
	Cue->bDisplayPaused = false;
	Cue->bKeepUntilStepEnds = Performer && Cue->bKeepUntilStepEnds;
	Cue->bActionEnded = false;
	SetCueTimeRemaining(Definition.CueId, Definition.TimeLimit);
	Refresh();
	return true;
}

void UStageCueComponent::FinishActionCue(FName CueId, bool bSucceeded)
{
	auto* Cue = Cues.FindByPredicate([CueId](const FStageCueRuntime& Entry) { return Entry.View.CueId == CueId; });
	if (!Cue) return;
	UpdateRecoveryPresentation(*Cue);
	if (Cue->bSoundWindowClosed) return;
	if (bSucceeded && Cue->Definition.Kind != EStageCueKind::Sound) ResolveCue(CueId);
	else if (Cue->bKeepUntilStepEnds)
	{
		Cue->bActionEnded = true;
		UpdatePresentation();
	}
	else DismissCue(CueId);
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
		SetCueTimeRemaining(CueId, FMath::Max(0.0f, Window.ExpiresAt - Window.OpenedAt));
		if (Cue->View.bHasDeadline) Cue->Deadline = Window.ExpiresAt;
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
	Cue->bSoundSucceeded = Result == EStageSoundResult::OnTime;
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

void UStageCueComponent::DismissLightingPreparation(FName RoleId)
{
	if (Cues.RemoveAll([RoleId](const FStageCueRuntime& Cue)
	{
		return Cue.Definition.Kind == EStageCueKind::Lighting && !Cue.Definition.ActivateAtStepId.IsNone()
		    && Cue.Definition.SubjectRoleId == RoleId;
	}) > 0) UpdatePresentation();
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

void UStageCueComponent::ShowActorSignal(FName CueId, EStageCueKind Kind, AActor* Actor)
{
	if (!IsValid(Actor)) return;
	FStageCueDefinition Definition;
	Definition.CueId = CueId;
	Definition.Kind = Kind;
	Definition.Targets.AddDefaulted_GetRef().Type = EStageCueMarkerType::Event;
	if (!ShowCue(Definition)) return;
	if (auto* Cue = Cues.FindByPredicate([CueId](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		Cue->ActorOverride = Actor;
		Cue->bKeepSignalVisible = true;
	}
	UpdatePresentation();
}

void UStageCueComponent::StartStage(UStageDataAsset* Data)
{
	ResetCues();
	DefinitionSource = Data;
}

void UStageCueComponent::NotifyStepStarted(FName StepId)
{
	if (!DefinitionSource || !GetWorld()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	Cues.RemoveAll([&](const FStageCueRuntime& Cue)
	{
		if (Cue.Definition.Kind == EStageCueKind::Sound && Cue.View.State == EStageCueState::Upcoming)
		{
			TSet<FName> StepsToActivation;
			return StepId.IsNone() || !DefinitionSource->GetStepRange(StepId, Cue.Definition.ActivateAtStepId, StepsToActivation);
		}
		return Cue.bKeepUntilStepEnds && (StepId.IsNone() || Cue.Performer.IsValid() || Cue.View.State == EStageCueState::Completed
		    || (Cue.View.bHasDeadline && Now >= Cue.Deadline) || Cue.Definition.HideAtStepId.IsNone());
	});
	for (FStageCueRuntime& Cue : Cues)
	{
		Cue.bKeepUntilStepEnds = false;
		Cue.bDisplayPaused = false;
	}
	if (StepId.IsNone())
	{
		UpdatePresentation();
		return;
	}
	for (const FStageCueDefinition& Definition : DefinitionSource->PlayerCues)
	{
		if (Definition.HideAtStepId == StepId) DismissCue(Definition.CueId);
		if (Definition.ShowAtStepId == StepId) ShowCue(Definition);
		if (Definition.ActivateAtStepId == StepId) ActivateCue(Definition.CueId);
	}
	UpdatePresentation();
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
	for (FStageCueRuntime& Cue : Cues) UpdateRecoveryPresentation(Cue);
	Cues.RemoveAll([&](const auto& Cue) {
		return !Cue.bKeepUntilStepEnds && ((Cue.bSoundWindowClosed && Now >= Cue.SoundFeedbackUntil)
		    || (Cue.View.State == EStageCueState::Completed && Now - Cue.CompletedAt >= CompletedDisplayTime));
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
		// Freeze the displayed sample. The original deadline still controls the cue's timing.
		if (!Cue.View.bHasDeadline || Cue.bDisplayPaused) continue;
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

void UStageCueComponent::UpdateRecoveryPresentation(FStageCueRuntime& Cue) const
{
	Cue.bDisplayPaused = false;
	if (!Manager.IsValid() || Manager->GetState() != EStageState::Running || !Cue.View.bHasDeadline || Cue.bSoundWindowClosed ||
	    (!Cue.bKeepUntilStepEnds && Cue.View.SecondsRemaining <= 0.f)) return;

	const auto IsRecovering = [](const UStageMemberComponent* Member) { return IsValid(Member) && Member->IsPhysicallyBlocked(); };
	bool bRecovering = IsRecovering(Cue.Performer.Get());
	if (!bRecovering && (Cue.Definition.Kind != EStageCueKind::Sound || !Cue.Performer.IsValid()))
	{
		const auto* Marker = Cue.Definition.Targets.FindByPredicate([](const FStageCueTarget& Target) { return Target.Type == EStageCueMarkerType::Recipient; });
		const FName RoleId = Marker ? Marker->BindingId : Cue.Definition.SubjectRoleId;
		const AActor* Actor = Manager->GetParticipant(RoleId);
		bRecovering = IsValid(Actor) && IsRecovering(Actor->FindComponentByClass<UStageMemberComponent>());
	}
	Cue.bDisplayPaused = bRecovering;
	Cue.bKeepUntilStepEnds |= bRecovering;
}

TArray<int32> UStageCueComponent::GetVisibleIndices(bool bSignalsOnly, bool bDeduplicateKinds) const
{
	TArray<int32> Indices;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bShowPreparation = !DefinitionSource || DefinitionSource->bShowPreparationCues;
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		const auto& Cue = Cues[Index];
		const auto& View = Cue.View;
		const auto* RecipientMarker = Cue.Definition.Kind == EStageCueKind::Props
		    ? Cue.Definition.Targets.FindByPredicate([](const FStageCueTarget& Target) { return Target.Type == EStageCueMarkerType::Recipient; })
		    : nullptr;
		const bool bPreparing = View.State == EStageCueState::Upcoming
		    && (!bDeduplicateKinds || RecipientMarker || Cue.Definition.Kind == EStageCueKind::Lighting || Cue.Definition.Kind == EStageCueKind::Sound);
		if (!bShowPreparation && !Cue.bKeepUntilStepEnds)
		{
			if (bPreparing) continue;
			if (RecipientMarker && Manager.IsValid())
				if (const auto* Recipient = Manager->GetParticipant(RecipientMarker->BindingId); IsValid(Recipient)
				    && GetPropSignalPhase(Cue, *Recipient, GetActiveThrowWindow(*Recipient), Now) == EStageCueWindowPhase::Waiting) continue;
		}
		if (bSignalsOnly && !Cue.bKeepUntilStepEnds && !bPreparing)
		{
			if (View.State != EStageCueState::Active && View.State != EStageCueState::Problem) continue;
			const bool bShowExpired = !bDeduplicateKinds && View.bOverdue && Now < Cue.Deadline + 0.65;
			if (!Cue.bKeepSignalVisible && !bShowExpired && Now >= Cue.SoundFeedbackUntil
			    && (View.bHasDeadline ? Now >= Cue.Deadline : Now - Cue.SignalStartedAt >= UntimedSignalDuration)) continue;
		}
		Indices.Add(Index);
	}
	const auto Priority = [](EStageCueState State) { return State == EStageCueState::Problem ? 3 : State == EStageCueState::Active ? 2 : State == EStageCueState::Upcoming ? 1 : 0; };
	Indices.Sort([&](int32 A, int32 B)
	{
		const auto& LeftCue = Cues[A];
		const auto& RightCue = Cues[B];
		const auto IsRetainedResult = [](const FStageCueRuntime& Cue)
		{
			return Cue.bKeepUntilStepEnds && (Cue.bActionEnded || Cue.bSoundWindowClosed || Cue.View.State == EStageCueState::Completed);
		};
		const bool bLeftEnded = IsRetainedResult(LeftCue);
		const bool bRightEnded = IsRetainedResult(RightCue);
		if (LeftCue.bDisplayPaused != RightCue.bDisplayPaused) return LeftCue.bDisplayPaused;
		if (bLeftEnded != bRightEnded) return !bLeftEnded;
		const auto& Left = LeftCue.View;
		const auto& Right = RightCue.View;
		if (Priority(Left.State) != Priority(Right.State)) return Priority(Left.State) > Priority(Right.State);
		if (Left.bHasDeadline != Right.bHasDeadline) return Left.bHasDeadline;
		if (Left.bHasDeadline && Left.SecondsRemaining != Right.SecondsRemaining) return Left.SecondsRemaining < Right.SecondsRemaining;
		return Left.Number < Right.Number;
	});
	if (bSignalsOnly && bDeduplicateKinds)
	{
		TSet<EStageCueKind> Seen;
		Indices.RemoveAll([&](int32 Index)
		{
			bool bAlreadySeen = false;
			Seen.Add(Cues[Index].Definition.Kind, &bAlreadySeen);
			return bAlreadySeen;
		});
	}
	if (bDeduplicateKinds) Indices.SetNum(FMath::Min(Indices.Num(), FMath::Clamp(MaxCards, 1, 5)));
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
	if (PerformanceHUDClass && Manager.IsValid())
	{
		UpdatePerformanceHUD(*Manager.Get());
		OnCuesChanged.Broadcast();
		return;
	}
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

void UStageCueComponent::UpdatePerformanceHUD(AStageManager& Stage)
{
	const UStageDataAsset* ActiveStage = Stage.GetActiveStage();
	TArray<FName> CastOrder;
	if (const UStageProductionDataAsset* Production = Stage.GetActiveProduction())
	{
		for (const FStageProductionScene& Scene : Production->Scenes)
			if (Scene.Scenario)
				for (FName Role : Scene.Scenario->GetRoleOptions()) CastOrder.AddUnique(Role);
	}
	else if (ActiveStage) CastOrder = ActiveStage->GetRoleOptions();

	struct FPreparationCue
	{
		FStagePerformanceCue Tag;
		int32 StepsUntilActivation;
	};
	TArray<FStagePerformanceCue, TInlineAllocator<6>> Tags;
	TArray<FPreparationCue, TInlineAllocator<3>> Preparations;
	const int32 CurrentStep = Stage.GetCurrentStepIndex();
	const bool bShowPreparation = ActiveStage && ActiveStage->bShowPreparationCues;
	const bool bCanPreview = Stage.GetState() == EStageState::Running && bShowPreparation;
	const double Now = GetWorld()->GetTimeSeconds();
	const int32 Limit = FMath::Clamp(MaxCards, 1, 5);
	const auto HasActiveCard = [&Tags](const FStagePerformanceCue& Cue)
	{
		return Tags.ContainsByPredicate([&Cue](const FStagePerformanceCue& Tag) { return Tag.Kind == Cue.Kind && Tag.Number == Cue.Number; });
	};
	for (int32 Index : GetVisibleIndices(true, false))
	{
		auto& Cue = Cues[Index];
		const EStageCueKind Kind = Cue.Definition.Kind;
		const bool bSoundCompleted = Cue.bSoundWindowClosed && Cue.bSoundSucceeded;
		if (bSoundCompleted && !Cue.bKeepUntilStepEnds) continue;

		AActor* Actor = nullptr;
		if (const FStageCueTarget* Recipient = Cue.Definition.Targets.FindByPredicate([](const FStageCueTarget& Target)
		    { return Target.Type == EStageCueMarkerType::Recipient; })) Actor = Stage.GetParticipant(Recipient->BindingId);
		if (!Actor && !Cue.Definition.SubjectRoleId.IsNone()) Actor = Stage.GetParticipant(Cue.Definition.SubjectRoleId);
		if (!Actor)
			if (const UStageMemberComponent* Performer = Cue.Performer.Get()) Actor = Performer->GetOwner();
		if (!Actor) Actor = Cue.ActorOverride.Get();

		int32 Number = 0;
		if (IsValid(Actor))
		{
			const int32 CastIndex = CastOrder.IndexOfByPredicate([&](FName Role) { return Stage.GetParticipant(Role) == Actor; });
			if (CastIndex != INDEX_NONE) Number = CastIndex + 1;
		}
		EStageCueWindowPhase Phase = Cue.View.State == EStageCueState::Upcoming ? EStageCueWindowPhase::Waiting : EStageCueWindowPhase::Open;
		if (Cue.View.State == EStageCueState::Problem)
			Phase = Kind == EStageCueKind::General && !Cue.View.bHasDeadline ? EStageCueWindowPhase::Hidden : EStageCueWindowPhase::Closed;
		float WindowStart = 0.0f;
		float WindowEnd = 0.0f;
		if (Cue.bSoundWindowClosed || (Cue.SoundLatestAt > 0.0 && Now > Cue.SoundLatestAt))
			Phase = EStageCueWindowPhase::Closed;
		if (Cue.SoundLatestAt > 0.0)
		{
			WindowStart = ElapsedFractionAt(Cue, Cue.SoundEarliestAt);
			WindowEnd = ElapsedFractionAt(Cue, Cue.SoundLatestAt);
			if (Phase != EStageCueWindowPhase::Closed && Now < Cue.SoundEarliestAt) Phase = EStageCueWindowPhase::Waiting;
		}
		else if (Kind == EStageCueKind::Props && Number > 0)
		{
			const auto ActiveWindow = GetActiveThrowWindow(*Actor);
			if (ActiveWindow.IsSet()) Cue.PropThrowWindow = ActiveWindow;
			const EStageCueWindowPhase PropPhase = GetPropSignalPhase(Cue, *Actor, ActiveWindow, Now);
			if (Phase != EStageCueWindowPhase::Closed && PropPhase != EStageCueWindowPhase::Hidden) Phase = PropPhase;
			if (Cue.PropThrowWindow.IsSet())
			{
				WindowStart = ElapsedFractionAt(Cue, Cue.PropThrowWindow->GetLowerBoundValue());
				WindowEnd = ElapsedFractionAt(Cue, Cue.PropThrowWindow->GetUpperBoundValue());
			}
		}
		if (Cue.View.State == EStageCueState::Completed || bSoundCompleted || Cue.bDisplayPaused)
			Phase = EStageCueWindowPhase::Waiting;
		else if (Cue.bActionEnded && !Cue.bSoundWindowClosed)
			Phase = Cue.View.bOverdue || (Cue.View.bHasDeadline && Now >= Cue.Deadline) ? EStageCueWindowPhase::Closed : EStageCueWindowPhase::Waiting;
		const double EndsAt = Cue.View.bHasDeadline ? Cue.Deadline : 0.0;
		const double StartsAt = Cue.View.bHasDeadline ? Cue.Deadline - Cue.TimerDuration : 0.0;
		const TOptional<float> PausedProgress = Cue.bDisplayPaused ? TOptional<float>(1.0f - Cue.View.TimeFraction) : TOptional<float>();
		const bool bUpcoming = Cue.View.State == EStageCueState::Upcoming;
		const bool bWaitingForSound = bShowPreparation && Kind == EStageCueKind::Sound && Phase == EStageCueWindowPhase::Waiting
		    && Now < Cue.SoundEarliestAt && !Cue.bDisplayPaused && !Cue.bActionEnded;
		FStagePerformanceCue Tag{Kind, Number > 0 ? Actor : nullptr, Number, Phase, StartsAt, EndsAt, WindowStart, WindowEnd, PausedProgress, bUpcoming || bWaitingForSound};
		if (Kind == EStageCueKind::Props && IsValid(Actor))
		{
			const auto* Member = Actor->FindComponentByClass<UStageMemberComponent>();
			const auto* ActivePickup = !bUpcoming && Member ? Member->GetActivePickUpCommand() : nullptr;
			if (bUpcoming || Phase == EStageCueWindowPhase::Open || PausedProgress.IsSet()
			    || (Phase == EStageCueWindowPhase::Waiting && ActivePickup))
			{
				const auto* Pickup = ActivePickup ? ActivePickup : FindCuePickup(Stage, Cue.Definition, *Actor);
				if (Pickup)
					for (FName Id : Pickup->GetPickupTargets())
						if (auto* Prop = Cast<AStagePhysicsActor>(Stage.GetTarget(Id)); IsValid(Prop)) Tag.PickupTargets.AddUnique(Prop);
			}
		}
		if (!bUpcoming)
		{
			if (Tags.Num() < Limit && !HasActiveCard(Tag)) Tags.Add(Tag);
			continue;
		}
		if (!bCanPreview || !ActiveStage->Steps.IsValidIndex(CurrentStep)) continue;
		TSet<FName> StepsToActivation;
		if (ActiveStage->GetStepRange(ActiveStage->Steps[CurrentStep].StepId, Cue.Definition.ActivateAtStepId, StepsToActivation))
			Preparations.Add({Tag, StepsToActivation.Num()});
	}
	const FPreparationCue* Next = nullptr;
	for (const FPreparationCue& Preparation : Preparations)
		if (!Next || Preparation.StepsUntilActivation < Next->StepsUntilActivation) Next = &Preparation;
	if (Next) Tags.Add(Next->Tag);

	PerformanceWidgets.RemoveAll([](const TObjectPtr<UStagePerformanceHUDWidget>& Widget)
	{
		if (IsValid(Widget) && IsValid(Widget->GetOwningPlayer())) return false;
		if (IsValid(Widget)) Widget->RemoveFromParent();
		return true;
	});
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Player = It->Get();
		if (!Player || !Player->GetLocalPlayer() || !Player->IsLocalController()) continue;
		if (PerformanceWidgets.ContainsByPredicate([&](const auto& Widget) { return Widget->GetOwningPlayer() == Player; })) continue;
		if (auto* Widget = CreateWidget<UStagePerformanceHUDWidget>(Player, PerformanceHUDClass); Widget && Widget->AddToPlayerScreen(5))
		{
			Widget->SetSource(Stage);
			PerformanceWidgets.Add(Widget);
		}
	}
	for (UStagePerformanceHUDWidget* Widget : PerformanceWidgets) Widget->Refresh(Stage, Tags, CastOrder);
}

void UStageCueComponent::UpdateWorldSignals()
{
	const TArray<int32> Indices = GetVisibleIndices(true);
	const bool bShowPreparation = !DefinitionSource || DefinitionSource->bShowPreparationCues;
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
		FStageCueRuntime& Cue = Cues[Indices[Slot]];
		const EStageCueKind Kind = Cue.Definition.Kind;
		UTexture2D* Icon = Cue.Definition.Icon.Get();
		if (!Icon) Icon = SignalIcons.FindRef(Kind);
		if (!Icon) Icon = SignalIcons.FindRef(EStageCueKind::General);
		if (!Icon) continue;

		AActor* TrackedActor = Cue.ActorOverride.Get();
		const AActor* Recipient = nullptr;
		FVector TargetOffset = FVector::ZeroVector;
		if (!TrackedActor && Manager.IsValid())
		{
			for (const FStageCueTarget& Target : Cue.Definition.Targets)
			{
				if (Target.Type != EStageCueMarkerType::Recipient) continue;
				if (AActor* Actor = Manager->GetParticipant(Target.BindingId))
				{
					TrackedActor = Actor;
					Recipient = Actor;
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
			PresentWorldSignal(*Widget, Cue, Icon, Recipient, GetWorld()->GetTimeSeconds(), bShowPreparation);
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
	for (UStagePerformanceHUDWidget* Widget : PerformanceWidgets) if (IsValid(Widget)) Widget->RemoveFromParent();
	PerformanceWidgets.Reset();
	Super::EndPlay(Reason);
}
