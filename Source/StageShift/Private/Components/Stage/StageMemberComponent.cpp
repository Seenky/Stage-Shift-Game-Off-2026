#include "Components/Stage/StageMemberComponent.h"

#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Stage/Core/Data/StageRecoveryRulesDataAsset.h"
#include "Stage/Core/StageAction.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StatusesComponent.h"

UStageMemberComponent::UStageMemberComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UStageMemberComponent::BindToStage(AStageManager* InManager)
{
	if (!bMemberFinished || !IsValid(InManager) || !GetOwner()) return false;
	UStatusesComponent* Statuses = GetOwner()->FindComponentByClass<UStatusesComponent>();
	if (!Statuses || (RecoveryRulesAsset && (!FMath::IsFinite(RecoveryRulesAsset->RecoveryWaitTimeout) ||
		RecoveryRulesAsset->RecoveryWaitTimeout < 0.0f))) return false;

	Manager = InManager;
	StatusComponent = Statuses;
	CachedStatuses = Statuses->GetStatuses();
	Statuses->OnAddStatuses.AddUniqueDynamic(this, &UStageMemberComponent::HandleStatusesChanged);
	Statuses->OnRemoveStatuses.AddUniqueDynamic(this, &UStageMemberComponent::HandleStatusesChanged);
	bClosingMember = false;
	bMemberFinished = false;
	bUpdateRequested = false;
	SetComponentTickEnabled(true);
	return true;
}

bool UStageMemberComponent::HasReaction() const
{
	if (PendingReaction) return true;
	if (!Manager.IsValid()) return false;
	for (const UStageMemberCommand* Root : RootCommands)
	{
		const auto* Entry = Manager->FindRoot(Root);
		if (Entry && Entry->CommandType == EStageCommandType::Reaction && Root->GetState() != EStageExecutionState::Finished) return true;
	}
	return false;
}

bool UStageMemberComponent::RunCommand(UStageMemberCommand* Command, EStageCommandType Type)
{
	if (!IsValid(Command) || bClosingMember || bMemberFinished || !Manager.IsValid() ||
		Command->GetState() != EStageExecutionState::NotStarted || Command->GetOuter() != this ||
		Command->Context.MemberComponent.Get() != this || Command->Context.Manager != Manager ||
		RootCommands.Contains(Command) || (Type == EStageCommandType::Reaction && HasReaction())) return false;
	BeginDispatch();
	RootCommands.Add(Command);
	if (Type == EStageCommandType::Reaction) PendingReaction = Command;
	else PendingCommands.Add(Command);
	EndDispatch();
	return true;
}

void UStageMemberComponent::PumpCommands()
{
	if (bPumpingCommands || bClosingMember || bMemberFinished || DispatchDepth != 0 || bUpdatingExecutions ||
		!Manager.IsValid() || Manager->EventDispatchDepth > 1 || ActiveRecoveryAction) return;
	if (RootCommands.ContainsByPredicate([](const UStageMemberCommand* Root)
		{ return Root->GetState() == EStageExecutionState::Running || Root->GetState() == EStageExecutionState::Stopping; })) return;
	UStageMemberCommand* Next = PendingReaction;
	if (Next) PendingReaction = nullptr;
	else if (!PendingCommands.IsEmpty()) { Next = PendingCommands[0]; PendingCommands.RemoveAt(0); }
	if (!Next) return;
	bPumpingCommands = true;
	StartPreparedCommand(Next);
	bPumpingCommands = false;
}

void UStageMemberComponent::RequestStopCommands()
{
	BeginDispatch();
	const auto Snapshot = RootCommands;
	for (UStageMemberCommand* Root : Snapshot)
	{
		const auto* Entry = Manager.IsValid() ? Manager->FindRoot(Root) : nullptr;
		if (Entry && Entry->CommandType == EStageCommandType::Finale) continue;
		PendingCommands.Remove(Root);
		if (PendingReaction == Root) PendingReaction = nullptr;
		if (Root->GetState() == EStageExecutionState::NotStarted) CompleteBeforeStart(Root, EStageCommandResult::Cancelled);
		else Root->RequestStop(EStageActionStopReason::Cancel);
	}
	EndDispatch();
}

