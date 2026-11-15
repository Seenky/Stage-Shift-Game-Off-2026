// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/StageDialogueAction.h"

#include "Components/Stage/StageMemberComponent.h"

void UStageDialogueAction::OnStart_Implementation()
{
	if (bDialogueCompleted)
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}
	
	if (!DialogueComponent.IsValid() || CurrentLine.Text.IsEmpty())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	DialogueComponent->OnDialogueLineEnded.AddDynamic(this, &ThisClass::HandleDialogueEnded);
	DialogueComponent->OnDialoguePlaybackStateChanged.AddDynamic(this, &ThisClass::HandleDialogueStateChanged);
    CurrentPlaybackId = FGuid::NewGuid(); // Identity precedes synchronous reveal/end callbacks.
    const bool bStarted = ResumeData.IsValid()
        ? (ResumeData.Line.Text.EqualTo(CurrentLine.Text) && DialogueComponent->ResumeSnapshotWithId(ResumeData, CurrentPlaybackId))
        : DialogueComponent->PlayLineWithId(CurrentLine, CurrentPlaybackId);
    if (!bStarted && GetState() != EStageExecutionState::Finished) Finish(EStageActionResult::Failed);

}

void UStageDialogueAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!DialogueComponent.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	if (Mode == EStageActionStopMode::StopAction)
    {
        if (DialogueComponent->GetPlaybackState().PlaybackId == CurrentPlaybackId)
        {
            DialogueComponent->CaptureResumeData(CurrentPlaybackId, ResumeData);
            DialogueComponent->StopPlayback();
        }
        if (GetState() != EStageExecutionState::Finished) Finish(EStageActionResult::Interrupted);
    }
}

void UStageDialogueAction::Cleanup_Implementation()
{
	Super::Cleanup_Implementation();

	if(DialogueComponent.IsValid())
	{
		DialogueComponent->OnDialogueLineEnded.RemoveAll(this);
		DialogueComponent->OnDialoguePlaybackStateChanged.RemoveAll(this);
		if (DialogueComponent->GetPlaybackState().PlaybackId == CurrentPlaybackId)
		{
			DialogueComponent->StopPlayback();
		}
	}
	DialogueComponent = nullptr;
	CurrentLine = FDialogueLine();
}

EStageActionStopMode UStageDialogueAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	return Reason == EStageActionStopReason::Cancel ? EStageActionStopMode::FinishAction : EStageActionStopMode::StopAction;
}

void UStageDialogueAction::SetupDialogueAction(
	const FDialogueLine& InLine,
	const bool bInResumeLine,
	const TObjectPtr<UDialogueComponent>& InDialogueComponent)
{
	CurrentLine = InLine;
	DialogueComponent = InDialogueComponent;
	bResumeLine = bInResumeLine;
}

void UStageDialogueAction::HandleDialogueEnded(FGuid PlaybackId, EDialogueEndReason Reason)
{
    if (PlaybackId != CurrentPlaybackId || GetState() == EStageExecutionState::Finished) return;
	if (Reason == EDialogueEndReason::Completed)
	{
		bDialogueCompleted = true;
		Finish(EStageActionResult::Succeeded);
	}
	else
	{
		Finish(GetState() == EStageExecutionState::Stopping ? EStageActionResult::Interrupted : EStageActionResult::Failed);
	}
}

void UStageDialogueAction::HandleDialogueStateChanged(const FDialoguePlaybackState& PlaybackState)
{
    if (PlaybackState.PlaybackId != CurrentPlaybackId) return;
    // Holding is still active playback, not completion of this command.
}
