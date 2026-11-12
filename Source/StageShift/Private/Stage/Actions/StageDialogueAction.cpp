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
	if (bResumeLine)
	{
		if (!DialogueComponent->ResumeLine())
		{
			if (!DialogueComponent->PlayLine(CurrentLine))
			{
				Finish(EStageActionResult::Failed);
				return;
			}
		}
	}
	else
	{
		if (!DialogueComponent->PlayLine(CurrentLine))
		{
			Finish(EStageActionResult::Failed);
			return;
		}
	}
	
	if (
		DialogueComponent.IsValid() && 
		DialogueComponent->IsPlaying() &&
		GetState() == EStageExecutionState::Running)
	{
		CurrentPlaybackId = DialogueComponent->GetPlaybackState().PlaybackId;
	}
	else
	{
		Finish(EStageActionResult::Succeeded);
	}
}

void UStageDialogueAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!DialogueComponent.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	if (Mode == EStageActionStopMode::StopAction) DialogueComponent->StopPlayback();
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
	if (Reason == EDialogueEndReason::Completed)
	{
		bDialogueCompleted = true;
		Finish(EStageActionResult::Succeeded);
	}
	else
	{
		Finish(EStageActionResult::Interrupted);
	}
}

void UStageDialogueAction::HandleDialogueStateChanged(const FDialoguePlaybackState& PlaybackState)
{
	if (PlaybackState.Phase == EDialoguePlaybackPhase::Holding)
	{
		bDialogueCompleted = true;
	}
}