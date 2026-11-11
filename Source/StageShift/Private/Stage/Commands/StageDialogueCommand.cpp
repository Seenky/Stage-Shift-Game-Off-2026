// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/StageDialogueCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/StageDialogueAction.h"

float UStageDialogueCommand::GetNeededTime_Implementation() const
{
	if (CurrentLine.Text.IsEmpty()) return 0.f;
	
	TArray<FString> Words;
	CurrentLine.Text.ToString().ParseIntoArray(Words, TEXT(" "), true);
	
	if (Words.IsEmpty()) return 0.f;
	
	float NeededTime = CurrentLine.HoldDuration + (Words.Num() - 1) * CurrentLine.WordInterval;
	
	for (auto Word : Words)
	{
		NeededTime += Word.Len() * CurrentLine.LetterInterval;
	}
	
	NeededTime -= CurrentLine.LetterInterval;
	
	return NeededTime;
}

UStageAction* UStageDialogueCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner()) return nullptr;
	
	const auto DialogueComponent = InContext.MemberComponent.Get()->GetOwner()->FindComponentByClass<UDialogueComponent>();
	if (!DialogueComponent) return nullptr;
	
	UStageDialogueAction* DialogueAction = NewObject<UStageDialogueAction>(InContext.MemberComponent.Get());
	if (!DialogueAction) return nullptr;
	
	DialogueAction->SetupDialogueAction(CurrentLine, bIsNeedResumeLine, DialogueComponent);
	
	return DialogueAction;
}

void UStageDialogueCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
	bIsNeedResumeLine = ActionResult == EStageActionResult::Interrupted;
	if (const auto DialogueAction = Cast<UStageDialogueAction>(Action))
	{
		bDialogueCompleted = DialogueAction->GetIsDialogueCompleted();
	}
}

bool UStageDialogueCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	return bDialogueCompleted;
}
