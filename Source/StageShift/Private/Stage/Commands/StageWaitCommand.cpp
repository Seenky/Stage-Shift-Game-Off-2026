// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/StageWaitCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/StageWaitAction.h"

void UStageWaitCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
    Super::ValidateDefinition(Data, Path, Errors);
    if (!FMath::IsFinite(WaitTime) || WaitTime < 0) Errors.Add(Path + TEXT(" / WaitTime must be finite and nonnegative"));
}

float UStageWaitCommand::GetNeededTime_Implementation() const
{
	return WaitTime;
}

UStageAction* UStageWaitCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.Get()) return nullptr;
	
	UStageWaitAction* WaitAction = NewObject<UStageWaitAction>(InContext.MemberComponent.Get());
	if (!WaitAction) return nullptr;
	
	WaitAction->SetupWait(CurrentTime);
	
	return WaitAction;
}

void UStageWaitCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
	if (const auto WaitAction = Cast<UStageWaitAction>(Action))
	{
		CurrentTime = WaitAction->GetWaitTime();
	}
}

void UStageWaitCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	CurrentTime = WaitTime;
}
