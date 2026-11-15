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
		!ResolveTarget(InContext)) return nullptr;
	
	UStageLookToAction* LookToAction = NewObject<UStageLookToAction>(InContext.MemberComponent.Get());
	if (!LookToAction) return nullptr;
	
	LookToAction->SetupLookTo(ResolveTarget(InContext)->GetActorLocation(), TurnSpeed, AcceptanceAngleDeg);
	
	return LookToAction;
}

bool UStageLookToCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() ||
		!ResolveTarget(InContext)) return 0.0f;
	
	const auto OwnerDir = InContext.MemberComponent.Get()->GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	const auto LookToDir = (ResolveTarget(InContext)->GetActorLocation() -
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

#include "Stage/Core/StageManager.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Engine/World.h"
#include "UObject/Package.h"

AActor* UStageLookToCommand::ResolveTarget(const FStageExecutionContext& InContext) const
{
    if (!TargetId.IsNone()) return InContext.Manager.IsValid() ? InContext.Manager->GetTarget(TargetId) : nullptr;
    FSoftObjectPath Path = LookToActor.ToSoftObjectPath();
    UWorld* World = InContext.MemberComponent.IsValid() ? InContext.MemberComponent->GetWorld() : nullptr;
    if (World && World->WorldType == EWorldType::PIE) Path.FixupForPIE(World->GetPackage()->GetPIEInstanceID());
    AActor* Actor = Cast<AActor>(Path.ResolveObject());
    return IsValid(Actor) && Actor->GetWorld() == World ? Actor : nullptr;
}
void UStageLookToCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
    Super::ValidateDefinition(Data, Path, Errors);
    if (!TargetId.IsNone() && !Data->TargetIds.Contains(TargetId)) Errors.Add(Path + TEXT(" / unknown TargetId: ") + TargetId.ToString());
    if (TargetId.IsNone() && LookToActor.IsNull()) Errors.Add(Path + TEXT(" / target is required"));
    if (!FMath::IsFinite(TurnSpeed) || TurnSpeed <= 0 || !FMath::IsFinite(AcceptanceAngleDeg) || AcceptanceAngleDeg < 0 || AcceptanceAngleDeg > 180) Errors.Add(Path + TEXT(" / invalid TurnSpeed or AcceptanceAngleDeg"));
}
