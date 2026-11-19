#include "Stage/Cues/StageCueComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/StageCondition.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Cues/StageCueWorldMarker.h"
#include "UI/StageCueWidget.h"
#include "TimerManager.h"

UStageCueComponent::UStageCueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	if (bEndingPlay || !GetWorld() || Definition.CueId.IsNone() || Definition.Title.IsEmpty() || !FMath::IsFinite(Definition.TimeLimit) || Definition.TimeLimit < 0.0f)
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
		Cue->Deadline = Cue->View.bHasDeadline ? GetWorld()->GetTimeSeconds() + Seconds : 0.0;
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

void UStageCueComponent::ReportProblem(FName CueId, const FText& Details)
{
	if (auto* Cue = Cues.FindByPredicate([&](const auto& Entry) { return Entry.View.CueId == CueId; }))
	{
		if (Cue->View.State == EStageCueState::Completed) return;
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
	Cues.RemoveAll([&](const auto& Cue) { return Cue.View.State == EStageCueState::Completed && Now - Cue.CompletedAt >= CompletedDisplayTime; });
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

TArray<int32> UStageCueComponent::GetVisibleIndices() const
{
	TArray<int32> Indices;
	for (int32 Index = 0; Index < Cues.Num(); ++Index) Indices.Add(Index);
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

TArray<AStageCueWorldMarker*> UStageCueComponent::GetVisibleMarkers() const
{
	TArray<AStageCueWorldMarker*> Result;
	for (const auto& Pair : Markers) if (IsValid(Pair.Value)) Result.Add(Pair.Value);
	return Result;
}

void UStageCueComponent::ResetCues()
{
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
