#include "Stage/Core/StageAction.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Core/StageManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UStageAction::ForceAbort(const FText& Reason)
{
	if (ActionState == EStageExecutionState::Finished) return;
	TerminationReason = Reason;
	if (Context.Manager.IsValid()) Context.Manager->AddDiagnostic(GetClass()->GetName() + TEXT(" / ") + Reason.ToString());
	ActionState = EStageExecutionState::Running;
	Finish(EStageActionResult::Failed);
}

void UStageAction::HandleWatchdog()
{
	ForceAbort(FText::FromString(TEXT("Action watchdog expired")));
}

void UStageAction::SetExecutionBudget(float Seconds)
{
	if (!FMath::IsFinite(Seconds) || Seconds <= WatchdogSeconds) return;
	WatchdogSeconds = Seconds;
	if (GetWorld() && ActionState == EStageExecutionState::Running)
		GetWorld()->GetTimerManager().SetTimer(WatchdogHandle, this, &ThisClass::HandleWatchdog, WatchdogSeconds, false);
}

void UStageAction::ValidateRecoveryTemplate(const FString& Path, TArray<FString>& Errors) const
{
	Errors.Add(Path + TEXT(" / action has no configured executable recovery template"));
}

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

	if (!FMath::IsFinite(WatchdogSeconds) || WatchdogSeconds <= 0)
	{
		ForceAbort(FText::FromString(TEXT("Invalid Action watchdog budget")));
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(WatchdogHandle, this, &ThisClass::HandleWatchdog, WatchdogSeconds, false);
	OnStart();
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
		const bool bFinalStopAfterInterruption = (Reason == EStageActionStopReason::Cancel && ActionStopReason == EStageActionStopReason::Interrupt);
		if (!bFinalStopAfterInterruption) return;
	}

	ActionStopReason = Reason;
	ActionState = EStageExecutionState::Stopping;
	const EStageActionStopMode RequestedMode = GetStopMode(Reason);

	if (ActionState != EStageExecutionState::Stopping || ActionStopReason != Reason) return;

	ActionStopMode = bWasStopping && PreviousStopMode == EStageActionStopMode::StopAction ? EStageActionStopMode::StopAction : RequestedMode;
	if (!bWasStopping || ActionStopMode != PreviousStopMode)
	{
		OnStop(ActionStopMode);
	}
}

void UStageAction::Finish(EStageActionResult InResult)
{
	if (ActionState != EStageExecutionState::Running && ActionState != EStageExecutionState::Stopping) return;

	if (!ensureMsgf(InResult != EStageActionResult::None, TEXT("An action cannot finish with result None."))) return;

	if (ActionState == EStageExecutionState::Stopping
	    && (InResult == EStageActionResult::Interrupted || InResult == EStageActionResult::Cancelled
	        || (InResult == EStageActionResult::Succeeded && ActionStopMode == EStageActionStopMode::StopAction)))
	{
		InResult = ActionStopReason == EStageActionStopReason::Interrupt ? EStageActionResult::Interrupted : EStageActionResult::Cancelled;
	}

	ActionResult = InResult;
	ActionState = EStageExecutionState::Finished;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WatchdogHandle);
	const TWeakObjectPtr<UStageMemberComponent> Member = Context.MemberComponent;
	if (Member.IsValid()) Member->BeginDispatch();
	Cleanup();
	OnFinished.Broadcast(this, ActionResult);
	if (Member.IsValid()) Member->EndDispatch();
}

void UStageAction::OnStart_Implementation()
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
		Finish(ActionStopReason == EStageActionStopReason::Interrupt ? EStageActionResult::Interrupted : EStageActionResult::Cancelled);
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
