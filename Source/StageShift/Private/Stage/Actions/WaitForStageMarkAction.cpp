#include "Stage/Actions/WaitForStageMarkAction.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"

void UWaitForStageMarkAction::SetupWaitForStageMark(UStageMemberComponent* InTargetMember)
{
	TargetMember = InTargetMember;
}

void UWaitForStageMarkAction::OnStart_Implementation()
{
	if (!TargetMember.IsValid() || TargetMember->GetWorld() != GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	CheckTarget();
	if (GetState() == EStageExecutionState::Finished) return;
	WaitTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &ThisClass::HandleWorldTick);
}

void UWaitForStageMarkAction::CheckTarget()
{
	if (!TargetMember.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (!TargetMember->IsPhysicallyBlocked() && TargetMember->IsAtStageMark()) Finish(EStageActionResult::Succeeded);
}

void UWaitForStageMarkAction::HandleWorldTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld() || TickType != LEVELTICK_All || DeltaSeconds <= 0.f) return;
	CheckTarget();
}

void UWaitForStageMarkAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (Mode == EStageActionStopMode::StopAction) Finish(EStageActionResult::Interrupted);
}

void UWaitForStageMarkAction::Cleanup_Implementation()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(WaitTickHandle);
	WaitTickHandle.Reset();
	TargetMember.Reset();
}