void UStageMemberComponent::StartPreparedCommand(UStageMemberCommand* Command)
{
	if (!IsValid(Command) || Command->GetState() != EStageExecutionState::NotStarted) return;
	BeginDispatch();
	ON_SCOPE_EXIT { EndDispatch(); };
	UStageMemberCommand* Root = Command->GetRootCommand();
	if (bMemberFinished || !Manager.IsValid() || !StatusComponent.IsValid() || !Root || Root->GetOuter() != this ||
		Command->Context.MemberComponent.Get() != this || Command->Context.Manager != Manager)
	{
		Command->Finish(EStageCommandResult::Failed);
		return;
	}
	RootCommands.AddUnique(Root);
	if (bClosingMember || Root->GetState() == EStageExecutionState::Stopping || Root->GetState() == EStageExecutionState::Finished)
	{
		CompleteBeforeStart(Command, EStageCommandResult::Cancelled);
		return;
	}
	const EStageCommandResult Admission = Manager->CheckCommandStart(Command);
	if (Command->GetState() != EStageExecutionState::NotStarted) return;
	if (Admission != EStageCommandResult::None)
	{
		CompleteBeforeStart(Command, Admission);
		return;
	}
	if (!Command->IsComposite())
	{
		const bool bResultAchieved = Command->IsResultAchieved(Command->Context);
		if (Command->GetState() != EStageExecutionState::NotStarted) return;
		if (bClosingMember || Root->GetState() == EStageExecutionState::Stopping || Root->GetState() == EStageExecutionState::Finished)
		{
			CompleteBeforeStart(Command, EStageCommandResult::Cancelled);
			return;
		}
		if (bResultAchieved)
		{
			CompleteBeforeStart(Command, EStageCommandResult::Succeeded);
			return;
		}
	}
	const EStageCommandResult FinalAdmission = Manager->CheckCommandStart(Command);
	if (Command->GetState() != EStageExecutionState::NotStarted) return;
	if (FinalAdmission != EStageCommandResult::None) { CompleteBeforeStart(Command, FinalAdmission); return; }
	if (Manager->ShouldSkipCommand(Command)) { CompleteBeforeStart(Command, EStageCommandResult::Skipped); return; }
	Manager->HandleCommandStarted(Command);
	Command->Start(Command->Context);
}

void UStageMemberComponent::CompleteBeforeStart(UStageMemberCommand* Command, EStageCommandResult Result)
{
	if (!IsValid(Command) || Command->GetState() != EStageExecutionState::NotStarted || Result == EStageCommandResult::None) return;
	BeginDispatch();
	ON_SCOPE_EXIT { EndDispatch(); };
	if (!bMemberFinished && Manager.IsValid() && Command->Context.MemberComponent.Get() == this && Command->Context.Manager == Manager)
		RootCommands.AddUnique(Command->GetRootCommand() ? Command->GetRootCommand() : Command);

	TArray<UStageMemberCommand*> Pending;
	TArray<UStageMemberCommand*> Changed;
	TSet<UStageMemberCommand*> Visited;
	Pending.Add(Command);
	while (!Pending.IsEmpty())
	{
		UStageMemberCommand* Node = Pending.Pop();
		if (!IsValid(Node) || Visited.Contains(Node)) continue;
		Visited.Add(Node);
		for (UStageMemberCommand* Child : Node->GetChildCommands())
			if (IsValid(Child) && Child->GetOuter() == Node) Pending.Add(Child);
		if (Node->GetState() != EStageExecutionState::NotStarted) continue;
		Node->CommandResult = Result;
		Node->CommandState = EStageExecutionState::Finished;
		Changed.Add(Node);
	}
	// Close the entire subtree before any callback can try to start another child.
	for (int32 Index = Changed.Num() - 1; Index >= 0; --Index)
	{
		UStageMemberCommand* Node = Changed[Index];
		HandleCommandFinished(Node, Result);
		Node->OnFinished.Broadcast(Node, Result);
	}
}

