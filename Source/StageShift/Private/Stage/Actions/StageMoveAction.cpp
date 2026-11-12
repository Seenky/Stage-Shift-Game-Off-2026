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
	CurrentRequestID = LastRequestID;
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
	
	const FPathFollowingRequestResult Result = AIController->MoveTo(CurrentRequest);

	AIController->ReceiveMoveCompleted.AddDynamic(this, &ThisClass::HandleReceiveMoveCompleted);
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
	
	CurrentRequestID = Result.MoveId;
}

void UStageMoveAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!AIController.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (Mode == EStageActionStopMode::StopAction) AIController->StopMovement();
}

void UStageMoveAction::Cleanup_Implementation()
{
	if (AIController.IsValid())
	{
		AIController->ReceiveMoveCompleted.RemoveDynamic(this, &ThisClass::HandleReceiveMoveCompleted);
		
		if (AIController->GetCurrentMoveRequestID() == CurrentRequestID)
		{
			AIController->StopMovement();
		}
	}
	MoveToLoc = FVector::ZeroVector;
	AcceptanceRadius = 0.0f;
	AIController = nullptr;
}

void UStageMoveAction::HandleReceiveMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
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
