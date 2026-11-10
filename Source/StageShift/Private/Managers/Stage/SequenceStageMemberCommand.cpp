#include "Managers/Stage/SequenceStageMemberCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "StageCommandContainerHelpers.h"

float USequenceStageMemberCommand::GetNeededTime_Implementation() const
{
	if (!StageCommandContainer::HasValidChildren(this)) return 0.0f;
	float Duration = 0.0f;
	for (const UStageMemberCommand* Child : Children) Duration += StageCommandContainer::NeededTime(Child);
	return Duration;
}

float USequenceStageMemberCommand::GetRemainingTime_Implementation() const
{
	if (GetState() == EStageExecutionState::Finished || !StageCommandContainer::HasValidChildren(this)) return 0.0f;
	float Remaining = 0.0f;
	for (const UStageMemberCommand* Child : Children)
	{
		if (Child->GetState() == EStageExecutionState::NotStarted) Remaining += StageCommandContainer::NeededTime(Child);
		else if (Child->GetState() != EStageExecutionState::Finished) Remaining += Child->GetRemainingTime();
	}
	return Remaining;
}

void USequenceStageMemberCommand::ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot,
	float InPlannedStartTime, float InPlannedEndTime)
{
	for (UStageMemberCommand* Child : Children)
		if (IsValid(Child)) Child->OnFinished.RemoveDynamic(this, &USequenceStageMemberCommand::HandleChildFinished);
	Super::ResetRuntime(InContext, InRoot, InPlannedStartTime, InPlannedEndTime);
	CurrentChildIndex = INDEX_NONE;
	ChildResults.Init(EStageCommandResult::None, Children.Num());
	if (!StageCommandContainer::HasValidChildren(this)) return;

	float ChildStart = InPlannedStartTime;
	for (UStageMemberCommand* Child : Children)
	{
		const float ChildEnd = ChildStart + StageCommandContainer::NeededTime(Child);
		Child->ResetRuntime(InContext, GetRootCommand(), ChildStart, ChildEnd);
		ChildStart = ChildEnd;
	}
}

void USequenceStageMemberCommand::CheckChildrenResults()
{
	ChildResults.SetNum(Children.Num());
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		UStageMemberCommand* Child = Children[Index];
		const bool bOwnedChild = IsValid(Child) && Child->GetOuter() == this;
		ChildResults[Index] = !bOwnedChild ? EStageCommandResult::Failed : Child->GetResult();
		if (bOwnedChild && Child->GetState() != EStageExecutionState::Finished)
			Child->OnFinished.AddUniqueDynamic(this, &USequenceStageMemberCommand::HandleChildFinished);
	}
}

void USequenceStageMemberCommand::BeginWork_Implementation()
{
	if (!StageCommandContainer::HasValidChildren(this) || !Context.MemberComponent.IsValid())
	{
		Finish(EStageCommandResult::Failed);
		return;
	}
	CheckChildrenResults();
	CurrentChildIndex = 0;
	StartNextChild();
}

void USequenceStageMemberCommand::StartNextChild()
{
	if (GetState() != EStageExecutionState::Running) return;
	if (!Children.IsValidIndex(CurrentChildIndex))
	{
		TryFinish();
		return;
	}
	if (ChildResults[CurrentChildIndex] != EStageCommandResult::None)
	{
		++CurrentChildIndex;
		StartNextChild();
		return;
	}
	UStageMemberCommand* Child = Children[CurrentChildIndex];
	if (Child->GetState() == EStageExecutionState::NotStarted)
		Context.MemberComponent->StartPreparedCommand(Child);
}

void USequenceStageMemberCommand::HandleChildFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	const int32 Index = Children.IndexOfByKey(Command);
	if (!ChildResults.IsValidIndex(Index) || ChildResults[Index] != EStageCommandResult::None) return;
	ChildResults[Index] = Result;
	Command->OnFinished.RemoveDynamic(this, &USequenceStageMemberCommand::HandleChildFinished);
	if (GetState() == EStageExecutionState::Running && Index == CurrentChildIndex)
	{
		++CurrentChildIndex;
		StartNextChild();
	}
	else if (GetState() == EStageExecutionState::Stopping) TryFinish();
}

void USequenceStageMemberCommand::StopWork_Implementation(EStageActionStopReason Reason)
{
	CheckChildrenResults();
	for (UStageMemberCommand* Child : Children)
	{
		if (!IsValid(Child) || Child->GetOuter() != this || Child->GetState() == EStageExecutionState::Finished) continue;
		if (Child->GetState() == EStageExecutionState::NotStarted && Context.MemberComponent.IsValid())
			Context.MemberComponent->CompleteBeforeStart(Child, EStageCommandResult::Cancelled);
		else Child->RequestStop(Reason);
	}
	TryFinish();
}

void USequenceStageMemberCommand::TryFinish()
{
	if (GetState() == EStageExecutionState::Finished) return;
	const EStageCommandResult Result = StageCommandContainer::ResolveResult(ChildResults);
	if (Result == EStageCommandResult::None) return;
	for (UStageMemberCommand* Child : Children)
		if (IsValid(Child)) Child->OnFinished.RemoveDynamic(this, &USequenceStageMemberCommand::HandleChildFinished);
	Finish(GetState() == EStageExecutionState::Stopping ? EStageCommandResult::Cancelled : Result);
}

void USequenceStageMemberCommand::FreezeFinaleChoice_Implementation(const FStageExecutionContext& InContext)
{
	if (!StageCommandContainer::HasValidChildren(this)) return;
	for (UStageMemberCommand* Child : Children) Child->FreezeFinaleChoice(InContext);
}
