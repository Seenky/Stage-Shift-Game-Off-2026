// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Actions/StageMoveAction.h"
#include "AIController.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Stage/Core/StageManager.h"

void UStageMoveAction::MoveToRequest(const FAIMoveRequest& InRequest)
{
	// A previous attempt's request is never owned by this working Action.
	CurrentRequestID = FAIRequestID::InvalidRequest;
	MoveRequest = InRequest;
}

void UStageMoveAction::LogMove(const FString& Details) const
{
	const UStageMemberComponent* Member = Context.MemberComponent.Get();
	const AActor* Owner = Member ? Member->GetOwner() : nullptr;
	FString Message = FString::Printf(TEXT("StageMoveAction / Owner[%s] / Label[%s] / Request[%s] / %s"),
	    *GetNameSafe(Owner), Owner ? *Owner->GetActorNameOrLabel() : TEXT("None"), *CurrentRequestID.ToString(), *Details);
	if (MoveRequest.IsMoveToActorRequest()) Message += TEXT(" / Target[") + GetNameSafe(MoveRequest.GetGoalActor()) + TEXT("]");
	if (IsValid(Owner) && MoveRequest.IsValid())
	{
		const FVector Goal = MoveRequest.GetDestination();
		Message += FString::Printf(TEXT(" / From=%s, Goal=%s, Distance=%.2f cm, AcceptanceRadius=%.2f cm, IncludeAgentRadius=%d, State=%s"),
		    *Owner->GetActorLocation().ToString(), *Goal.ToString(), FVector::Distance(Owner->GetActorLocation(), Goal),
		    MoveRequest.GetAcceptanceRadius(), MoveRequest.IsReachTestIncludingAgentRadius(), *UEnum::GetValueAsString(GetState()));
	}
	if (Context.Manager.IsValid()) Context.Manager->AddDiagnostic(Message);
	else UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
}

void UStageMoveAction::OnStart_Implementation()
{
	const auto PawnOwner = Cast<APawn>(Context.MemberComponent.Get()->GetOwner());
	if (!PawnOwner)
	{
		LogMove(TEXT("Start failed: owner is not a Pawn"));
		Finish(EStageActionResult::Failed);
		return;
	}

	AIController = PawnOwner->GetController<AAIController>();
	if (!AIController.IsValid())
	{
		LogMove(TEXT("Start failed: no valid AIController"));
		Finish(EStageActionResult::Failed);
		return;
	}

	if (!MoveRequest.IsValid())
	{
		LogMove(TEXT("Start failed: invalid move request or missing target"));
		Finish(EStageActionResult::Failed);
		return;
	}

	// MoveTo may finish synchronously and clear the stored request in Cleanup.
	const FAIMoveRequest CurrentRequest = MoveRequest;

	LogMove(TEXT("Move requested"));
	AIController->ReceiveMoveCompleted.AddUniqueDynamic(this, &ThisClass::HandleReceiveMoveCompleted);
	bStartingRequest = true;
	const FPathFollowingRequestResult Result = AIController->MoveTo(CurrentRequest);
	bStartingRequest = false;
	CurrentRequestID = Result.MoveId;
	const UPathFollowingComponent* PathFollowing = AIController.IsValid() ? AIController->GetPathFollowingComponent() : nullptr;
	LogMove(FString::Printf(TEXT("MoveTo returned: Result=%s, PartialPath=%d"),
	    *UEnum::GetValueAsString(Result.Code.GetValue()), PathFollowing && PathFollowing->HasPartialPath()));
	if (GetState() == EStageExecutionState::Finished)
	{
		if (AIController.IsValid() && CurrentRequestID.IsValid() && AIController->GetCurrentMoveRequestID() == CurrentRequestID)
			AIController->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::ForcedScript, CurrentRequestID);
		return;
	}
	switch (Result.Code)
	{
	case EPathFollowingRequestResult::Failed:
		Finish(EStageActionResult::Failed);
		return;
	case EPathFollowingRequestResult::AlreadyAtGoal:
		Finish(EStageActionResult::Succeeded);
		return;
	case EPathFollowingRequestResult::RequestSuccessful:
	default:
		break;
	}

	const auto Completions = MoveTemp(StartingCompletions);
	for (const auto& Completion : Completions)
		HandleReceiveMoveCompleted(Completion.Key, Completion.Value);
}

void UStageMoveAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	LogMove(FString::Printf(TEXT("Stop requested: Mode=%s"), *UEnum::GetValueAsString(Mode)));
	if (!AIController.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (Mode == EStageActionStopMode::StopAction)
	{
		if (CurrentRequestID.IsValid() && AIController->GetCurrentMoveRequestID() == CurrentRequestID)
			AIController->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::ForcedScript, CurrentRequestID);
		if (GetState() != EStageExecutionState::Finished) Finish(EStageActionResult::Interrupted);
	}
}

void UStageMoveAction::Cleanup_Implementation()
{
	LogMove(FString::Printf(TEXT("Action finished: Result=%s"), *UEnum::GetValueAsString(GetResult())));
	if (AIController.IsValid())
	{
		AIController->ReceiveMoveCompleted.RemoveDynamic(this, &ThisClass::HandleReceiveMoveCompleted);

		if (CurrentRequestID.IsValid() && AIController->GetCurrentMoveRequestID() == CurrentRequestID)
		{
			AIController->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::ForcedScript, CurrentRequestID);
		}
	}
	MoveRequest = FAIMoveRequest();
	AIController = nullptr;
}

void UStageMoveAction::HandleReceiveMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (bStartingRequest)
	{
		StartingCompletions.Emplace(RequestID, Result);
		return;
	}
	if (!CurrentRequestID.IsValid() || RequestID != CurrentRequestID || GetState() == EStageExecutionState::Finished) return;
	LogMove(FString::Printf(TEXT("Navigation finished: Result=%s"), *UEnum::GetValueAsString(Result)));
	if (Result == EPathFollowingResult::Type::Success)
	{
		Finish(MoveRequest.IsValid() ? EStageActionResult::Succeeded : EStageActionResult::Failed);
	}
	else if (GetState() == EStageExecutionState::Stopping)
	{
		Finish(EStageActionResult::Interrupted);
	}
	else
	{
		Finish(EStageActionResult::Failed);
	}
}
