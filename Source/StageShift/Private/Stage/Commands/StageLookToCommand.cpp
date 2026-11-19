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
	UStageMemberComponent* Member = InContext.MemberComponent.Get();
	const AActor* Target = ResolveTarget(InContext, TargetId);
	if (!Member || !Member->GetOwner() || !Target) return nullptr;

	auto* LookToAction = NewObject<UStageLookToAction>(Member);
	LookToAction->SetupLookTo(GetLookTarget(*Target), TurnSpeed, AcceptanceAngleDeg, bUseTargetDirection);
	return LookToAction;
}

FVector UStageLookToCommand::GetLookTarget(const AActor& Target) const
{
	return bUseTargetDirection ? FRotator(0.0, Target.GetActorRotation().Yaw, 0.0).Vector() : Target.GetActorLocation();
}

bool UStageLookToCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const UStageMemberComponent* Member = InContext.MemberComponent.Get();
	const AActor* Owner = Member ? Member->GetOwner() : nullptr;
	const AActor* Target = ResolveTarget(InContext, TargetId);
	if (!Owner || !Target) return false;

	const FVector OwnerDir = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector LookTarget = GetLookTarget(*Target);
	const FVector LookToDir = (bUseTargetDirection ? LookTarget : LookTarget - Owner->GetActorLocation()).GetSafeNormal2D();

	const float DotProduct = FVector::DotProduct(OwnerDir, LookToDir);

	const float AcceptanceRad = FMath::Cos(FMath::DegreesToRadians(AcceptanceAngleDeg));
	return DotProduct >= AcceptanceRad;
}

void UStageLookToCommand::ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime)
{
	Super::ResetRuntime(InContext, InRoot, InPlannedStartTime, InPlannedEndTime);
	OnFinished.AddUniqueDynamic(this, &ThisClass::HandleCommandFinished);
}

void UStageLookToCommand::HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	if (Result == EStageCommandResult::Succeeded)
		if (UStageMemberComponent* Member = Context.MemberComponent.Get()) Member->RememberStageYaw(Command);
}

#include "Stage/Core/Data/StageDataAsset.h"

void UStageLookToCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (!TargetId.IsNone() && !Data->TargetIds.Contains(TargetId)) Errors.Add(Path + TEXT(" / unknown TargetId: ") + TargetId.ToString());
	if (TargetId.IsNone()) Errors.Add(Path + TEXT(" / TargetId is required"));
	if (!FMath::IsFinite(TurnSpeed) || TurnSpeed <= 0 || !FMath::IsFinite(AcceptanceAngleDeg) || AcceptanceAngleDeg < 0 || AcceptanceAngleDeg > 180)
		Errors.Add(Path + TEXT(" / invalid TurnSpeed or AcceptanceAngleDeg"));
}

void UStageLookToCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	if (!ResolveTarget(InContext, TargetId)) Errors.Add(Path + TEXT(" / Look target is unloaded or belongs to another world"));
}
