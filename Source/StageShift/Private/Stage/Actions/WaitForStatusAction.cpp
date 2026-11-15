#include "Stage/Actions/WaitForStatusAction.h"

#include "StatusesComponent.h"

void UWaitForStatusAction::SetupWaitForStatus(UStatusesComponent* InTargetStatuses, const FGameplayTag& InStatusTag)
{
	TargetStatuses = InTargetStatuses;
	StatusTag = InStatusTag;
}

void UWaitForStatusAction::OnStart_Implementation()
{
	if (!TargetStatuses.IsValid() || !StatusTag.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	TargetStatuses->OnRemoveStatuses.AddUniqueDynamic(this, &ThisClass::HandleStatusesRemoved);
	if (!TargetStatuses->GetStatuses().HasTag(StatusTag)) Finish(EStageActionResult::Succeeded);
}

void UWaitForStatusAction::HandleStatusesRemoved(FGameplayTagContainer RemovedStatuses)
{
	if (TargetStatuses.IsValid() && !TargetStatuses->GetStatuses().HasTag(StatusTag)) Finish(EStageActionResult::Succeeded);
}

void UWaitForStatusAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (Mode == EStageActionStopMode::StopAction) Finish(EStageActionResult::Interrupted);
}

void UWaitForStatusAction::Cleanup_Implementation()
{
	if (TargetStatuses.IsValid()) TargetStatuses->OnRemoveStatuses.RemoveDynamic(this, &ThisClass::HandleStatusesRemoved);
	TargetStatuses.Reset();
}
