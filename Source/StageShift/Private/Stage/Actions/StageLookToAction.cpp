// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/StageLookToAction.h"

#include "Components/Stage/StageMemberComponent.h"

namespace 
{
	constexpr float GetTurnDeltaTime() { return 1.f / 30.f; }
}

void UStageLookToAction::SetupLookTo(
	const FVector& InLookToLoc, 
	const float& InTurnSpeed,
	const float& InAcceptanceAngleDeg)
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
	
	GetWorld()->GetTimerManager().SetTimer(
		LookTimerHandle, 
		this, 
		&ThisClass::TurnAtLoc, 
		GetTurnDeltaTime(),
		true);
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
		GetWorld()->GetTimerManager().ClearTimer(LookTimerHandle);
		Finish(EStageActionResult::Interrupted);
		return;
	}
}

void UStageLookToAction::Cleanup_Implementation()
{
	if (!GetWorld()) return;
	
	GetWorld()->GetTimerManager().ClearTimer(LookTimerHandle);
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

void UStageLookToAction::TurnAtLoc()
{
	const auto Owner = Context.MemberComponent.Get()->GetOwner();
	if (!Owner)
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	const FRotator ActorRotation = Owner->GetActorRotation();
	const FRotator LookAtRotation = (LookToLoc - Owner->GetActorLocation()).GetSafeNormal2D().Rotation();
	const FRotator NewRotation = FMath::RInterpConstantTo(
		ActorRotation, 
		LookAtRotation,
		GetTurnDeltaTime(), 
		TurnSpeed);
	
	Owner->SetActorRotation(NewRotation);
	
	if (GetIsSucceeded(LookToLoc))
	{
		Finish(EStageActionResult::Succeeded);
	}
}
