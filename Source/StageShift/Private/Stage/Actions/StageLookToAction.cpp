// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Actions/StageLookToAction.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"

void UStageLookToAction::SetupLookTo(const FVector& InLookTarget, const float& InTurnSpeed, const float& InAcceptanceAngleDeg, bool bInUseTargetDirection)
{
	LookTarget = InLookTarget;
	bUseTargetDirection = bInUseTargetDirection;
	TurnSpeed = InTurnSpeed;
	AcceptanceDotThreshold = FMath::Cos(FMath::DegreesToRadians(InAcceptanceAngleDeg));
}

void UStageLookToAction::OnStart_Implementation()
{
	const UStageMemberComponent* Member = Context.MemberComponent.Get();
	const AActor* Owner = Member ? Member->GetOwner() : nullptr;
	if (!Owner || !GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	if (IsFacingTarget(*Owner))
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}

	LookTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::TurnAtLoc);
}

void UStageLookToAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	if (Mode == EStageActionStopMode::StopAction)
	{
		Finish(EStageActionResult::Interrupted);
		return;
	}
}

void UStageLookToAction::Cleanup_Implementation()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(LookTickHandle);
	LookTickHandle.Reset();
}

EStageActionStopMode UStageLookToAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	return Reason == EStageActionStopReason::Cancel ? EStageActionStopMode::FinishAction : EStageActionStopMode::StopAction;
}

FVector UStageLookToAction::GetLookDirection(const AActor& Owner) const
{
	return (bUseTargetDirection ? LookTarget : LookTarget - Owner.GetActorLocation()).GetSafeNormal2D();
}

bool UStageLookToAction::IsFacingTarget(const AActor& Owner) const
{
	const FVector OwnerDir = Owner.GetActorForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(OwnerDir, GetLookDirection(Owner)) >= AcceptanceDotThreshold;
}

void UStageLookToAction::TurnAtLoc(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld() || TickType != LEVELTICK_All || DeltaSeconds <= 0.f) return;

	const UStageMemberComponent* Member = Context.MemberComponent.Get();
	AActor* Owner = Member ? Member->GetOwner() : nullptr;
	if (!Owner)
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	const FRotator ActorRotation = Owner->GetActorRotation();
	const FRotator LookAtRotation = GetLookDirection(*Owner).Rotation();
	const FRotator NewRotation = FMath::RInterpConstantTo(ActorRotation, LookAtRotation, DeltaSeconds, TurnSpeed);

	Owner->SetActorRotation(NewRotation);

	if (IsFacingTarget(*Owner))
	{
		Finish(EStageActionResult::Succeeded);
	}
}
