// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/StageDialogueAction.h"

#include "Components/Stage/StageMemberComponent.h"

void UStageDialogueAction::OnStart_Implementation()
{
	if (!DialogueComponent.IsValid() || CurrentLine.Text.IsEmpty())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	DialogueComponent->OnDialogueLineEnded.AddDynamic(this, &ThisClass::HandleDialogueEnded);
	DialogueComponent->PlayLine(CurrentLine);
	CurrentPlaybackId = DialogueComponent->GetPlaybackState().PlaybackId;
}

void UStageDialogueAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!DialogueComponent.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	DialogueComponent->StopPlayback();
}

void UStageDialogueAction::Cleanup_Implementation()
{
	Super::Cleanup_Implementation();

	if(DialogueComponent.IsValid()) DialogueComponent->StopPlayback();
	DialogueComponent = nullptr;
	CurrentLine = FDialogueLine();
}

EStageActionStopMode UStageDialogueAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	return Reason == EStageActionStopReason::Cancel ? EStageActionStopMode::FinishAction : EStageActionStopMode::StopAction;
}

void UStageDialogueAction::SetupDialogueAction(
	const FDialogueLine& InLine,
	const TObjectPtr<UDialogueComponent>& InDialogueComponent)
{
	CurrentLine = InLine;
	DialogueComponent = InDialogueComponent;
}

void UStageDialogueAction::HandleDialogueEnded(FGuid PlaybackId, EDialogueEndReason Reason)
{
	if (Reason == EDialogueEndReason::Completed)
	{
		Finish(EStageActionResult::Succeeded);
	}
	else
	{
		Finish(EStageActionResult::Interrupted);
	}
}
