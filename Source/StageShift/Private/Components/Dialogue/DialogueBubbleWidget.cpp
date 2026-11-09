// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Dialogue/DialogueViewModel.h"

void UDialogueBubbleWidget::OnDialogueViewModelSet_Implementation(UDialogueViewModel* ViewModel)
{
	DialogueViewModel = ViewModel;
	
	if (DialogueViewModel)
	{
		DialogueViewModel->OnDialogueLineEnded.AddDynamic(this, &ThisClass::OnDialogueLineEnded);
	}
}

void UDialogueBubbleWidget::OnDialogueLineEnded_Implementation(FGuid PlaybackId, EDialogueEndReason Reason)
{
}
