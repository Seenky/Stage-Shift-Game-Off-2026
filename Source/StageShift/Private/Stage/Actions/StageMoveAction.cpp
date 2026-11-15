// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/StageMoveAction.h"
#include "AIController.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Navigation/PathFollowingComponent.h"

void UStageMoveAction::MoveToRequest(
	const FVector& InMoveToLoc, 
	const float& InAcceptanceRadius, 
	const FAIRequestID& LastRequestID)
{
    // A previous attempt's request is never owned by this working Action.
	CurrentRequestID = FAIRequestID::InvalidRequest;
	MoveToLoc = InMoveToLoc;
	AcceptanceRadius = InAcceptanceRadius;
}

void UStageMoveAction::OnStart_Implementation()
{
	const auto PawnOwner = Cast<APawn>(Context.MemberComponent.Get()->GetOwner());
	if (!PawnOwner)
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	AIController = PawnOwner->GetController<AAIController>();
	if (!AIController.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	FAIMoveRequest CurrentRequest(MoveToLoc);
	CurrentRequest.SetAcceptanceRadius(AcceptanceRadius);
	
    AIController->ReceiveMoveCompleted.AddUniqueDynamic(this, &ThisClass::HandleReceiveMoveCompleted);
    bStartingRequest = true;
    const FPathFollowingRequestResult Result = AIController->MoveTo(CurrentRequest);
    bStartingRequest = false;
    CurrentRequestID = Result.MoveId;
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
    for (const auto& Completion : Completions) HandleReceiveMoveCompleted(Completion.Key, Completion.Value);
}

void UStageMoveAction::OnStop_Implementation(EStageActionStopMode Mode)
{
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
	if (AIController.IsValid())
	{
		AIController->ReceiveMoveCompleted.RemoveDynamic(this, &ThisClass::HandleReceiveMoveCompleted);
		
		if (CurrentRequestID.IsValid() && AIController->GetCurrentMoveRequestID() == CurrentRequestID)
		{
            AIController->GetPathFollowingComponent()->AbortMove(*this, FPathFollowingResultFlags::ForcedScript, CurrentRequestID);
		}
	}
	MoveToLoc = FVector::ZeroVector;
	AcceptanceRadius = 0.0f;
	AIController = nullptr;
}

void UStageMoveAction::HandleReceiveMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    if (bStartingRequest) { StartingCompletions.Emplace(RequestID, Result); return; }
    if (!CurrentRequestID.IsValid() || RequestID != CurrentRequestID || GetState() == EStageExecutionState::Finished) return;
	if (Result == EPathFollowingResult::Type::Success)
	{
		Finish(EStageActionResult::Succeeded);
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
