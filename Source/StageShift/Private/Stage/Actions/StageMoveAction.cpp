// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/StageMoveAction.h"
#include "AIController.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Navigation/PathFollowingComponent.h"

void UStageMoveAction::MoveToRequest(
	const FVector& InMoveToLoc, 
	const float& InAcceptableRadius, 
	const FAIRequestID& LastRequestID)
{
	CurrentRequestID = LastRequestID;
	MoveToLoc = InMoveToLoc;
	AcceptableRadius = InAcceptableRadius;
}

void UStageMoveAction::OnStart_Implementation()
{
	const auto PawnOwner = Cast<APawn>(Context.MemberComponent.Get()->GetOwner());
	if (!PawnOwner)
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	const auto AIController = PawnOwner->GetController<AAIController>();
	if (!AIController)
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	FAIMoveRequest CurrentRequest(MoveToLoc);
	CurrentRequest.SetAcceptanceRadius(AcceptableRadius);
	
	const FPathFollowingRequestResult Result = AIController->MoveTo(CurrentRequest);

	AIController->ReceiveMoveCompleted.AddDynamic(this, &ThisClass::)
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
	
}

void UStageMoveAction::Cleanup_Implementation()
{
	
}

void UStageMoveAction::HandleReceiveMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (Result == EPathFollowingResult::Type::Success)
	{
		Finish(EStageActionResult::Succeeded);
	}
	else
	{
		Finish(EStageActionResult::Failed);
	}
}
