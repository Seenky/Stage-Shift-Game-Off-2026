// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/StageMoveCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Stage/Actions/StageMoveAction.h"


UStageAction* UStageMoveCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() || 
		!MoveToActor) return nullptr;
	
	UStageMoveAction* MoveAction = NewObject<UStageMoveAction>(InContext.MemberComponent.Get());
	if (!MoveAction) return nullptr;
	
	MoveAction->MoveToRequest(MoveToActor->GetActorLocation(), AcceptableRadius, AIRequestID);
	
	return MoveAction;
}

void UStageMoveCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
	if (const auto MoveAction = Cast<UStageMoveAction>(Action))
	{
		AIRequestID = MoveAction->GetCurrentRequestID();
	}
}

const float UStageMoveCommand::GetNeedMoveToTime() const
{
	if (!Context.MemberComponent.Get() ||
		!Context.MemberComponent.Get()->GetOwner() || 
		!MoveToActor) return 0.f;
	
	float Speed = 150.f;
	if (const auto MovementComp = 
		Context.MemberComponent.Get()->GetOwner()->FindComponentByClass<UCharacterMovementComponent>())
	{
		Speed = MovementComp->GetMaxSpeedForNavMovement();
	}
	
	const FVector StartLocation = Context.MemberComponent.Get()->GetOwner()->GetActorLocation();
	const FVector EndLocation = MoveToActor->GetActorLocation();
	const float Time = FVector::Distance(StartLocation, EndLocation) / Speed + 0.5f;
	return Time;
}

float UStageMoveCommand::GetNeededTime_Implementation() const
{
	return GetNeedMoveToTime();
}

bool UStageMoveCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() || 
		!MoveToActor) return false;
		
	const FVector StartLocation = Context.MemberComponent.Get()->GetOwner()->GetActorLocation();
	const FVector EndLocation = MoveToActor->GetActorLocation();
	
	return FVector::Distance(StartLocation, EndLocation) <= AcceptableRadius;
}
