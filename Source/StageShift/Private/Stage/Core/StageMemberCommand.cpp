#include "Stage/Core/StageMemberCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Stage/Core/StageAction.h"
#include "Stage/Core/StageManager.h"

void UStageMemberCommand::PreStart(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot)
{
	if (bPreStarted || CommandState != EStageExecutionState::NotStarted) return;

	bPreStarted = true;
	Context = InContext;
	RootCommand = InRoot ? InRoot : this;
	for (UStageMemberCommand* Child : GetChildCommands())
		if (IsValid(Child) && Child->GetOuter() == this) Child->PreStart(InContext, GetRootCommand());
	OnPreStart(InContext);
}

void UStageMemberCommand::Start(const FStageExecutionContext& InContext)
{
	if (CommandState != EStageExecutionState::NotStarted) return;

	Context = InContext;
	if (!RootCommand.IsValid()) RootCommand = this;
	CommandState = EStageExecutionState::Running;

	if (!Context.Manager.IsValid() || !Context.MemberComponent.IsValid() || !GetWorld())
	{
		Finish(EStageCommandResult::Failed);
		return;
	}

	const TWeakObjectPtr<UStageMemberComponent> Member = Context.MemberComponent;
	if (Member.IsValid()) Member->BeginDispatch();
	OnStart();
	if (Member.IsValid()) Member->EndDispatch();
}

void UStageMemberCommand::RequestStop(EStageActionStopReason Reason)
{
	if (CommandState == EStageExecutionState::Finished || CommandState == EStageExecutionState::Stopping) return;
	
	CommandState = EStageExecutionState::Stopping;
	const TWeakObjectPtr<UStageMemberComponent> Member = Context.MemberComponent;
	if (Member.IsValid()) Member->BeginDispatch();
	OnStop(Reason);
	if (Member.IsValid()) Member->EndDispatch();
}

const TArray<TObjectPtr<UStageMemberCommand>>& UStageMemberCommand::GetChildCommands() const
{
	static const TArray<TObjectPtr<UStageMemberCommand>> Empty;
	return Empty;
}

float UStageMemberCommand::GetNeededTime_Implementation() const
{
	return 0.0f;
}

float UStageMemberCommand::GetRemainingTime_Implementation() const
{
	if (CommandState == EStageExecutionState::Finished) return 0.0f;
	return FMath::Max(0.0f, GetNeededTime() - GetAccumulatedActiveTime());
}

bool UStageMemberCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	return bHasSuccessfulMainAction;
}


UStageAction* UStageMemberCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	return nullptr;
}

void UStageMemberCommand::NotifyMainActionStarted()
{
	if (CommandState != EStageExecutionState::Running || CurrentStartTime >= 0.0f) return;
	bHasStartedMainAction = true;
	if (const UWorld* World = GetWorld()) CurrentStartTime = static_cast<float>(World->GetTimeSeconds());
}

float UStageMemberCommand::GetAccumulatedActiveTime() const
{
	float Elapsed = AccumulatedActiveTime;
	if (CurrentStartTime >= 0.0f)
	{
		if (const UWorld* World = GetWorld())
		{
			Elapsed += FMath::Max(0.0f, static_cast<float>(World->GetTimeSeconds()) - CurrentStartTime);
		}
	}
	return Elapsed;
}

void UStageMemberCommand::SaveAccumulatedActiveTime()
{
	AccumulatedActiveTime = GetAccumulatedActiveTime();
	CurrentStartTime = -1.0f;
}

void UStageMemberCommand::ApplyMainActionResult(UStageAction* Action, EStageActionResult ActionResult)
{
	if (CommandState != EStageExecutionState::Running && CommandState != EStageExecutionState::Stopping) return;
	if (!IsValid(Action) || ActionResult == EStageActionResult::None) return;

	SaveAccumulatedActiveTime();
	if (ActionResult == EStageActionResult::Succeeded) bHasSuccessfulMainAction = true;
	OnMainActionResult(Action, ActionResult);
}

void UStageMemberCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
}

bool UStageMemberCommand::ShouldUseShortVersion(float ShortDuration) const
{
	if (CatchUpPolicy != EStageCatchUpPolicy::UseShortIfNeed || bUseShortVersion ||
		CommandState != EStageExecutionState::Running || CurrentStartTime >= 0.0f ||
		!FMath::IsFinite(ShortDuration) || ShortDuration < 0.0f) return false;

	const AStageManager* Manager = Context.Manager.Get();
	if (!Manager || Manager->GetState() != EStageState::Running) return false;

	const FStageRootExecution* Root = Manager->FindRoot(this);
	if (!Root || Root->CommandType != EStageCommandType::Scenario) return false;

	const float RemainingTime = GetRemainingTime();
	return FMath::IsFinite(RemainingTime) && ShortDuration < RemainingTime &&
		Manager->GetCommandLateness(this) > 0.0f;
}

void UStageMemberCommand::UseShortVersion()
{
	if (bUseShortVersion || CurrentStartTime >= 0.0f || CommandState != EStageExecutionState::Running) return;
	bUseShortVersion = true;
	AccumulatedActiveTime = 0.0f;
}

FStageEvent UStageMemberCommand::BuildSuccessEvent_Implementation() const
{
	FStageEvent Event;
	Event.EventTag = OnSuccessEventTag;
	if (const UStageMemberComponent* Member = Context.MemberComponent.Get()) Event.SubjectActor = Member->GetOwner();
	return Event;
}

void UStageMemberCommand::ResetRuntime(
	const FStageExecutionContext& InContext, 
	UStageMemberCommand* InRoot,
	float InPlannedStartTime, 
	float InPlannedEndTime)
{
	Context = InContext;
	RootCommand = InRoot ? InRoot : this;
	PlannedStartTime = InPlannedStartTime;
	PlannedEndTime = InPlannedEndTime;
	CommandState = EStageExecutionState::NotStarted;
	CommandResult = EStageCommandResult::None;
	AccumulatedActiveTime = 0.0f;
	CurrentStartTime = -1.0f;
	bUseShortVersion = false;
	bHasStartedMainAction = false;
	bHasSuccessfulMainAction = false;
	OnFinished.Clear();
}

void UStageMemberCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
}

void UStageMemberCommand::OnStart_Implementation()
{
	if (UStageMemberComponent* Member = Context.MemberComponent.Get()) Member->BeginExecution(this);
	else Finish(EStageCommandResult::Failed);
}

void UStageMemberCommand::OnStop_Implementation(EStageActionStopReason Reason)
{
	if (UStageMemberComponent* Member = Context.MemberComponent.Get()) Member->StopExecution(this, Reason);
	else Finish(EStageCommandResult::Cancelled);
}

void UStageMemberCommand::Finish(EStageCommandResult InResult)
{
	if (CommandState == EStageExecutionState::Finished) return;
	if (!ensureMsgf(InResult != EStageCommandResult::None, TEXT("A command cannot finish with result None."))) return;

	if (CommandState == EStageExecutionState::Stopping) InResult = EStageCommandResult::Cancelled;
	SaveAccumulatedActiveTime();
	CommandResult = InResult;
	CommandState = EStageExecutionState::Finished;
	
	const TWeakObjectPtr<UStageMemberComponent> Member = Context.MemberComponent;
	if (Member.IsValid())
	{
		Member->BeginDispatch();
		Member->HandleCommandFinished(this, CommandResult);
	}
	OnFinished.Broadcast(this, CommandResult);
	if (Member.IsValid()) Member->EndDispatch();
}

UWorld* UStageMemberCommand::GetWorld() const
{
	const UStageMemberComponent* Member = Context.MemberComponent.Get();
	return Member ? Member->GetWorld() : Super::GetWorld();
}