FStageCommandExecution* UStageMemberComponent::FindExecution(UStageMemberCommand* Command)
{
	return Executions.FindByPredicate([Command](const FStageCommandExecution& Entry) { return Entry.Command == Command; });
}

void UStageMemberComponent::BeginExecution(UStageMemberCommand* Command)
{
	if (!IsValid(Command) || FindExecution(Command)) return;
	if (bClosingMember || bMemberFinished || !Manager.IsValid() || !StatusComponent.IsValid() ||
		Command->Context.MemberComponent.Get() != this || Command->Context.Manager != Manager)
	{
		Command->Finish(EStageCommandResult::Failed);
		return;
	}
	if (Command->GetState() != EStageExecutionState::Running) return;
	UStageMemberCommand* Root = Command->GetRootCommand();
	if (!Root || Root->GetState() == EStageExecutionState::Finished || Root->GetState() == EStageExecutionState::Stopping)
	{
		Command->Finish(EStageCommandResult::Cancelled);
		return;
	}

	RootCommands.AddUnique(Root);
	FStageCommandExecution& Entry = Executions.AddDefaulted_GetRef();
	Entry.Command = Command;
	bUpdateRequested = true;
	if (DispatchDepth == 0) UpdateExecutions();
}

UStageAction* UStageMemberComponent::GetMainAction(UStageMemberCommand* Command) const
{
	const FStageCommandExecution* Entry = Executions.FindByPredicate(
		[Command](const FStageCommandExecution& Item) { return Item.Command == Command; });
	return Entry ? Entry->CurrentMainAction.Get() : nullptr;
}

void UStageMemberComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bMemberFinished) return;
	if (!Manager.IsValid() || !StatusComponent.IsValid())
	{
		EndStageMember();
		return;
	}

	// Also detect direct changes to the status set.
	if (CachedStatuses != StatusComponent->GetStatuses())
	{
		CachedStatuses = StatusComponent->GetStatuses();
		bUpdateRequested = true;
	}
	if (Executions.ContainsByPredicate([](const FStageCommandExecution& Entry) { return Entry.RecoveryWaitDeadline >= 0.0f; }))
		bUpdateRequested = true;
	if (bUpdateRequested) UpdateExecutions();
	PumpCommands();
	TryFinishMember();
}

void UStageMemberComponent::HandleStatusesChanged(FGameplayTagContainer ChangedStatuses)
{
	if (!StatusComponent.IsValid() || bMemberFinished) return;
	CachedStatuses = StatusComponent->GetStatuses();
	bUpdateRequested = true;
	if (DispatchDepth == 0) UpdateExecutions();
}

void UStageMemberComponent::UpdateExecutions()
{
	if (bUpdatingExecutions || DispatchDepth != 0 || bMemberFinished) return;
	bUpdatingExecutions = true;
	bUpdateRequested = false;
	TArray<UStageMemberCommand*> Snapshot;
	for (const FStageCommandExecution& Entry : Executions) Snapshot.Add(Entry.Command);
	for (UStageMemberCommand* Command : Snapshot) AdvanceExecution(Command);
	// A synchronous callback may request another pass; it runs on the next tick.
	bUpdatingExecutions = false;
	FinishingActions.Reset();
	PumpCommands();
	TryFinishMember();
}

