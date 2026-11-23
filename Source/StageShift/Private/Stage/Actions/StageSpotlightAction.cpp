#include "Stage/Actions/StageSpotlightAction.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Cues/StageCueComponent.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "StageSpotlightAction"

void UStageSpotlightAction::Setup(AStageSpotlightControl* InSpotlight, AActor* InFocusActor, FName InTargetRoleId, const FText& InTargetName,
    float InTimeLimit, FName InCueId)
{
	Spotlight = InSpotlight;
	FocusActor = InFocusActor;
	TargetRoleId = InTargetRoleId;
	TargetName = InTargetName;
	TimeLimit = InTimeLimit;
	CueId = InCueId;
}

void UStageSpotlightAction::OnStart_Implementation()
{
	auto* Manager = Context.Manager.Get();
	auto* Member = Context.MemberComponent.Get();
	auto* Lamp = Spotlight.Get();
	AActor* Actor = FocusActor.Get();
	if (!Manager || !Manager->PlayerCues || !Member || !Lamp || !Actor)
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	if (!Lamp->ClaimCue(*this, CueId, Actor))
	{
		ForceAbort(LOCTEXT("SpotlightUnavailable", "The spotlight is unavailable or already assigned to another Command."));
		return;
	}
	if (GetState() != EStageExecutionState::Running) return;

	Lamp->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSpotlightChanged);
	Lamp->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleActorEndPlay);
	Actor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleActorEndPlay);
	Deadline = GetWorld()->GetTimeSeconds() + TimeLimit;
	GetWorld()->GetTimerManager().SetTimer(Timeout, this, &ThisClass::HandleTimeout, TimeLimit, false);

	FStageCueDefinition Cue;
	Cue.CueId = CueId;
	Cue.Kind = EStageCueKind::Lighting;
	Cue.Title = LOCTEXT("Title", "Направь прожектор");
	Cue.Details = FText::Format(LOCTEXT("Target", "Цель: {0}"), TargetName);
	Cue.TimeLimit = TimeLimit;
	FStageCueTarget& Marker = Cue.Targets.AddDefaulted_GetRef();
	Marker.Type = EStageCueMarkerType::Recipient;
	Marker.BindingId = TargetRoleId;
	Manager->PlayerCues->DismissLightingPreparation(TargetRoleId);
	bCueShown = true;
	if (!Manager->PlayerCues->ShowActionCue(Cue, *Member))
	{
		bCueShown = false;
		Finish(EStageActionResult::Failed);
	}
}

void UStageSpotlightAction::HandleSpotlightChanged(FName ChangedCueId, AActor* Target, EStageSpotlightState State)
{
	if (ChangedCueId != CueId || Target != FocusActor.Get() || State != EStageSpotlightState::TargetLit || !GetWorld()) return;
	Finish(GetWorld()->GetTimeSeconds() <= Deadline ? EStageActionResult::Succeeded : EStageActionResult::Failed);
}

void UStageSpotlightAction::HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	Finish(EStageActionResult::Failed);
}

void UStageSpotlightAction::HandleTimeout()
{
	Finish(EStageActionResult::Failed);
}

void UStageSpotlightAction::Cleanup_Implementation()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(Timeout);

	auto* Lamp = Spotlight.Get();
	if (Lamp)
	{
		Lamp->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleSpotlightChanged);
		Lamp->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleActorEndPlay);
	}
	if (AActor* Actor = FocusActor.Get()) Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleActorEndPlay);

	const bool bSucceeded = GetResult() == EStageActionResult::Succeeded;
	if (Lamp) Lamp->ReleaseCue(*this, bSucceeded);
	if (bCueShown && Context.Manager.IsValid() && Context.Manager->PlayerCues)
		Context.Manager->PlayerCues->FinishActionCue(CueId, bSucceeded);
}

#undef LOCTEXT_NAMESPACE
