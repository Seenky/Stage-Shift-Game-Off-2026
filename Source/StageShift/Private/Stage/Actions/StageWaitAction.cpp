// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/StageWaitAction.h"

void UStageWaitAction::SetupWait(const float& InTime)
{
	WaitTime = InTime;
}

void UStageWaitAction::OnStart_Implementation()
{
	if (!GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	if (WaitTime <= 0.f)
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}
	
	GetWorld()->GetTimerManager().SetTimer(
		WaitTimerHandle, 
		this, 
		&ThisClass::FinishTimer, 
		WaitTime);
}

void UStageWaitAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	
	WaitTime = GetWorld()->GetTimerManager().GetTimerRemaining(WaitTimerHandle);
	if (Mode == EStageActionStopMode::StopAction)
	{
		GetWorld()->GetTimerManager().ClearTimer(WaitTimerHandle);
		Finish(EStageActionResult::Interrupted);
	}
}

void UStageWaitAction::Cleanup_Implementation()
{
	if (!GetWorld()) return;
	
	GetWorld()->GetTimerManager().ClearTimer(WaitTimerHandle);
}

void UStageWaitAction::FinishTimer()
{
	Finish(EStageActionResult::Succeeded);
}
