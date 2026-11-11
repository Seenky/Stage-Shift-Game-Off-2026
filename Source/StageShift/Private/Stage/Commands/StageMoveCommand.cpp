// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/StageMoveCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/StageMoveAction.h"


UStageAction* UStageMoveCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() || 
		!MoveToActor) return nullptr;
	
	UStageMoveAction* MoveAction = NewObject<UStageMoveAction>(InContext.MemberComponent.Get());
	if (!MoveAction) return nullptr;
	
	MoveAction->MoveToRequest(MoveToActor->GetActorLocation(), AcceptableRadius, AIRequestID);
}

void UStageMoveCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
	if (const auto MoveAction = Cast<UStageMoveAction>(Action))
	{
		AIRequestID = MoveAction->GetCurrentRequestID();
	}
}

bool UStageMoveCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() || 
		!MoveToActor) return false;
		
	
	
	const FVector StartLocation = InContext.MemberComponent.Get()->GetOwner()->GetActorLocation();
	const FVector EndLocation = MoveToActor->GetActorLocation();
	
	return ;
}
