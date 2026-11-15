#include "Stage/Commands/WaitForStatusCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/WaitForStatusAction.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"
#include "StatusesComponent.h"

void UWaitForStatusCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (TargetRoleId.IsNone() || !Data || !Data->GetRoleOptions().Contains(TargetRoleId)) Errors.Add(Path + TEXT(" / TargetRoleId must be a declared role"));
	if (!StatusTag.IsValid()) Errors.Add(Path + TEXT(" / StatusTag is required"));
}

bool UWaitForStatusCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const AActor* Actor = InContext.Manager.IsValid() ? InContext.Manager->GetParticipant(TargetRoleId) : nullptr;
	const auto* Statuses = IsValid(Actor) ? Actor->FindComponentByClass<UStatusesComponent>() : nullptr;
	return IsValid(Statuses) && StatusTag.IsValid() && !Statuses->GetStatuses().HasTag(StatusTag);
}

UStageAction* UWaitForStatusCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid() || !StatusTag.IsValid()) return nullptr;
	AActor* Actor = InContext.Manager->GetParticipant(TargetRoleId);
	auto* Statuses = IsValid(Actor) ? Actor->FindComponentByClass<UStatusesComponent>() : nullptr;
	if (!IsValid(Statuses)) return nullptr;

	auto* Action = NewObject<UWaitForStatusAction>(InContext.MemberComponent.Get());
	Action->SetupWaitForStatus(Statuses, StatusTag);
	// The stage/finale deadline controls this wait, rather than the default 30-second watchdog.
	if (const UStageDataAsset* Stage = InContext.Manager->GetActiveStage()) Action->SetExecutionBudget(Stage->MainDuration + Stage->FinaleBudget);
	return Action;
}