UStageAction* UStageMemberComponent::SelectRecoveryTemplate() const
{
	if (!RecoveryRulesAsset || !StatusComponent.IsValid()) return nullptr;
	for (const FStageActionRule& Rule : RecoveryRulesAsset->Rules)
	{
		if (IsValid(Rule.Action) && !Rule.Action->GetClass()->HasAnyClassFlags(CLASS_Abstract) &&
			Rule.ExecRequirements.GetIsAccess(StatusComponent->GetStatuses())) return Rule.Action;
	}
	return nullptr;
}

void UStageMemberComponent::AdvanceExecution(UStageMemberCommand* Command)
{
	FStageCommandExecution* Entry = FindExecution(Command);
	if (!Entry || !IsValid(Command)) return;
	if (UStageAction* Main = Entry->CurrentMainAction)
	{
		if (Main->GetState() != EStageExecutionState::Running) return;
		const bool bRequirementsMet = StatusComponent.IsValid() &&
			Command->ExecutionRequirements.GetIsAccess(StatusComponent->GetStatuses());
		if (Command->GetState() == EStageExecutionState::Running &&
			(!bRequirementsMet || !Main->CanContinue())) Main->RequestStop(EStageActionStopReason::Interrupt);
		return;
	}
	if (Command->GetState() == EStageExecutionState::Finished)
	{
		FinishExecution(Command, Command->GetResult());
		return;
	}
	if (Command->GetState() == EStageExecutionState::Stopping || bClosingMember ||
		!Manager.IsValid() || !StatusComponent.IsValid())
	{
		FinishExecution(Command, EStageCommandResult::Cancelled);
		return;
	}

	const EStageCommandResult Admission = Manager->CheckCommandStart(Command);
	if (!FindExecution(Command) || Command->GetState() != EStageExecutionState::Running) return;
	if (Admission != EStageCommandResult::None) { FinishExecution(Command, Admission); return; }
	const bool bResultAchieved = Command->IsResultAchieved(Command->Context);
	if (!FindExecution(Command) || Command->GetState() != EStageExecutionState::Running) return;
	if (bResultAchieved)
	{
		FinishExecution(Command, EStageCommandResult::Succeeded);
		return;
	}
	if (ActiveRecoveryAction)
	{
		if (ActiveRecoveryAction->GetState() == EStageExecutionState::Running)
		{
			RecoveryWaiters.AddUnique(Command);
			FindExecution(Command)->RecoveryWaitDeadline = -1.0f;
		}
		return;
	}

	if (Command->ExecutionRequirements.GetIsAccess(StatusComponent->GetStatuses()))
	{
		UStageAction* Action = Command->CreateMainAction(Command->Context);
		const EStageCommandResult MainAdmission = Manager.IsValid() ? Manager->CheckCommandStart(Command) : EStageCommandResult::Cancelled;
		Entry = FindExecution(Command);
		if (!Entry || Command->GetState() != EStageExecutionState::Running) return;
		if (MainAdmission != EStageCommandResult::None) { FinishExecution(Command, MainAdmission); return; }
		if (bClosingMember || !StatusComponent.IsValid())
		{
			FinishExecution(Command, EStageCommandResult::Cancelled);
			return;
		}
		if (!IsValid(Action) || Action->GetOuter() != this || Action->GetState() != EStageExecutionState::NotStarted)
		{
			FinishExecution(Command, EStageCommandResult::Failed);
			return;
		}
		if (!Command->ExecutionRequirements.GetIsAccess(StatusComponent->GetStatuses()))
		{
			bUpdateRequested = true;
			return;
		}
		Entry->RecoveryWaitDeadline = -1.0f;
		Entry->CurrentMainAction = Action; // UPROPERTY holds the working Action before Start.
		Action->OnFinished.AddUniqueDynamic(this, &UStageMemberComponent::HandleActionFinished);
		Command->NotifyMainActionStarted();
		Action->Start(Command->Context);
		return;
	}
	// Without a recovery profile, wait for external status changes or command cancellation.
	if (!RecoveryRulesAsset) return;
	if (UStageAction* Template = SelectRecoveryTemplate())
	{
		ActiveRecoveryAction = DuplicateObject<UStageAction>(Template, this);
		ActiveRecoveryAction->OnFinished.Clear();
		RecoveryWaiters.AddUnique(Command);
		FindExecution(Command)->RecoveryWaitDeadline = -1.0f;
		ActiveRecoveryAction->OnFinished.AddDynamic(this, &UStageMemberComponent::HandleActionFinished);
		ActiveRecoveryAction->Start(Command->Context);
		return;
	}

	Entry = FindExecution(Command);
	if (!Entry || !GetWorld()) return;
	if (RecoveryRulesAsset->RecoveryWaitTimeout == 0.0f)
	{
		Entry->RecoveryWaitDeadline = -1.0f;
		return;
	}
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (Entry->RecoveryWaitDeadline < 0.0f) Entry->RecoveryWaitDeadline = Now + RecoveryRulesAsset->RecoveryWaitTimeout;
	if (Now >= Entry->RecoveryWaitDeadline) FinishExecution(Command, EStageCommandResult::Failed);
}

