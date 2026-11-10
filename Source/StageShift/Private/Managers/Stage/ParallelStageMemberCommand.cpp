#include "Managers/Stage/ParallelStageMemberCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "StageCommandContainerHelpers.h"

float UParallelStageMemberCommand::GetNeededTime_Implementation() const
{
	if (!StageCommandContainer::HasValidChildren(this)) return 0.0f;
	float Duration = 0.0f;
	for (const UStageMemberCommand* Child : Children) Duration = FMath::Max(Duration, StageCommandContainer::NeededTime(Child));
	return Duration;
}

float UParallelStageMemberCommand::GetRemainingTime_Implementation() const
{
	if (GetState() == EStageExecutionState::Finished || !StageCommandContainer::HasValidChildren(this)) return 0.0f;
	float Remaining = 0.0f;
	for (const UStageMemberCommand* Child : Children)
	{
		if (Child->GetState() == EStageExecutionState::NotStarted)
			Remaining = FMath::Max(Remaining, StageCommandContainer::NeededTime(Child));
		else if (Child->GetState() != EStageExecutionState::Finished)
			Remaining = FMath::Max(Remaining, Child->GetRemainingTime());
	}
	return Remaining;
}

void UParallelStageMemberCommand::ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot,
	float InPlannedStartTime, float InPlannedEndTime)
{
	for (UStageMemberCommand* Child : Children)
		if (IsValid(Child)) Child->OnFinished.RemoveDynamic(this, &UParallelStageMemberCommand::HandleChildFinished);
	Super::ResetRuntime(InContext, InRoot, InPlannedStartTime, InPlannedEndTime);
	bIssuingChildren = false;
	ChildResults.Init(EStageCommandResult::None, Children.Num());
	if (!StageCommandContainer::HasValidChildren(this)) return;
	for (UStageMemberCommand* Child : Children)
		Child->ResetRuntime(InContext, GetRootCommand(), InPlannedStartTime, InPlannedStartTime + StageCommandContainer::NeededTime(Child));
}

void UParallelStageMemberCommand::CheckChildrenResults()
{
	ChildResults.SetNum(Children.Num());
	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		UStageMemberCommand* Child = Children[Index];
		const bool bOwnedChild = IsValid(Child) && Child->GetOuter() == this;
		ChildResults[Index] = !bOwnedChild ? EStageCommandResult::Failed : Child->GetResult();
		if (bOwnedChild && Child->GetState() != EStageExecutionState::Finished)
			Child->OnFinished.AddUniqueDynamic(this, &UParallelStageMemberCommand::HandleChildFinished);
	}
}

void UParallelStageMemberCommand::BeginWork_Implementation()
{
	if (!StageCommandContainer::HasValidChildren(this) || !Context.MemberComponent.IsValid())
	{
		Finish(EStageCommandResult::Failed);
		return;
	}
	CheckChildrenResults(); // Subscribe every branch before any branch can finish synchronously.
	bIssuingChildren = true;
	for (UStageMemberCommand* Child : Children)
	{
		if (GetState() != EStageExecutionState::Running) break;
		if (Child->GetState() == EStageExecutionState::NotStarted) Context.MemberComponent->StartPreparedCommand(Child);
	}
	bIssuingChildren = false;
	TryFinish();
}

void UParallelStageMemberCommand::HandleChildFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	const int32 Index = Children.IndexOfByKey(Command);
	if (!ChildResults.IsValidIndex(Index) || ChildResults[Index] != EStageCommandResult::None) return;
	ChildResults[Index] = Result;
	Command->OnFinished.RemoveDynamic(this, &UParallelStageMemberCommand::HandleChildFinished);
	TryFinish();
}

void UParallelStageMemberCommand::StopWork_Implementation(EStageActionStopReason Reason)
{
	CheckChildrenResults();
	const bool bWasIssuingChildren = bIssuingChildren;
	bIssuingChildren = true;
	for (UStageMemberCommand* Child : Children)
	{
		if (!IsValid(Child) || Child->GetOuter() != this || Child->GetState() == EStageExecutionState::Finished) continue;
		if (Child->GetState() == EStageExecutionState::NotStarted && Context.MemberComponent.IsValid())
			Context.MemberComponent->CompleteBeforeStart(Child, EStageCommandResult::Cancelled);
		else Child->RequestStop(Reason);
	}
	bIssuingChildren = bWasIssuingChildren;
	TryFinish();
}

void UParallelStageMemberCommand::TryFinish()
{
	if (GetState() == EStageExecutionState::Finished || bIssuingChildren) return;
	const EStageCommandResult Result = StageCommandContainer::ResolveResult(ChildResults);
	if (Result == EStageCommandResult::None) return;
	for (UStageMemberCommand* Child : Children)
		if (IsValid(Child)) Child->OnFinished.RemoveDynamic(this, &UParallelStageMemberCommand::HandleChildFinished);
	Finish(GetState() == EStageExecutionState::Stopping ? EStageCommandResult::Cancelled : Result);
}

void UParallelStageMemberCommand::FreezeFinaleChoice_Implementation(const FStageExecutionContext& InContext)
{
	if (!StageCommandContainer::HasValidChildren(this)) return;
	for (UStageMemberCommand* Child : Children) Child->FreezeFinaleChoice(InContext);
}
