#include "Stage/Core/StageAction.h"

#include "Components/Stage/StageMemberComponent.h"

void UStageAction::Start(const FStageExecutionContext& InContext)
{
	if (ActionState != EStageExecutionState::NotStarted) return;

	Context = InContext;
	ActionState = EStageExecutionState::Running;

	if (!Context.Manager.IsValid() || !Context.MemberComponent.IsValid() || !GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	Execute();
}

bool UStageAction::CanContinue_Implementation() const
{
	return true;
}

void UStageAction::RequestStop(EStageActionStopReason Reason)
{
	if (ActionState != EStageExecutionState::Running && ActionState != EStageExecutionState::Stopping) return;

	const bool bWasStopping = ActionState == EStageExecutionState::Stopping;
	const EStageActionStopMode PreviousStopMode = ActionStopMode;
	if (bWasStopping)
	{
		const bool bFinalStopAfterInterruption =
			(Reason == EStageActionStopReason::Finished && ActionStopReason == EStageActionStopReason::Interrupted);
		if (!bFinalStopAfterInterruption) return;
	}

	ActionStopReason = Reason;
	ActionState = EStageExecutionState::Stopping;
	const EStageActionStopMode RequestedMode = GetStopMode(Reason);
	
	if (ActionState != EStageExecutionState::Stopping || ActionStopReason != Reason) return;

	ActionStopMode = bWasStopping && PreviousStopMode == EStageActionStopMode::StopAction
		? EStageActionStopMode::StopAction : RequestedMode;
	if (!bWasStopping || ActionStopMode != PreviousStopMode)
	{
		OnStop(ActionStopMode);
	}
}

void UStageAction::Finish(EStageActionResult InResult)
{
	if (ActionState != EStageExecutionState::Running && ActionState != EStageExecutionState::Stopping) return;
	
	if (!ensureMsgf(InResult != EStageActionResult::None, TEXT("An action cannot finish with result None."))) return;

	if (ActionState == EStageExecutionState::Stopping &&
		(InResult == EStageActionResult::Interrupted || InResult == EStageActionResult::Cancelled ||
		 (InResult == EStageActionResult::Succeeded && ActionStopMode == EStageActionStopMode::StopAction)))
	{
		InResult = ActionStopReason == EStageActionStopReason::Interrupted
			? EStageActionResult::Interrupted : EStageActionResult::Cancelled;
	}
	
	ActionResult = InResult;
	ActionState = EStageExecutionState::Finished;
	const TWeakObjectPtr<UStageMemberComponent> Member = Context.MemberComponent;
	if (Member.IsValid()) Member->BeginDispatch();
	Cleanup();
	OnFinished.Broadcast(this, ActionResult);
	if (Member.IsValid()) Member->EndDispatch();
}

void UStageAction::Execute_Implementation()
{
	Finish(EStageActionResult::Failed);
}

EStageActionStopMode UStageAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	return EStageActionStopMode::StopAction;
}

void UStageAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (Mode == EStageActionStopMode::StopAction)
	{
		Finish(ActionStopReason == EStageActionStopReason::Interrupted
			? EStageActionResult::Interrupted : EStageActionResult::Cancelled);
	}
}

void UStageAction::Cleanup_Implementation()
{
}

UWorld* UStageAction::GetWorld() const
{
	const UStageMemberComponent* Member = Context.MemberComponent.Get();
	return Member ? Member->GetWorld() : Super::GetWorld();
}
