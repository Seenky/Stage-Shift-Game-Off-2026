// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Commands/StageMoveCommand.h"
#include "AIController.h"
#include "Components/Stage/StageMemberComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Stage/Actions/StageMoveAction.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"
#include "NavigationSystem.h"

UStageAction* UStageMoveCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner() || !ResolveTarget(InContext, TargetId)) return nullptr;

	UStageMoveAction* MoveAction = NewObject<UStageMoveAction>(InContext.MemberComponent.Get());
	if (!MoveAction) return nullptr;

	MoveAction->MoveToRequest(ResolveTarget(InContext, TargetId)->GetActorLocation(), AcceptanceRadius, AIRequestID);

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
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner() || !ResolveTarget(InContext, TargetId) || !AIController.IsValid()
	    || !AIController->GetPathFollowingComponent())
		return false;

	return AIController->GetPathFollowingComponent()->HasReached(ResolveTarget(InContext, TargetId)->GetActorLocation(), EPathFollowingReachMode::OverlapAgent, AcceptanceRadius);
}

void UStageMoveCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() || !InContext.MemberComponent.Get()->GetOwner() || !ResolveTarget(InContext, TargetId)) return;

	const auto Owner = Context.MemberComponent.Get()->GetOwner();
	float Speed = 150.f;
	if (const auto MovementComp = Owner->FindComponentByClass<UCharacterMovementComponent>())
	{
		Speed = MovementComp->GetMaxSpeedForNavMovement();
	}

	Speed = FMath::Max(Speed, 1.f);

	const float AllDistance = FVector::Distance(InContext.MemberComponent.Get()->GetOwner()->GetActorLocation(), ResolveTarget(InContext, TargetId)->GetActorLocation());
	const float StopDistance = AcceptanceRadius + Owner->GetSimpleCollisionRadius();
	const float RemainingDistance = FMath::Max(0.f, AllDistance - StopDistance);
	NeedMoveTime = RemainingDistance / Speed;

	const auto Pawn = Cast<APawn>(InContext.MemberComponent.Get()->GetOwner());
	if (!Pawn) return;
	AIController = Pawn->GetController<AAIController>();
}

void UStageMoveCommand::OnStart_Implementation()
{
	if (!Context.MemberComponent.Get() || !Context.MemberComponent.Get()->GetOwner() || !ResolveTarget(Context, TargetId) || !AIController.IsValid())
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

#include "Stage/Core/Data/StageDataAsset.h"

void UStageMoveCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (!TargetId.IsNone() && !Data->TargetIds.Contains(TargetId)) Errors.Add(Path + TEXT(" / unknown TargetId: ") + TargetId.ToString());
	if (TargetId.IsNone()) Errors.Add(Path + TEXT(" / TargetId is required"));
	if (!FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0) Errors.Add(Path + TEXT(" / AcceptanceRadius must be finite and nonnegative"));
}

void UStageMoveCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	if (!ResolveTarget(InContext, TargetId)) Errors.Add(Path + TEXT(" / Move target is unloaded or belongs to another world"));
	const auto* Member = InContext.MemberComponent.Get();
	if (!Member) return;
	const auto* Pawn = Cast<APawn>(Member->GetOwner());
	UWorld* World = Member->GetWorld();
	if (!Pawn || !Pawn->FindComponentByClass<UPawnMovementComponent>())
		Errors.Add(Path + TEXT(" / Move requires a pawn with navigation movement"));
	else if (World && World->IsGameWorld() && !Pawn->GetController<AAIController>())
		Errors.Add(Path + TEXT(" / Move requires an AIController"));
	else if (World && !World->IsGameWorld() && Pawn->AutoPossessAI == EAutoPossessAI::Disabled && !Pawn->GetController<AAIController>())
		Errors.Add(Path + TEXT(" / Move requires AI possession"));
	const auto* Navigation = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Navigation || !Navigation->GetDefaultNavDataInstance()) Errors.Add(Path + TEXT(" / Move requires navigation data in the current world"));
}
