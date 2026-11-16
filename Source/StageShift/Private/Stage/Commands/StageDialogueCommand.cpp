// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Commands/StageDialogueCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Stage/Actions/StageDialogueAction.h"

void UStageDialogueCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	TArray<int32> Letters, Words;
	StageDialogue::Tokenize(CurrentLine.Text.ToString(), Letters, Words);
	if (Words.IsEmpty()) Errors.Add(Path + TEXT(" / dialogue text must contain words"));
	if (!FMath::IsFinite(CurrentLine.HoldDuration) || CurrentLine.HoldDuration < 0 || !FMath::IsFinite(CurrentLine.WordInterval) || CurrentLine.WordInterval < 0
	    || !FMath::IsFinite(CurrentLine.LetterInterval) || CurrentLine.LetterInterval < 0)
		Errors.Add(Path + TEXT(" / dialogue timing must be finite and nonnegative"));
	for (const auto& Cue : CurrentLine.Cues)
		if (Cue.WordIndex < 0 || Cue.WordIndex >= Words.Num() || !FMath::IsFinite(Cue.BlendTime) || Cue.BlendTime < 0 || !FMath::IsFinite(Cue.Value) || Cue.Value < 0
		    || Cue.Value > 1)
			Errors.Add(Path + TEXT(" / invalid dialogue cue index/value/blend time"));
}

UStageDialogueCommand::UStageDialogueCommand()
{
	bRequireStageLocation = true;
	bRequireStageYaw = true;
}

float UStageDialogueCommand::GetNeededTime_Implementation() const
{
	return StageDialogue::GetDuration(CurrentLine);
}

UStageAction* UStageDialogueCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner()) return nullptr;

	const auto DialogueComponent = InContext.MemberComponent.Get()->GetOwner()->FindComponentByClass<UDialogueComponent>();
	if (!DialogueComponent) return nullptr;

	UStageDialogueAction* DialogueAction = NewObject<UStageDialogueAction>(InContext.MemberComponent.Get());
	if (!DialogueAction) return nullptr;

	DialogueAction->SetupDialogueAction(CurrentLine, false, DialogueComponent);
	if (ResumeData.IsValid()) DialogueAction->SetupResumeSnapshot(ResumeData);

	return DialogueAction;
}

void UStageDialogueCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{

	if (const auto DialogueAction = Cast<UStageDialogueAction>(Action))
	{
		bDialogueCompleted = DialogueAction->GetIsDialogueCompleted();
		ResumeData = ActionResult == EStageActionResult::Interrupted ? DialogueAction->GetResumeSnapshot() : FDialogueResumeData{};
	}
}

bool UStageDialogueCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	return bDialogueCompleted;
}

void UStageDialogueCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	const auto* Member = InContext.MemberComponent.Get();
	const AActor* Actor = Member ? Member->GetOwner() : nullptr;
	if (IsValid(Actor) && !Actor->FindComponentByClass<UDialogueComponent>()) Errors.Add(Path + TEXT(" / Speak requires DialogueComponent"));
}