void UStageMemberComponent::HandleActionFinished(UStageAction* Action, EStageActionResult Result)
{
	if (ActiveRecoveryAction == Action)
	{
		FinishingActions.AddUnique(Action);
		Action->OnFinished.RemoveDynamic(this, &UStageMemberComponent::HandleActionFinished);
		ActiveRecoveryAction = nullptr;
		const TArray<TObjectPtr<UStageMemberCommand>> Waiters = RecoveryWaiters;
		RecoveryWaiters.Reset();
		for (UStageMemberCommand* Command : Waiters)
		{
			if (!FindExecution(Command)) continue;
			if (Command->GetState() == EStageExecutionState::Stopping)
				FinishExecution(Command, EStageCommandResult::Cancelled);
			else if (Result == EStageActionResult::Failed || Result == EStageActionResult::Cancelled)
				FinishExecution(Command, EStageCommandResult::Failed);
		}
		bUpdateRequested = true;
		return;
	}

	FStageCommandExecution* Entry = Executions.FindByPredicate(
		[Action](const FStageCommandExecution& Item) { return Item.CurrentMainAction == Action; });
	if (!Entry) return; // A late response from a different working Action.
	UStageMemberCommand* Command = Entry->Command;
	FinishingActions.AddUnique(Action);
	Entry->CurrentMainAction = nullptr;
	Action->OnFinished.RemoveDynamic(this, &UStageMemberComponent::HandleActionFinished);
	Command->ApplyMainActionResult(Action, Result);
	if (!FindExecution(Command)) return;
	if (Command->GetState() == EStageExecutionState::Stopping)
		FinishExecution(Command, EStageCommandResult::Cancelled);
	else if (Command->GetState() == EStageExecutionState::Finished)
		FinishExecution(Command, Command->GetResult());
	else if (Result == EStageActionResult::Succeeded)
	{
		const bool bResultAchieved = Command->IsResultAchieved(Command->Context);
		if (FindExecution(Command)) FinishExecution(Command, bResultAchieved ? EStageCommandResult::Succeeded : EStageCommandResult::Failed);
	}
	else if (Result == EStageActionResult::Interrupted) bUpdateRequested = true;
	else FinishExecution(Command, EStageCommandResult::Failed);
}

void UStageMemberComponent::RemoveRecoveryWaiter(UStageMemberCommand* Command)
{
	RecoveryWaiters.Remove(Command);
	if (RecoveryWaiters.IsEmpty() && ActiveRecoveryAction)
		ActiveRecoveryAction->RequestStop(EStageActionStopReason::Cancel);
}

void UStageMemberComponent::StopExecution(UStageMemberCommand* Command, EStageActionStopReason Reason)
{
	if (!IsValid(Command)) return;
	BeginDispatch();
	RemoveRecoveryWaiter(Command);
	if (UStageAction* Main = GetMainAction(Command)) Main->RequestStop(Reason);
	else FinishExecution(Command, EStageCommandResult::Cancelled);
	EndDispatch();
}

