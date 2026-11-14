// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/StageLookToCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/StageLookToAction.h"

UStageLookToCommand::UStageLookToCommand()
{
	bRequireStageLocation = true;
	bRequireStageYaw = false;
}

float UStageLookToCommand::GetNeededTime_Implementation() const
{
	return 360.f / TurnSpeed;
}

UStageAction* UStageLookToCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() ||
		!LookToActor) return nullptr;
	
	UStageLookToAction* LookToAction = NewObject<UStageLookToAction>(InContext.MemberComponent.Get());
	if (!LookToAction) return nullptr;
	
	LookToAction->SetupLookTo(LookToActor->GetActorLocation(), TurnSpeed, AcceptanceAngleDeg);
	
	return LookToAction;
}

bool UStageLookToCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() ||
		!LookToActor) return 0.0f;
	
	const auto OwnerDir = InContext.MemberComponent.Get()->GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	const auto LookToDir = (LookToActor->GetActorLocation() - 
		InContext.MemberComponent.Get()->GetOwner()->GetActorLocation()).GetSafeNormal2D();
	
	const float DotProduct = FVector::DotProduct(OwnerDir, LookToDir);
	
	const float AcceptanceRad = FMath::Cos(FMath::DegreesToRadians(AcceptanceAngleDeg));
	return DotProduct >= AcceptanceRad;
}

void UStageLookToCommand::ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot,
	float InPlannedStartTime, float InPlannedEndTime)
{
	Super::ResetRuntime(InContext, InRoot, InPlannedStartTime, InPlannedEndTime);
	OnFinished.AddUniqueDynamic(this, &ThisClass::HandleCommandFinished);
}

void UStageLookToCommand::HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	if (Result == EStageCommandResult::Succeeded)
		if (UStageMemberComponent* Member = Context.MemberComponent.Get()) Member->RememberStageYaw(Command);
}
