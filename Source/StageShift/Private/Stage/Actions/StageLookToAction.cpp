// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Actions/StageLookToAction.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"

void UStageLookToAction::SetupLookTo(const FVector& InLookToLoc, const float& InTurnSpeed, const float& InAcceptanceAngleDeg)
{
	LookToLoc = InLookToLoc;
	TurnSpeed = InTurnSpeed;
	AcceptanceDotThreshold = FMath::Cos(FMath::DegreesToRadians(InAcceptanceAngleDeg));
}

void UStageLookToAction::OnStart_Implementation()
{
	const auto Owner = Context.MemberComponent.Get()->GetOwner();
	if (!Owner || !GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	if (GetIsSucceeded(LookToLoc))
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

bool UStageLookToAction::GetIsSucceeded(const FVector& InLookToLoc) const
{
	const auto Owner = Context.MemberComponent.Get()->GetOwner();
	if (!Owner) return false;

	const FVector OwnerDir = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector LookAtDir = (InLookToLoc - Owner->GetActorLocation()).GetSafeNormal2D();
	const auto DotProduct = FVector::DotProduct(OwnerDir, LookAtDir);

	return DotProduct >= AcceptanceDotThreshold;
}

void UStageLookToAction::TurnAtLoc(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld() || TickType != LEVELTICK_All || DeltaSeconds <= 0.f) return;

	const auto Owner = Context.MemberComponent.Get()->GetOwner();
	if (!Owner)
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	const FRotator ActorRotation = Owner->GetActorRotation();
	const FRotator LookAtRotation = (LookToLoc - Owner->GetActorLocation()).GetSafeNormal2D().Rotation();
	const FRotator NewRotation = FMath::RInterpConstantTo(ActorRotation, LookAtRotation, DeltaSeconds, TurnSpeed);

	Owner->SetActorRotation(NewRotation);

	if (GetIsSucceeded(LookToLoc))
	{
		Finish(EStageActionResult::Succeeded);
	}
}
