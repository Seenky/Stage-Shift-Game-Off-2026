#include "Stage/Core/ParallelStageMemberCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "StageCommandContainerHelpers.h"

void UParallelStageMemberCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (bFinishOnPrimaryBranch && (!PrimaryBranch || !Children.Contains(PrimaryBranch)))
		Errors.Add(Path + TEXT(" / Select a primary branch for Parallel."));
}

float UParallelStageMemberCommand::GetNeededTime_Implementation() const
{
	if (!StageCommandContainer::HasValidChildren(this)) return 0.0f;
	if (bFinishOnPrimaryBranch && PrimaryBranch && Children.Contains(PrimaryBranch)) return StageCommandContainer::NeededTime(PrimaryBranch);
	float Duration = 0.0f;
	for (const UStageMemberCommand* Child : Children)
		Duration = FMath::Max(Duration, StageCommandContainer::NeededTime(Child));
	return Duration;
}

float UParallelStageMemberCommand::GetRemainingTime_Implementation() const
{
	if (GetState() == EStageExecutionState::Finished || !StageCommandContainer::HasValidChildren(this)) return 0.0f;
	if (bFinishOnPrimaryBranch && PrimaryBranch && Children.Contains(PrimaryBranch) && GetState() != EStageExecutionState::Stopping)
		return PrimaryBranch->GetState() == EStageExecutionState::NotStarted ? StageCommandContainer::NeededTime(PrimaryBranch) : PrimaryBranch->GetRemainingTime();
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

void UParallelStageMemberCommand::ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime)
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
		if (bOwnedChild && Child->GetState() != EStageExecutionState::Finished) Child->OnFinished.AddUniqueDynamic(this, &UParallelStageMemberCommand::HandleChildFinished);
	}
}

void UParallelStageMemberCommand::OnStart_Implementation()
{
	UStageMemberCommand* Primary = bFinishOnPrimaryBranch ? PrimaryBranch.Get() : nullptr;
	if (!StageCommandContainer::HasValidChildren(this) || !Context.MemberComponent.IsValid() || (bFinishOnPrimaryBranch && (!Primary || !Children.Contains(Primary))))
	{
		Finish(EStageCommandResult::Failed);
		return;
	}
	CheckChildrenResults(); // Subscribe every branch before any branch can finish synchronously.
	bIssuingChildren = true;
	if (Primary && Primary->GetState() == EStageExecutionState::NotStarted) Context.MemberComponent->StartPreparedCommand(Primary);
	for (UStageMemberCommand* Child : Children)
	{
		if (GetState() != EStageExecutionState::Running || (Primary && Primary->GetState() == EStageExecutionState::Finished)) break;
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

void UParallelStageMemberCommand::OnStop_Implementation(EStageActionStopReason Reason)
{
	CheckChildrenResults();
	StopChildren(Reason);
	TryFinish();
}

void UParallelStageMemberCommand::StopChildren(EStageActionStopReason Reason)
{
	TGuardValue<bool> Issuing(bIssuingChildren, true);
	for (UStageMemberCommand* Child : Children)
	{
		if (!IsValid(Child) || Child->GetOuter() != this || Child->GetState() == EStageExecutionState::Finished) continue;
		if (Child->GetState() == EStageExecutionState::NotStarted && Context.MemberComponent.IsValid())
			Context.MemberComponent->CompleteBeforeStart(Child, EStageCommandResult::Cancelled);
		else
			Child->RequestStop(Reason);
	}
}

void UParallelStageMemberCommand::TryFinish()
{
	if (GetState() == EStageExecutionState::Finished || bIssuingChildren) return;
	const bool bUsePrimary = bFinishOnPrimaryBranch && GetState() == EStageExecutionState::Running;
	const int32 PrimaryIndex = bUsePrimary ? Children.IndexOfByKey(PrimaryBranch) : INDEX_NONE;
	if (bUsePrimary && !ChildResults.IsValidIndex(PrimaryIndex)) return;
	const EStageCommandResult Result = bUsePrimary ? ChildResults[PrimaryIndex] : StageCommandContainer::ResolveResult(ChildResults);
	if (Result == EStageCommandResult::None) return;
	if (bUsePrimary)
	{
		StopChildren(EStageActionStopReason::ParallelBranchFinished);
		if (ChildResults.Contains(EStageCommandResult::None)) return;
	}
	for (UStageMemberCommand* Child : Children)
		if (IsValid(Child)) Child->OnFinished.RemoveDynamic(this, &UParallelStageMemberCommand::HandleChildFinished);
	Finish(GetState() == EStageExecutionState::Stopping ? EStageCommandResult::Cancelled : Result);
}
