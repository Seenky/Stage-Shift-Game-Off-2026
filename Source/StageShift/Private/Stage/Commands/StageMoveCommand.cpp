// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Commands/StageMoveCommand.h"
#include "AIController.h"
#include "Components/Stage/StageMemberComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Stage/Actions/StageMoveAction.h"

UStageAction* UStageMoveCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner() || !ResolveTarget(InContext)) return nullptr;

	UStageMoveAction* MoveAction = NewObject<UStageMoveAction>(InContext.MemberComponent.Get());
	if (!MoveAction) return nullptr;

	MoveAction->MoveToRequest(ResolveTarget(InContext)->GetActorLocation(), AcceptanceRadius, AIRequestID);

	return MoveAction;
}

void UStageMoveCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
	if (const auto MoveAction = Cast<UStageMoveAction>(Action))
	{
		AIRequestID = MoveAction->GetCurrentRequestID();
	}
}

float UStageMoveCommand::GetNeededTime_Implementation() const
{
	return NeedMoveTime;
}

bool UStageMoveCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner() || !ResolveTarget(InContext) || !AIController.IsValid()
	    || !AIController->GetPathFollowingComponent())
		return false;

	return AIController->GetPathFollowingComponent()->HasReached(ResolveTarget(InContext)->GetActorLocation(), EPathFollowingReachMode::OverlapAgent, AcceptanceRadius);
}

void UStageMoveCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner() || !ResolveTarget(InContext)) return;

	const auto Owner = Context.MemberComponent.Get()->GetOwner();
	float Speed = 150.f;
	if (const auto MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>())
	{
		Speed = MovementComp->GetMaxSpeedForNavMovement();
	}

	Speed = FMath::Max(Speed, 1.f);

	const float AllDistance = FVector::Distance(InContext.MemberComponent.Get()->GetOwner()->GetActorLocation(), ResolveTarget(InContext)->GetActorLocation());
	const float StopDistance = AcceptanceRadius + Owner->GetSimpleCollisionRadius();
	const float RemainingDistance = FMath::Max(0.f, AllDistance - StopDistance);
	NeedMoveTime = RemainingDistance / Speed;

	const auto Pawn = Cast<APawn>(InContext.MemberComponent.Get()->GetOwner());
	if (!Pawn) return;
	AIController = Pawn->GetController<AAIController>();
}

void UStageMoveCommand::OnStart_Implementation()
{
	if (!Context.MemberComponent.Get() || !Context.MemberComponent.Get()->GetOwner() || !ResolveTarget(Context) || !AIController.IsValid())
	{
		Finish(EStageCommandResult::Failed);
		return;
	}

	Super::OnStart_Implementation();
}

void UStageMoveCommand::ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime)
{
	Super::ResetRuntime(InContext, InRoot, InPlannedStartTime, InPlannedEndTime);
	OnFinished.AddUniqueDynamic(this, &ThisClass::HandleCommandFinished);
}

void UStageMoveCommand::HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	if (Result == EStageCommandResult::Succeeded)
		if (UStageMemberComponent* Member = Context.MemberComponent.Get()) Member->RememberStagePose(Command);
}

#include "Stage/Core/StageManager.h"
#include "Stage/Core/Data/StageDataAsset.h"

AActor* UStageMoveCommand::ResolveTarget(const FStageExecutionContext& InContext) const
{
	return !TargetId.IsNone() && InContext.Manager.IsValid() ? InContext.Manager->GetTarget(TargetId) : nullptr;
}

void UStageMoveCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (!TargetId.IsNone() && !Data->TargetIds.Contains(TargetId)) Errors.Add(Path + TEXT(" / unknown TargetId: ") + TargetId.ToString());
	if (TargetId.IsNone()) Errors.Add(Path + TEXT(" / TargetId is required"));
	if (!FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0) Errors.Add(Path + TEXT(" / AcceptanceRadius must be finite and nonnegative"));
}