void UStageMemberComponent::FinishExecution(UStageMemberCommand* Command, EStageCommandResult Result)
{
	Executions.RemoveAll([Command](const FStageCommandExecution& Item) { return Item.Command == Command; });
	RemoveRecoveryWaiter(Command);
	if (IsValid(Command)) Command->Finish(Result);
}

void UStageMemberComponent::BeginDispatch()
{
	++DispatchDepth;
	if (Manager.IsValid()) Manager->BeginNotification();
}

void UStageMemberComponent::EndDispatch()
{
	check(DispatchDepth > 0);
	const auto BoundManager = Manager;
	--DispatchDepth;
	if (DispatchDepth == 0 && !bUpdatingExecutions)
	{
		if (bUpdateRequested) UpdateExecutions();
		FinishingActions.Reset();
		PumpCommands();
		TryFinishMember();
	}
	if (BoundManager.IsValid()) BoundManager->EndNotification();
}

void UStageMemberComponent::HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	if (!Manager.IsValid() || !RootCommands.Contains(Command->GetRootCommand())) return;
	Manager->AccountCommandResult(Command, Result);
	OnCommandFinished.Broadcast(Command, Result);
	if (Result == EStageCommandResult::Succeeded && Command->OnSuccessEventTag.IsValid() &&
		(Command->IsComposite() || Command->HasSuccessfulMainAction())) ReportStageEvent(Command->BuildSuccessEvent());
}

void UStageMemberComponent::ReportStageEvent(const FStageEvent& Event)
{
	if (!Manager.IsValid() || !Event.EventTag.IsValid()) return;
	BeginDispatch();
	OnStageEvent.Broadcast(Event);
	if (Manager.IsValid()) Manager->ReceiveStageEvent(Event);
	EndDispatch();
}

void UStageMemberComponent::EndStageMember()
{
	if (bMemberFinished) return;
	BeginDispatch();
	bClosingMember = true;
	PendingCommands.Reset();
	PendingReaction = nullptr;
	const TArray<TObjectPtr<UStageMemberCommand>> Roots = RootCommands;
	for (UStageMemberCommand* Command : Roots)
		if (IsValid(Command))
		{
			if (Command->GetState() == EStageExecutionState::NotStarted) CompleteBeforeStart(Command, EStageCommandResult::Cancelled);
			else Command->RequestStop(EStageActionStopReason::Cancel);
		}
	if (RecoveryWaiters.IsEmpty() && ActiveRecoveryAction)
		ActiveRecoveryAction->RequestStop(EStageActionStopReason::Cancel);
	EndDispatch();
}

void UStageMemberComponent::TryFinishMember()
{
	if (!bClosingMember || bMemberFinished || DispatchDepth != 0 || bUpdatingExecutions ||
		!Executions.IsEmpty() || ActiveRecoveryAction) return;
	if (RootCommands.ContainsByPredicate([](const UStageMemberCommand* Command)
		{ return IsValid(Command) && Command->GetState() != EStageExecutionState::Finished; })) return;

	if (UStatusesComponent* Statuses = StatusComponent.Get())
	{
		Statuses->OnAddStatuses.RemoveDynamic(this, &UStageMemberComponent::HandleStatusesChanged);
		Statuses->OnRemoveStatuses.RemoveDynamic(this, &UStageMemberComponent::HandleStatusesChanged);
	}
	Manager.Reset();
	StatusComponent.Reset();
	RootCommands.Reset();
	PendingCommands.Reset();
	PendingReaction = nullptr;
	RecoveryWaiters.Reset();
	FinishingActions.Reset();
	CachedStatuses.Reset();
	bUpdateRequested = false;
	bMemberFinished = true;
	SetComponentTickEnabled(false);
	OnMemberFinished.Broadcast(this);
}
