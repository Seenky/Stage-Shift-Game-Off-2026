// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Actions/ReturnToStageMarkAction.h"
#include "Stage/Actions/StageMoveAction.h"
#include "Stage/Actions/StageLookToAction.h"
#include "Components/Stage/StageMemberComponent.h"
#include "GameFramework/PawnMovementComponent.h"

void UReturnToStageMarkAction::ValidateRecoveryTemplate(const FString& Path, TArray<FString>& Errors) const
{
	if (!FMath::IsFinite(TurnSpeed) || TurnSpeed <= 0) Errors.Add(Path + TEXT(" / TurnSpeed must be positive and finite"));
	if (!FMath::IsFinite(WatchdogSeconds) || WatchdogSeconds <= 0) Errors.Add(Path + TEXT(" / recovery watchdog must be positive and finite"));
}

void UReturnToStageMarkAction::OnStart_Implementation()
{
	UStageMemberComponent* Member = Context.MemberComponent.Get();
	AActor* Owner = Member ? Member->GetOwner() : nullptr;
	if (!IsValid(Owner) || Member->IsPhysicallyBlocked())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (Member->IsAtStageMark())
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}
	if (Member->IsAtStageMark(true, false))
	{
		BeginTurn();
		return;
	}
	MoveAction = NewObject<UStageMoveAction>(this);
	const auto* Movement = Owner->FindComponentByClass<UPawnMovementComponent>();
	const float Speed = Movement ? FMath::Max(1.0f, Movement->GetMaxSpeed()) : 150.0f;
	MoveAction->SetExecutionBudget(FVector::Distance(Owner->GetActorLocation(), Member->GetStageLocation()) / Speed * 2.0f + 10.0f);
	SetExecutionBudget(MoveAction->WatchdogSeconds + 360.0f / FMath::Max(TurnSpeed, 0.1f) + 10.0f);
	MoveAction->MoveToRequest(Member->GetStageLocation(), FMath::Max(0.0f, Member->StageLocationTolerance * 0.5f), FAIRequestID::InvalidRequest);
	MoveAction->OnFinished.AddUniqueDynamic(this, &ThisClass::HandleChildFinished);
	MoveAction->Start(Context);
}

void UReturnToStageMarkAction::BeginTurn()
{
	UStageMemberComponent* Member = Context.MemberComponent.Get();
	if (!Member || !IsValid(Member->GetOwner()))
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (Member->IsAtStageMark())
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}
	LookAction = NewObject<UStageLookToAction>(this);
	LookAction->SetupLookTo(Member->GetOwner()->GetActorLocation() + FRotator(0, Member->GetStageYaw(), 0).Vector() * 100, TurnSpeed, Member->StageYawTolerance);
	LookAction->SetExecutionBudget(360.0f / FMath::Max(TurnSpeed, 0.1f) + 10.0f);
	LookAction->OnFinished.AddUniqueDynamic(this, &ThisClass::HandleChildFinished);
	LookAction->Start(Context);
}

void UReturnToStageMarkAction::HandleChildFinished(UStageAction* Action, EStageActionResult Result)
{
	if (GetState() != EStageExecutionState::Running) return;
	if (Action != MoveAction && Action != LookAction) return;
	if (Result != EStageActionResult::Succeeded)
	{
		Finish(Result);
		return;
	}
	if (Action == MoveAction)
		BeginTurn();
	else
		Finish(Context.MemberComponent.IsValid() && Context.MemberComponent->IsAtStageMark() ? EStageActionResult::Succeeded : EStageActionResult::Failed);
}

void UReturnToStageMarkAction::Cleanup_Implementation()
{
	for (UStageAction* Child : {static_cast<UStageAction*>(MoveAction.Get()), static_cast<UStageAction*>(LookAction.Get())})
		if (Child)
		{
			Child->OnFinished.RemoveDynamic(this, &ThisClass::HandleChildFinished);
			Child->ForceAbort(FText::FromString(TEXT("Recovery parent stopped")));
		}
}
