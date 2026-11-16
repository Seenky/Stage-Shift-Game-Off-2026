// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Actions/ReturnToStageMarkAction.h"
#include "Stage/Actions/StageMoveAction.h"
#include "Stage/Actions/StageLookToAction.h"
#include "Components/Stage/StageMemberComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Stage/Core/StageManager.h"

void UReturnToStageMarkAction::ValidateRecoveryTemplate(const FString& Path, TArray<FString>& Errors) const
{
	if (!FMath::IsFinite(TurnSpeed) || TurnSpeed <= 0) Errors.Add(Path + TEXT(" / TurnSpeed must be positive and finite"));
	if (!FMath::IsFinite(WatchdogSeconds) || WatchdogSeconds <= 0) Errors.Add(Path + TEXT(" / recovery watchdog must be positive and finite"));
}

void UReturnToStageMarkAction::LogRecovery(const FString& Details) const
{
	const UStageMemberComponent* Member = Context.MemberComponent.Get();
	const AActor* Owner = Member ? Member->GetOwner() : nullptr;
	FString Message = FString::Printf(TEXT("ReturnToStageMarkAction / Owner[%s] / %s"), *GetNameSafe(Owner), *Details);
	if (IsValid(Owner))
	{
		const FVector Location = Owner->GetActorLocation();
		const FVector Mark = Member->GetStageLocation();
		const float Yaw = Owner->GetActorRotation().Yaw;
		Message += FString::Printf(
		    TEXT(" / Location=%s, Mark=%s, Distance=%.2f cm, DistanceXY=%.2f cm, DeltaZ=%.2f cm, LocationTolerance=%.2f cm, CollisionRadius=%.2f cm, Yaw=%.2f deg, StageYaw=%.2f deg, YawError=%.2f deg, YawTolerance=%.2f deg, AtLocation=%d, AtYaw=%d, PhysicallyBlocked=%d"),
		    *Location.ToString(), *Mark.ToString(), FVector::Distance(Location, Mark), FVector::Dist2D(Location, Mark), Location.Z - Mark.Z,
		    Member->StageLocationTolerance, Owner->GetSimpleCollisionRadius(), Yaw, Member->GetStageYaw(),
		    FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Member->GetStageYaw())), Member->StageYawTolerance,
		    Member->IsAtStageMark(true, false), Member->IsAtStageMark(false, true), Member->IsPhysicallyBlocked());
	}
	else Message += TEXT(" / Missing valid Member or owner");
	if (Context.Manager.IsValid()) Context.Manager->AddDiagnostic(Message);
	else UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
}

void UReturnToStageMarkAction::OnStart_Implementation()
{
	UStageMemberComponent* Member = Context.MemberComponent.Get();
	AActor* Owner = Member ? Member->GetOwner() : nullptr;
	if (!IsValid(Owner) || Member->IsPhysicallyBlocked())
	{
		LogRecovery(IsValid(Owner) ? TEXT("Start failed: participant is physically blocked") : TEXT("Start failed: invalid Member or owner"));
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
	const float AcceptanceRadius = FMath::Max(0.0f, Member->StageLocationTolerance * 0.5f);
	// Match the stage mark check: measure from the actor center, without adding the capsule radius.
	MoveAction->MoveToRequest(Member->GetStageLocation(), AcceptanceRadius, FAIRequestID::InvalidRequest, false);
	MoveAction->OnFinished.AddUniqueDynamic(this, &ThisClass::HandleChildFinished);
	LogRecovery(FString::Printf(TEXT("Move started: AcceptanceRadius=%.2f cm"), AcceptanceRadius));
	MoveAction->Start(Context);
}

void UReturnToStageMarkAction::BeginTurn()
{
	UStageMemberComponent* Member = Context.MemberComponent.Get();
	if (!Member || !IsValid(Member->GetOwner()))
	{
		LogRecovery(TEXT("Turn failed: invalid Member or owner"));
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
	LogRecovery(FString::Printf(TEXT("Turn started: TurnSpeed=%.2f deg/s"), TurnSpeed));
	LookAction->Start(Context);
}

void UReturnToStageMarkAction::HandleChildFinished(UStageAction* Action, EStageActionResult Result)
{
	if (GetState() != EStageExecutionState::Running) return;
	if (Action != MoveAction && Action != LookAction) return;
	LogRecovery(FString::Printf(TEXT("%s finished: Result=%s"), Action == MoveAction ? TEXT("Move") : TEXT("Turn"), *UEnum::GetValueAsString(Result)));
	if (Result != EStageActionResult::Succeeded)
	{
		Finish(Result);
		return;
	}
	if (Action == MoveAction)
		BeginTurn();
	else
	{
		const bool bAtStageMark = Context.MemberComponent.IsValid() && Context.MemberComponent->IsAtStageMark();
		if (!bAtStageMark) LogRecovery(TEXT("Final stage mark check failed after successful turn"));
		Finish(bAtStageMark ? EStageActionResult::Succeeded : EStageActionResult::Failed);
	}
}

void UReturnToStageMarkAction::Cleanup_Implementation()
{
	LogRecovery(FString::Printf(TEXT("Recovery finished: Result=%s"), *UEnum::GetValueAsString(GetResult())));
	for (UStageAction* Child : {static_cast<UStageAction*>(MoveAction.Get()), static_cast<UStageAction*>(LookAction.Get())})
		if (Child)
		{
			Child->OnFinished.RemoveDynamic(this, &ThisClass::HandleChildFinished);
			Child->ForceAbort(FText::FromString(TEXT("Recovery parent stopped")));
		}
}
