// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/StageDialogueCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/StageDialogueAction.h"

float UStageDialogueCommand::GetNeededTime_Implementation() const
{
	if (CurrentLine.Text.IsEmpty()) return 3.f;
	
	TArray<FString> Words;
	CurrentLine.Text.ToString().ParseIntoArray(Words, TEXT(" "), true);
	
	float NeededTime = CurrentLine.HoldDuration + (Words.Num() - 1) * CurrentLine.WordInterval;
	for (auto Word : Words)
	{
		NeededTime += Word.Len() * CurrentLine.LetterInterval;
	}
	
	return NeededTime;
}

UStageAction* UStageDialogueCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner()) return nullptr;
	
	const auto DialogueComponent = InContext.MemberComponent.Get()->GetOwner()->FindComponentByClass<UDialogueComponent>();
	if (!DialogueComponent) return nullptr;
	
	UStageDialogueAction* DialogueAction = NewObject<UStageDialogueAction>(this);
	if (!DialogueAction) return nullptr;
	
	DialogueAction->SetupDialogueAction(CurrentLine, DialogueComponent);
	
	return DialogueAction;
}